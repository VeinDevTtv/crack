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
#include "CLI/CLI.hpp"

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

    std::string filePath;
    app.add_option("-f,--file", filePath, "Path to the input file")
       ->required()
       ->check(CLI::ExistingFile);

    std::string query;
    app.add_option("-q,--query", query, "Search query")->required();

    int contextChunks = 0;
    app.add_option("-C,--context", contextChunks, "Number of surrounding paragraphs (chunks) to show")
        ->default_val(0)
        ->check(CLI::NonNegativeNumber);

    std::string searchMode = "keyword";
    app.add_option("-m,--mode", searchMode, "Search mode (keyword, fuzzy)")
        ->default_val("keyword")
        ->check(CLI::IsMember({"keyword", "fuzzy"}));

    int fuzzyTolerance = 2;
    app.add_option("--tolerance", fuzzyTolerance, "Maximum Levenshtein distance for fuzzy search")
        ->default_val(2)
        ->check(CLI::NonNegativeNumber);

    bool ignoreComments = false;
    app.add_flag("--ignore-comments", ignoreComments, "Ignore lines starting with // or #");

    std::string chunkMode = "paragraph";
    app.add_option("--chunk-mode", chunkMode, "Chunking strategy (paragraph, brace)")
        ->default_val("paragraph")
        ->check(CLI::IsMember({"paragraph", "brace"}));

    CLI11_PARSE(app, argc, argv);

    std::cout << "Processing file: " << filePath << std::endl;
    std::cout << "Search query: '" << query << "'" << std::endl;
    std::cout << "Search mode: " << searchMode << std::endl;
    if (searchMode == "fuzzy") {
        std::cout << "Fuzzy tolerance: " << fuzzyTolerance << std::endl;
    }
    std::cout << "Context chunks: " << contextChunks << std::endl;
    std::cout << "Ignore comments: " << (ignoreComments ? "Yes" : "No") << std::endl;
    std::cout << "Chunk mode: " << chunkMode << std::endl;

    try {
        std::vector<std::string> fileContent = readFileLines(filePath);
        std::cout << "Successfully read " << fileContent.size() << " lines from file." << std::endl;

        // --- Chunking --- 
        std::vector<Chunk> chunks;
        if (chunkMode == "paragraph") {
             chunks = chunkByParagraphs(fileContent);
             std::cout << "Chunked content into " << chunks.size() << " paragraphs (non-blank)." << std::endl;
        } else if (chunkMode == "brace") {
            chunks = chunkByBraces(fileContent);
             std::cout << "Chunked content into " << chunks.size() << " brace-based blocks." << std::endl;
        } else {
             // Should not happen due to CLI11 validation, but good practice
            throw std::runtime_error("Invalid chunk mode specified.");
        }
       
        // --- Search --- 
        std::cout << "\n--- " << searchMode << " Search Results --- (" << query << ")" << std::endl;
        std::vector<std::pair<size_t, size_t>> matchingChunkInfo; // Store {chunk_index, match_score (0 for keyword, distance for fuzzy)}

        for (size_t i = 0; i < chunks.size(); ++i) {
            const auto& chunk = chunks[i];
            size_t bestMatchScore = std::numeric_limits<size_t>::max(); 
            bool chunkMatchFound = false;

            for (const auto& line : chunk.lines) {
                // --- Filtering --- 
                std::string processedLine = line;
                // Trim leading whitespace for comment check
                size_t firstChar = processedLine.find_first_not_of(" \t");
                if (ignoreComments && firstChar != std::string::npos) {
                    if (processedLine.rfind("//", firstChar) == firstChar || 
                        processedLine.rfind("#", firstChar) == firstChar) {
                        continue; // Skip this line if it's a comment
                    }
                }
                // --- End Filtering ---

                if (searchMode == "keyword") {
                    if (processedLine.find(query) != std::string::npos) { // Search on processedLine
                        bestMatchScore = 0; 
                        chunkMatchFound = true;
                        break; 
                    }
                } else if (searchMode == "fuzzy") {
                    size_t queryLen = query.length();
                    if (queryLen == 0) continue;
                    for (size_t j = 0; (j = processedLine.find_first_not_of(" \t\n\r", j)) != std::string::npos; ) {
                         size_t wordEnd = processedLine.find_first_of(" \t\n\r", j);
                         if (wordEnd == std::string::npos) {
                            wordEnd = processedLine.length(); 
                         }
                         std::string word = processedLine.substr(j, wordEnd - j);
                         
                         size_t distance = levenshteinDistance(word, query);
                         if (distance <= (size_t)fuzzyTolerance) {
                              if (distance < bestMatchScore) {
                                 bestMatchScore = distance;
                             }
                             chunkMatchFound = true;
                         }
                         if (wordEnd == processedLine.length()) break;
                         j = wordEnd + 1;
                    }
                } 
            } 
            if (chunkMatchFound) {
                matchingChunkInfo.push_back({i, bestMatchScore});
            }
        } 

        // --- Sort Results (for fuzzy, lower distance is better) ---
        if (searchMode == "fuzzy") {
             std::sort(matchingChunkInfo.begin(), matchingChunkInfo.end(), 
                       [](const auto& a, const auto& b) {
                           return a.second < b.second; // Sort by distance (ascending)
                       });
        }

        // --- Output Results --- 
        if (matchingChunkInfo.empty()) {
            std::cout << "No matches found." << std::endl;
        } else {
            std::cout << "Found " << matchingChunkInfo.size() << " matching paragraph(s)." << std::endl;
            std::set<size_t> printedChunkIndices; 

            for (const auto& matchPair : matchingChunkInfo) {
                size_t matchIndex = matchPair.first;
                size_t matchScore = matchPair.second;

                // Avoid re-printing context for matches already covered
                if (printedChunkIndices.count(matchIndex)) {
                    continue;
                }

                size_t start = (matchIndex > (size_t)contextChunks) ? matchIndex - contextChunks : 0;
                size_t end = std::min(matchIndex + contextChunks, chunks.size() - 1);

                bool firstChunkInGroup = true;
                for (size_t i = start; i <= end; ++i) {
                    if (printedChunkIndices.find(i) == printedChunkIndices.end()) {
                        if (firstChunkInGroup) {
                             std::cout << "\n--------------------" << std::endl; 
                             firstChunkInGroup = false;
                        }
                        const auto& chunkToPrint = chunks[i];
                        std::cout << "[L" << chunkToPrint.startLine 
                                  << (i == matchIndex ? " (*) " : "     ") // Mark the matching chunk
                                  << "Chunk " << (i+1) << "/" << chunks.size() << "]";
                        if (searchMode == "fuzzy" && i == matchIndex) {
                             std::cout << " (Dist: " << matchScore << ")";
                        }
                        std::cout << std::endl;

                        for(const auto& chunkLine : chunkToPrint.lines) {
                            std::cout << chunkLine << std::endl;
                        }
                         std::cout << "---" << std::endl; 
                        printedChunkIndices.insert(i);
                    }
                }
            }
             std::cout << "\n--------------------" << std::endl;
        }
        // --- TODO: Implement semantic search --- 

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1; // Indicate error
    }

    return 0;
} 