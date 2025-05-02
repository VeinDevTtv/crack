#ifndef PYTHON_BRIDGE_H
#define PYTHON_BRIDGE_H

#include <vector>
#include <string>
#include <optional> // To represent potential Python errors

// Structure to hold similarity results from Python
struct SemanticResult {
    size_t chunkIndex; // Original index in the C++ chunk list
    double score;      // Similarity score
};

namespace PythonBridge {

    // Initialize the Python interpreter and import our module
    bool initialize(const std::string& scriptPath, const std::string& venvPath = ".venv");

    // Load the semantic model in Python
    bool load_model(const std::string& modelName = "all-MiniLM-L6-v2");

    // Call the Python function to find similar chunks
    std::optional<std::vector<SemanticResult>> find_similar(const std::string& query, const std::vector<std::string>& chunkContents);

    // Finalize the Python interpreter (optional, but good practice)
    void finalize();

} // namespace PythonBridge

#endif // PYTHON_BRIDGE_H 