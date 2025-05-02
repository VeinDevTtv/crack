#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm> // Needed for std::all_of
#include <cctype>    // Needed for std::isspace
#include <set>       // Needed for std::set to store unique printed chunk indices
#include <numeric>   // Needed for std::iota
#include <limits>    // Needed for std::numeric_limits
#include <nlohmann/json.hpp> // JSON library
#include "CLI/CLI.hpp"
#include "python_bridge.h" // Include the Python bridge header

#ifdef _WIN32
#include <io.h> // For _isatty
#include <windows.h> // For enabling virtual terminal processing
#else
#include <unistd.h> // For isatty
#endif

// Use nlohmann::json
using json = nlohmann::json;

// --- ANSI Color Codes --- 
namespace Color {
    const std::string RESET = "\033[0m";
    const std::string BOLD = "\033[1m";
    const std::string DIM = "\033[2m";
    const std::string UNDERLINE = "\033[4m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string GRAY = "\033[90m"; 
    // Add more as needed
}

// Function to check if stdout is a TTY
bool is_stdout_tty() {
#ifdef _WIN32
    return _isatty(_fileno(stdout));
#else
    return isatty(fileno(stdout));
#endif
}

// Function to enable virtual terminal processing on Windows
void enable_windows_vt_processing() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode)) return;
#endif
}

// Structure to represent a text chunk
struct Chunk {
    size_t startLine;       // 1-based starting line number in the original file
    std::vector<std::string> lines; // Content of the chunk

    // Helper to check if the chunk is empty or whitespace
    bool isEmptyOrWhitespace() const {
        if (lines.empty()) return true;
        for(const auto& line : lines) {
            if (std::find_if_not(line.begin(), line.end(), ::isspace) != line.end()) {
                return false; // Found a non-whitespace character
            }
        }
        return true; // All lines are empty or whitespace
    }

    // Helper to join lines into a single string (optional)
    std::string contentAsString(const std::string& separator = "\n") const {
        std::string result;
        for (size_t i = 0; i < lines.size(); ++i) {
            result += lines[i];
            if (i < lines.size() - 1) {
                result += separator;
            }
        }
        return result;
    }

    // Function to convert Chunk to JSON
    void to_json(json& j, const Chunk& c) {
        j = json{{"startLine", c.startLine}, {"lines", c.lines}};
    }
};

// Function to check if a line is blank (empty or only whitespace)
bool isLineBlank(const std::string& line) {
    return std::all_of(line.begin(), line.end(), ::isspace);
}

// Function to chunk file content by paragraphs (blank lines)
std::vector<Chunk> chunkByParagraphs(const std::vector<std::string>& fileContent) {
    std::vector<Chunk> chunks;
    Chunk currentChunk;
    currentChunk.startLine = 1;

    for (size_t i = 0; i < fileContent.size(); ++i) {
        const std::string& line = fileContent[i];
        size_t currentLineNum = i + 1;

        if (isLineBlank(line)) {
            // End of a paragraph
            if (!currentChunk.isEmptyOrWhitespace()) {
                chunks.push_back(currentChunk);
            }
            // Start a new chunk after the blank line
            currentChunk = Chunk{};
            currentChunk.startLine = currentLineNum + 1; 
        } else {
             // If the current chunk is starting and hasn't had its start line set properly yet
            if (currentChunk.lines.empty()) {
                 currentChunk.startLine = currentLineNum;
            }
            currentChunk.lines.push_back(line);
        }
    }

    // Add the last chunk if it's not empty
    if (!currentChunk.isEmptyOrWhitespace()) {
        chunks.push_back(currentChunk);
    }

    return chunks;
}

// Function to chunk file content by simple brace matching
std::vector<Chunk> chunkByBraces(const std::vector<std::string>& fileContent) {
    std::vector<Chunk> chunks;
    Chunk currentChunk;
    int braceLevel = 0;
    bool inChunk = false;

    for (size_t i = 0; i < fileContent.size(); ++i) {
        const std::string& line = fileContent[i];
        size_t currentLineNum = i + 1;

        int lineBraceChange = 0;
        for (char c : line) {
            if (c == '{') {
                lineBraceChange++;
            } else if (c == '}') {
                lineBraceChange--;
            }
        }

        if (!inChunk && lineBraceChange > 0 && braceLevel == 0) {
            // Start of a potential top-level block
            currentChunk = Chunk{};
            currentChunk.startLine = currentLineNum;
            currentChunk.lines.push_back(line);
            braceLevel += lineBraceChange;
            inChunk = true;
        } else if (inChunk) {
            currentChunk.lines.push_back(line);
            braceLevel += lineBraceChange;
            // Check if the chunk ends (brace level returns to 0)
            // We check <= 0 because a line might contain closing braces that 
            // bring the level to or below zero.
            if (braceLevel <= 0) {
                chunks.push_back(currentChunk);
                inChunk = false;
                braceLevel = 0; // Reset for the next potential chunk
            }
        } else {
            // Line is outside any brace block, treat as its own small chunk? Or ignore?
            // For now, let's treat non-empty, non-brace lines outside blocks as single-line chunks.
            if (!isLineBlank(line) && line.find_first_of("{}") == std::string::npos) {
                 chunks.push_back({{currentLineNum}, {line}});
            }
        }
    }

    // If the file ends while still inside a chunk (unbalanced braces?)
    if (inChunk && !currentChunk.isEmptyOrWhitespace()) {
        chunks.push_back(currentChunk);
    }

    return chunks;
}

// Function to read the entire file into a vector of strings (lines)
std::vector<std::string> readFileLines(const std::string& filePath) {
    std::ifstream fileStream(filePath);
    if (!fileStream.is_open()) {
        throw std::runtime_error("Error opening file: " + filePath);
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(fileStream, line)) {
        lines.push_back(line);
    }

    if (fileStream.bad()) {
         throw std::runtime_error("Error reading file: " + filePath);
    }

    return lines;
}

// Function to calculate Levenshtein distance between two strings
// Source: https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C++
size_t levenshteinDistance(const std::string &s1, const std::string &s2) {
    const size_t len1 = s1.size(), len2 = s2.size();
    std::vector<size_t> col(len2 + 1), prevCol(len2 + 1);

    std::iota(prevCol.begin(), prevCol.end(), 0);

    for (size_t i = 0; i < len1; i++) {
        col[0] = i + 1;
        for (size_t j = 0; j < len2; j++) {
            col[j + 1] = std::min({ prevCol[j + 1] + 1, col[j] + 1, prevCol[j] + (s1[i] == s2[j] ? 0 : 1) });
        }
        col.swap(prevCol);
    }
    return prevCol[len2];
}

int main(int argc, char** argv) {
    CLI::App app{"ContextCracker: Semantic context-aware extraction tool"};
    app.set_config("--config"); // Allow config file

    // --- Options --- 
    std::string filePath, query, searchMode = "keyword", chunkMode = "paragraph", outputFormat = "text";
    int contextChunks = 0, fuzzyTolerance = 2;
    bool ignoreComments = false;
    std::string semanticModel = "all-MiniLM-L6-v2";
    std::string pythonScript = "semantic_searcher.py"; // Default script name
    std::string venvDir = ".venv"; // Default venv directory

    app.add_option("-f,--file", filePath, "Path to the input file")
       ->required()
       ->check(CLI::ExistingFile);
    app.add_option("-q,--query", query, "Search query")->required();
    app.add_option("-C,--context", contextChunks, "Number of surrounding paragraphs (chunks) to show")
        ->default_val(0)
        ->check(CLI::NonNegativeNumber);
    app.add_option("-m,--mode", searchMode, "Search mode (keyword, fuzzy, semantic)") // Add semantic
        ->default_val("keyword")
        ->check(CLI::IsMember({"keyword", "fuzzy", "semantic"})); 
    app.add_option("--tolerance", fuzzyTolerance, "Maximum Levenshtein distance for fuzzy search")
        ->default_val(2)
        ->check(CLI::NonNegativeNumber);
    app.add_flag("--ignore-comments", ignoreComments, "Ignore lines starting with // or #");
    app.add_option("--chunk-mode", chunkMode, "Chunking strategy (paragraph, brace)")
        ->default_val("paragraph")
        ->check(CLI::IsMember({"paragraph", "brace"}));
    app.add_option("--output-format", outputFormat, "Output format (text, json, markdown)")
        ->default_val("text")
        ->check(CLI::IsMember({"text", "json", "markdown"}));
    app.add_option("--semantic-model", semanticModel, "Name of the Sentence Transformer model")
        ->default_val("all-MiniLM-L6-v2");
    app.add_option("--py-script", pythonScript, "Path to the Python semantic search script")
        ->default_val("semantic_searcher.py")
        ->check(CLI::ExistingFile);
    app.add_option("--venv-dir", venvDir, "Path to the Python virtual environment directory")
        ->default_val(".venv")
        ->check(CLI::ExistingDirectory);

    std::string colorMode = "auto";
    app.add_option("--color", colorMode, "When to use color output (always, auto, never)")
        ->default_val("auto")
        ->check(CLI::IsMember({"always", "auto", "never"}));

    CLI11_PARSE(app, argc, argv);

    // --- Determine color usage --- 
    bool useColor = false;
    if (colorMode == "always") {
        useColor = true;
    } else if (colorMode == "auto") {
        useColor = is_stdout_tty();
    }
    // Enable VT processing on Windows if colors are used
    if (useColor) {
        enable_windows_vt_processing();
    }

    // --- Initialize Python Bridge (if needed) --- 
    bool pythonInitialized = false;
    if (searchMode == "semantic") {
        pythonInitialized = PythonBridge::initialize(pythonScript, venvDir);
        if (!pythonInitialized) {
            std::cerr << "Failed to initialize Python bridge. Semantic search disabled." << std::endl;
            // Fallback or exit? For now, let's exit.
            return 1;
        }
        if (!PythonBridge::load_model(semanticModel)) {
             std::cerr << "Failed to load semantic model in Python." << std::endl;
             PythonBridge::finalize();
             return 1;
        }
    }

    // Lambda for conditional output (using Color namespace)
    auto printInfo = [&](const std::string& msg) {
        if (outputFormat == "text") {
            std::cout << msg << std::endl;
        }
    };

    // Print effective options
    printInfo("Processing file: " + filePath);
    printInfo("Search query: '" + query + "'");
    printInfo("Search mode: " + searchMode);
    if (searchMode == "fuzzy") {
        printInfo("Fuzzy tolerance: " + std::to_string(fuzzyTolerance));
    } else if (searchMode == "semantic") {
        printInfo("Semantic model: " + semanticModel);
    }
    printInfo("Context chunks: " + std::to_string(contextChunks));
    printInfo("Ignore comments: " + std::string(ignoreComments ? "Yes" : "No"));
    printInfo("Chunk mode: " + chunkMode);
    printInfo("Output format: " + outputFormat);
    printInfo("Color mode: " + colorMode + " (" + (useColor ? "Active" : "Inactive") + ")");

    // --- Main Logic --- 
    try {
        std::vector<std::string> fileContent = readFileLines(filePath);
        printInfo("Successfully read " + std::to_string(fileContent.size()) + " lines from file.");

        std::vector<Chunk> chunks = (chunkMode == "brace") ? 
                                      chunkByBraces(fileContent) : 
                                      chunkByParagraphs(fileContent);
        printInfo("Chunked content into " + std::to_string(chunks.size()) + " chunks.");

        printInfo("\n--- " + searchMode + " Search Results --- (" + query + ")");
        
        // Store results as {chunk_index, score}. Score meaning depends on mode.
        // Fuzzy: lower is better (distance). Semantic: higher is better (similarity).
        std::vector<std::pair<size_t, double>> results;

        // --- Perform Search --- 
        if (searchMode == "keyword" || searchMode == "fuzzy") {
            for (size_t i = 0; i < chunks.size(); ++i) {
                const auto& chunk = chunks[i];
                double bestMatchScore = (searchMode == "keyword") ? 0.0 : std::numeric_limits<double>::max();
                bool chunkMatchFound = false;
                for (const auto& line : chunk.lines) {
                    std::string processedLine = line;
                    size_t firstChar = processedLine.find_first_not_of(" \t");
                    if (ignoreComments && firstChar != std::string::npos && (processedLine.rfind("//", firstChar) == firstChar || processedLine.rfind("#", firstChar) == firstChar)) {
                        continue;
                    }
                    if (searchMode == "keyword") {
                        if (processedLine.find(query) != std::string::npos) {
                            bestMatchScore = 1.0; // Max score for keyword
                            chunkMatchFound = true;
                            break;
                        }
                    } else { // Fuzzy
                        size_t queryLen = query.length();
                        if (queryLen == 0) continue;
                        for (size_t j = 0; (j = processedLine.find_first_not_of(" \t\n\r", j)) != std::string::npos; ) {
                             size_t wordEnd = processedLine.find_first_of(" \t\n\r", j);
                             if (wordEnd == std::string::npos) wordEnd = processedLine.length();
                             std::string word = processedLine.substr(j, wordEnd - j);
                             size_t distance = levenshteinDistance(word, query);
                             if (distance <= (size_t)fuzzyTolerance) {
                                  if (static_cast<double>(distance) < bestMatchScore) {
                                     bestMatchScore = static_cast<double>(distance);
                                 }
                                 chunkMatchFound = true;
                             }
                             if (wordEnd == processedLine.length()) break;
                             j = wordEnd + 1;
                        }
                    }
                }
                if (chunkMatchFound) {
                    results.push_back({i, bestMatchScore});
                }
            }
        } else if (searchMode == "semantic") {
            if (!pythonInitialized) {
                 throw std::runtime_error("Semantic search requested but Python bridge failed to initialize.");
            }
            std::vector<std::string> chunkTexts;
            chunkTexts.reserve(chunks.size());
            for(const auto& chunk : chunks) {
                // Use contentAsString to pass the whole chunk text to Python model
                chunkTexts.push_back(chunk.contentAsString()); 
            }
            
            std::optional<std::vector<SemanticResult>> semanticResultsOpt = PythonBridge::find_similar(query, chunkTexts);

            if(semanticResultsOpt.has_value()) {
                for(const auto& res : semanticResultsOpt.value()) {
                    results.push_back({res.chunkIndex, res.score});
                }
            } else {
                printInfo("Semantic search failed or returned no results.");
                // No results added
            }
        }

        // --- Sort Results --- 
        if (searchMode == "fuzzy") {
            // Lower distance is better
            std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        } else if (searchMode == "semantic") {
            // Higher score is better
             std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        } // Keyword doesn't need sorting by score (all 1.0 or not present)
        

        // --- Output Results --- 
        if (outputFormat == "json") {
            json resultsJson = json::array();
            std::set<size_t> addedChunkIndices;
            for (const auto& matchPair : results) { // Use combined results vector
                size_t matchIndex = matchPair.first;
                if (addedChunkIndices.count(matchIndex)) continue;

                json resultEntry;
                resultEntry["match_chunk_index"] = matchIndex;
                resultEntry["match_score"] = matchPair.second;
                resultEntry["search_mode"] = searchMode;
                
                json contextChunkArray = json::array();
                size_t start = (matchIndex > (size_t)contextChunks) ? matchIndex - contextChunks : 0;
                size_t end = std::min(matchIndex + contextChunks, chunks.size() - 1);
                for (size_t i = start; i <= end; ++i) {
                     if (addedChunkIndices.find(i) == addedChunkIndices.end()) {
                        json chunkJson;
                        chunks[i].to_json(chunkJson, chunks[i]);
                        chunkJson["is_direct_match"] = (i == matchIndex);
                        contextChunkArray.push_back(chunkJson);
                        addedChunkIndices.insert(i);
                    }
                }
                resultEntry["context_chunks"] = contextChunkArray;
                resultsJson.push_back(resultEntry);
            }
            std::cout << resultsJson.dump(2) << std::endl;
        } else if (outputFormat == "markdown") {
            // --- Markdown Output --- (Placeholder for now)
            std::cout << "## ContextCracker Results" << std::endl;
             std::cout << "* File: `" << filePath << "`" << std::endl;
             std::cout << "* Query: `" << query << "`" << std::endl;
             std::cout << "* Mode: `" << searchMode << "`" << std::endl;
             // TODO: Implement detailed markdown formatting for results
             std::cout << "\n*(Markdown output is incomplete)*" << std::endl;

        } else { // Text output (with color)
            if (results.empty()) {
                std::cout << (useColor ? Color::YELLOW : "") << "No matches found." << (useColor ? Color::RESET : "") << std::endl;
            } else {
                printInfo("Found " + std::to_string(results.size()) + " matching chunk(s). Displaying context...");
                std::set<size_t> printedChunkIndices;
                for (const auto& matchPair : results) {
                    size_t matchIndex = matchPair.first;
                    double matchScore = matchPair.second;
                    if (printedChunkIndices.count(matchIndex)) continue;

                    size_t start = (matchIndex > (size_t)contextChunks) ? matchIndex - contextChunks : 0;
                    size_t end = std::min(matchIndex + contextChunks, chunks.size() - 1);
                    bool firstChunkInGroup = true;
                    for (size_t i = start; i <= end; ++i) {
                        if (printedChunkIndices.find(i) == printedChunkIndices.end()) {
                            if (firstChunkInGroup) {
                                 std::cout << (useColor ? Color::DIM : "") << "\n--------------------" << (useColor ? Color::RESET : "") << std::endl;
                                 firstChunkInGroup = false;
                            }
                            const auto& chunkToPrint = chunks[i];
                            // Header line
                            std::cout << (useColor ? Color::CYAN : "") 
                                      << "[L" << chunkToPrint.startLine
                                      << (i == matchIndex ? (useColor ? Color::BOLD + Color::GREEN : "") + " (*) " + (useColor ? Color::RESET + Color::CYAN : "") : "     ")
                                      << "Chunk " << (i + 1) << "/" << chunks.size() << "]"
                                      << (useColor ? Color::RESET : "");
                            
                            // Score (if applicable)
                            if (i == matchIndex) {
                                std::stringstream ss;
                                if (searchMode == "fuzzy") {
                                    ss << " (Dist: " << matchScore << ")";
                                } else if (searchMode == "semantic") {
                                     ss << std::fixed << std::setprecision(4) << matchScore;
                                     ss << " (Score: " << ss.str() << ")";
                                }
                                 std::cout << (useColor ? Color::YELLOW : "") << ss.str() << (useColor ? Color::RESET : "");
                            }
                            std::cout << std::endl;

                            // Content lines (highlight matching words? - simple version for now)
                            for (const auto& chunkLine : chunkToPrint.lines) {
                                std::string lineToPrint = chunkLine;
                                if (useColor && (searchMode == "keyword" || searchMode == "fuzzy") && !query.empty()) {
                                    // Basic highlighting for keyword/fuzzy (first occurrence)
                                    size_t pos = lineToPrint.find(query); // case-sensitive
                                    if (pos != std::string::npos) {
                                        lineToPrint.replace(pos, query.length(), 
                                            Color::BOLD + Color::MAGENTA + query + Color::RESET);
                                    }
                                    // Note: Fuzzy highlighting is harder - would need to know matched word
                                }
                                // Dim context lines that are not the direct match?
                                bool isContextOnly = (i != matchIndex);
                                std::cout << (useColor && isContextOnly ? Color::GRAY : "") 
                                          << lineToPrint 
                                          << (useColor && isContextOnly ? Color::RESET : "") << std::endl;
                            }
                            std::cout << (useColor ? Color::DIM : "") << "---" << (useColor ? Color::RESET : "") << std::endl;
                            printedChunkIndices.insert(i);
                        }
                    }
                }
                std::cout << (useColor ? Color::DIM : "") << "\n--------------------" << (useColor ? Color::RESET : "") << std::endl;
            }
        }

    } catch (const std::exception& e) {
        // Decide whether to print error as JSON or text
        if (outputFormat == "json") {
            json errorJson;
            errorJson["error"] = e.what();
            std::cerr << errorJson.dump(2) << std::endl;
        } else {
            std::cerr << "Error: " << e.what() << std::endl;
        }
        if (pythonInitialized) PythonBridge::finalize(); // Finalize python on error too
        return 1; // Indicate error
    }

    // --- Finalize Python Bridge --- 
    if (pythonInitialized) {
        PythonBridge::finalize();
    }

    return 0;
} 