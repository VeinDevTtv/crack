#include <iostream>
#include <string>
#include <vector>
#include "CLI/CLI.hpp"

int main(int argc, char** argv) {
    CLI::App app{"ContextCracker: Semantic context-aware extraction tool"};

    std::string filePath;
    app.add_option("-f,--file", filePath, "Path to the input file")
       ->required()
       ->check(CLI::ExistingFile);

    std::string query;
    app.add_option("-q,--query", query, "Search query")->required();

    // Placeholder for other options (search mode, context lines, etc.)
    // Example:
    // int contextLines = 5;
    // app.add_option("-C,--context", contextLines, "Number of context lines to show", true);

    // std::string searchMode = "keyword";
    // app.add_option("-m,--mode", searchMode, "Search mode (keyword, semantic, fuzzy)")->default_val("keyword");


    CLI11_PARSE(app, argc, argv);

    std::cout << "Processing file: " << filePath << std::endl;
    std::cout << "Search query: '" << query << "'" << std::endl;

    // --- TODO: Implement file reading, chunking, and searching ---

    return 0;
} 