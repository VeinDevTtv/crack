cmake_minimum_required(VERSION 3.16)
project(ContextCracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- Dependencies ---

# Fetch CLI11 for argument parsing
include(FetchContent)
FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG        v2.4.2 # Or latest stable tag
)
FetchContent_MakeAvailable(CLI11)

# Fetch nlohmann/json for JSON output
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3 # Or latest stable tag
)
FetchContent_MakeAvailable(nlohmann_json)

# Fetch pybind11 for C++/Python interop
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.12.0 # Or latest stable tag
)
FetchContent_MakeAvailable(pybind11)

# --- Find Python --- 
# Hint: You might need to set -DPython_EXECUTABLE=/path/to/your/.venv/bin/python 
#       during cmake configuration if it doesn't find the venv python automatically.
find_package(Python REQUIRED COMPONENTS Interpreter Development)
message(STATUS "Found Python Interpreter: ${Python_EXECUTABLE}")
message(STATUS "Found Python Include Dirs: ${Python_INCLUDE_DIRS}")
message(STATUS "Found Python Libraries: ${Python_LIBRARIES}")

# --- Python Bridge Library --- 
# We will create python_bridge.cpp later
add_library(py_bridge python_bridge.cpp)
target_include_directories(py_bridge PUBLIC
    ${Python_INCLUDE_DIRS}
    ${pybind11_SOURCE_DIR}/include 
)
target_link_libraries(py_bridge PRIVATE pybind11::embed ${Python_LIBRARIES})

# --- Executable ---

add_executable(context_cracker main.cpp)

# Link dependencies
target_include_directories(context_cracker PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${cli11_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/include
    # Pybind headers might be needed if main.cpp uses pybind types directly,
    # but we'll try to encapsulate in the bridge first.
    # ${pybind11_SOURCE_DIR}/include 
)

# Link our bridge library and its dependencies to the executable
target_link_libraries(context_cracker PRIVATE 
    CLI11::CLI11 
    py_bridge # Link the bridge library
)

# --- Optional: Installation ---
# install(TARGETS context_cracker DESTINATION bin)
# install(DIRECTORY include/ DESTINATION include) # If you have headers to install

# --- Testing (Optional, requires CTest) ---
# enable_testing()
# add_test(NAME MyTest COMMAND context_cracker --file test.txt --query "hello")
# set_tests_properties(MyTest PROPERTIES WILL_FAIL true) # Example property 