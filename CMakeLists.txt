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
  GIT_TAG        v2.3.2 # Or latest stable tag
)
FetchContent_MakeAvailable(CLI11)

# --- Executable ---

add_executable(context_cracker main.cpp)

# Link dependencies
target_include_directories(context_cracker PUBLIC
  $<INSTALL_INTERFACE:include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> # If you create an include dir
  ${cli11_SOURCE_DIR}/include
)

target_link_libraries(context_cracker PRIVATE CLI11::CLI11)

# --- Optional: Installation ---
# install(TARGETS context_cracker DESTINATION bin)
# install(DIRECTORY include/ DESTINATION include) # If you have headers to install

# --- Testing (Optional, requires CTest) ---
# enable_testing()
# add_test(NAME MyTest COMMAND context_cracker --file test.txt --query "hello")
# set_tests_properties(MyTest PROPERTIES WILL_FAIL true) # Example property 