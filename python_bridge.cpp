// Placeholder for Python bridge implementation
#include "python_bridge.h"
#include <pybind11/embed.h> // Core pybind embed header
#include <pybind11/stl.h>    // For automatic C++ STL <-> Python conversions
#include <iostream>
#include <stdexcept>
#include <filesystem> // For path manipulation

namespace py = pybind11;

namespace PythonBridge {

    // Global variable to hold the Python module once imported
    py::module_ semantic_module;
    bool initialized = false;
    py::scoped_interpreter* guard_ptr = nullptr; // Using pointer for manual control matching header

    // Helper to print Python errors
    void handle_py_error() {
        try {
            py::error_already_set err;
            if (err) {
                std::cerr << "Python Error: " << err.what() << std::endl;
                // Optional: Print Python traceback
                // try {
                //     py::object traceback = py::module_::import("traceback");
                //     py::object formated_exception = traceback.attr("format_exception")(err.type(), err.value(), err.trace());
                //     std::cerr << py::str(formated_exception) << std::endl;
                // } catch (py::error_already_set &) { /* ignore */ }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error handling Python error: " << e.what() << std::endl;
        }
    }

    bool initialize(const std::string& scriptPath, const std::string& venvPath) {
        if (initialized) {
            std::cerr << "Warning: Python Bridge already initialized." << std::endl;
            return true;
        }

        try {
            std::cout << "Initializing Python interpreter..." << std::endl;
            // Start the interpreter and keep it alive
            // py::scoped_interpreter guard{}; // Using manual init/finalize now
            py::initialize_interpreter();
            // guard_ptr = new py::scoped_interpreter(); // Scoped would finalize on destruction

            // --- Environment Setup (Crucial for finding modules/packages) ---
            // 1. Add venv site-packages to path (if venvPath is provided)
            if (!venvPath.empty()) {
                 // This path needs to be correct for your OS and Python version
                // Example for Windows Python 3.9+: .venv/Lib/site-packages
                // Example for Linux Python 3.9+: .venv/lib/python3.x/site-packages
                // We'll try a common structure
                std::filesystem::path venv_abs_path = std::filesystem::absolute(venvPath);
                #ifdef _WIN32
                    std::filesystem::path site_packages = venv_abs_path / "Lib" / "site-packages";
                #else
                    // This might need adjustment based on exact python minor version
                    // Trying a glob might be more robust if we knew Python version
                     std::filesystem::path site_packages;
                     // Crude check for typical linux structure
                     for (const auto& entry : std::filesystem::directory_iterator(venv_abs_path / "lib")) {
                         if (entry.is_directory() && entry.path().filename().string().find("python") != std::string::npos) {
                             site_packages = entry.path() / "site-packages";
                             break;
                         }
                     }
                #endif

                if (!site_packages.empty() && std::filesystem::exists(site_packages)){
                     py::module_ sys = py::module_::import("sys");
                     sys.attr("path").attr("insert")(0, site_packages.string());
                     std::cout << "Added to sys.path: " << site_packages.string() << std::endl;
                 } else {
                     std::cerr << "Warning: Could not find venv site-packages directory structure: " 
                               << site_packages.string() << std::endl;
                     std::cerr << "         Semantic search might fail to import dependencies." << std::endl;
                 }
            }

            // 2. Add the script's directory to sys.path
            std::filesystem::path script_dir = std::filesystem::absolute(scriptPath).parent_path();
            py::module_ sys = py::module_::import("sys");
            sys.attr("path").attr("insert")(0, script_dir.string());
            std::cout << "Added to sys.path: " << script_dir.string() << std::endl;

            // Extract the script name (without .py)
            std::string scriptName = std::filesystem::path(scriptPath).stem().string();
            
            std::cout << "Importing Python module: " << scriptName << "..." << std::endl;
            semantic_module = py::module_::import(scriptName.c_str());
            std::cout << "Python module imported successfully." << std::endl;

            initialized = true;
            return true;

        } catch (py::error_already_set &e) {
            handle_py_error();
            initialized = false;
            // if (guard_ptr) { delete guard_ptr; guard_ptr = nullptr; } // Clean up if needed
            if (py::is_initialized()) py::finalize_interpreter(); // cleanup
            return false;
        } catch (const std::exception &e) {
            std::cerr << "C++ Error during Python initialization: " << e.what() << std::endl;
            initialized = false;
             // if (guard_ptr) { delete guard_ptr; guard_ptr = nullptr; }
            if (py::is_initialized()) py::finalize_interpreter();
            return false;
        }
    }

    bool load_model(const std::string& modelName) {
        if (!initialized || !semantic_module) {
            std::cerr << "Error: Python Bridge not initialized or module not loaded." << std::endl;
            return false;
        }
        try {
            std::cout << "Requesting Python to load model: " << modelName << "..." << std::endl;
            py::object result = semantic_module.attr("initialize_model")(modelName);
            return result.cast<bool>();
        } catch (py::error_already_set &e) {
            handle_py_error();
            return false;
        } catch (const std::exception &e) {
             std::cerr << "C++ Error during model loading: " << e.what() << std::endl;
            return false;
        }
    }

    std::optional<std::vector<SemanticResult>> find_similar(const std::string& query, const std::vector<std::string>& chunkContents) {
        if (!initialized || !semantic_module) {
            std::cerr << "Error: Python Bridge not initialized or module not loaded." << std::endl;
            return std::nullopt;
        }
         if (chunkContents.empty()) {
            return std::vector<SemanticResult>{}; // Return empty results for empty input
        }

        try {
            std::cout << "Calling Python find_similar_chunks..." << std::endl;
            // Convert C++ vector<string> to Python list[str]
            // pybind11/stl.h handles this automatically when passing as an argument
            py::object pyResult = semantic_module.attr("find_similar_chunks")(query, chunkContents);

            if (pyResult.is_none()) {
                std::cerr << "Python function find_similar_chunks returned None." << std::endl;
                return std::nullopt;
            }

            std::vector<SemanticResult> cppResults;
            // The Python function returns a list of dictionaries [{'index': int, 'score': float}]
            py::list pyList = pyResult.cast<py::list>();
            
            cppResults.reserve(pyList.size());
            for (py::handle item : pyList) {
                py::dict pyDict = item.cast<py::dict>();
                cppResults.push_back({pyDict["index"].cast<size_t>(), 
                                      pyDict["score"].cast<double>()});
            }
            std::cout << "Received " << cppResults.size() << " results from Python." << std::endl;
            return cppResults;

        } catch (py::error_already_set &e) {
            handle_py_error();
            return std::nullopt;
        } catch (const std::exception &e) {
             std::cerr << "C++ Error during semantic search call: " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    void finalize() {
        if (initialized) {
            std::cout << "Finalizing Python interpreter..." << std::endl;
            // Module references should be cleaned up automatically by pybind11
            // semantic_module = py::module_(); // Reset module object if needed
            
            // delete guard_ptr; // If using scoped_interpreter pointer
            // guard_ptr = nullptr;
            
            py::finalize_interpreter();
            initialized = false;
        }
    }

} // namespace PythonBridge 