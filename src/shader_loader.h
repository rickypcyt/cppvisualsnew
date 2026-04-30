#pragma once

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// Metadata for a single procedural effect
struct EffectMetadata {
    std::string name;           // Display name (e.g., "ASCII Ocean")
    std::string shaderFile;     // Source file (e.g., "procedural_pack3.glsl")
    int modeIndex;              // uMode value passed to shader
    std::string description;    // Optional description
    std::string author;         // Optional author
    bool enabled = true;        // Whether this effect is enabled for randomization and UI
    float defaultZoom = 1.0f;   // Default camera zoom for this effect

    bool isValid() const { return !name.empty() && modeIndex >= 0; }
};

// Registry that holds all discovered effects from shader parsing
class EffectRegistry {
public:
    // Scan shader files and extract @EFFECT metadata
    void scanShaderFiles(const std::vector<std::string>& searchRoots,
                         const std::vector<std::string>& shaderFiles);
    
    // Get sorted list of effects (sorted by modeIndex)
    std::vector<EffectMetadata> getAllEffects() const;
    
    // Get effect by mode index
    std::optional<EffectMetadata> getEffectByMode(int modeIndex) const;
    
    // Get effect by name
    std::optional<EffectMetadata> getEffectByName(const std::string& name) const;
    
    // Get display names for UI (sorted by modeIndex)
    std::vector<std::string> getEffectNameStrings() const;
    
    // Helper: Get effect index by name (returns -1 if not found)
    int getEffectIndexByName(const std::string& name) const;
    
    // Helper: Get effect name by index (returns empty if not found)
    std::string getEffectNameByIndex(int index) const;
    
    // Check if registry has any effects
    bool empty() const { return effects_.empty(); }
    
    size_t size() const { return effects_.size(); }
    
    // Clear all registered effects
    void clear() { effects_.clear(); }

private:
    std::unordered_map<int, EffectMetadata> effects_;  // keyed by modeIndex
    
    // Parse a single shader file for @EFFECT annotations
    void parseShaderFile(const std::filesystem::path& path,
                        const std::string& shaderFileName);
    
    // Parse one @EFFECT line
    std::optional<EffectMetadata> parseEffectLine(const std::string& line,
                                                   const std::string& shaderFile);
};

// Global registry instance (initialized on first use)
EffectRegistry& GetEffectRegistry();

// Common search roots used to locate shader source files at runtime.
inline const std::vector<std::string> kShaderSearchRoots = {
    "./shaders",
    "shaders",
    "../shaders",
    "../../shaders"
};

// Loads and concatenates shader source files provided in `shaderFiles` using
// the configured `searchRoots`. Returns true on success and stores the
// resulting source code in `outSource`. When provided, `outError` receives a
// diagnostic message on failure.
// Also returns a mapping of file IDs to filenames for #line directive debugging.
// Handles #version directives by extracting the first one and removing duplicates.
inline bool LoadShaderSources(const std::vector<std::string>& searchRoots,
                              const std::vector<std::string>& shaderFiles,
                              std::string& outSource,
                              std::string* outError,
                              std::vector<std::string>* outFileMapping = nullptr) {
    namespace fs = std::filesystem;

    outSource.clear();

    if (outFileMapping) {
        outFileMapping->clear();
    }

    std::string versionLine;
    std::vector<std::string> fileContents;

    // First pass: read all files and extract #version
    for (const std::string& shaderFile : shaderFiles) {
        fs::path resolvedPath;
        bool found = false;
        for (const auto& root : searchRoots) {
            fs::path candidate = fs::path(root) / shaderFile;
            if (fs::exists(candidate)) {
                resolvedPath = candidate;
                found = true;
                break;
            }
        }

        if (!found) {
            if (outError) {
                std::ostringstream msg;
                msg << "Unable to locate shader file '" << shaderFile << "' in search roots: ";
                for (size_t i = 0; i < searchRoots.size(); ++i) {
                    if (i > 0) {
                        msg << ", ";
                    }
                    msg << searchRoots[i];
                }
                *outError = msg.str();
            }
            return false;
        }

        std::ifstream file(resolvedPath, std::ios::in);
        if (!file.is_open()) {
            if (outError) {
                *outError = "Failed to open shader file: " + resolvedPath.string();
            }
            return false;
        }

        std::string content;
        std::string line;
        while (std::getline(file, line)) {
            // Extract first #version directive
            if (line.find("#version") == 0) {
                if (versionLine.empty()) {
                    versionLine = line;
                }
                // Skip #version lines in output (we'll add single version at start)
            } else {
                content += line + "\n";
            }
        }
        fileContents.push_back(content);

        // Store mapping for error translation
        if (outFileMapping) {
            outFileMapping->push_back(shaderFile);
        }
    }

    // Second pass: assemble with single #version at start
    if (!versionLine.empty()) {
        outSource += versionLine + "\n";
    }

    int fileId = 0;
    for (const auto& content : fileContents) {
        // Insert #line directive for debugging
        outSource += "\n#line 1 " + std::to_string(fileId) + "\n";
        outSource += content;
        fileId++;
    }

    return true;
}

template <size_t N>
bool LoadShaderSources(const std::vector<std::string>& searchRoots,
                       const std::array<const char*, N>& shaderFiles,
                       std::string& outSource,
                       std::string* outError,
                       std::vector<std::string>* outFileMapping = nullptr) {
    std::vector<std::string> files;
    files.reserve(N);
    for (const char* file : shaderFiles) {
        files.emplace_back(file);
    }
    return LoadShaderSources(searchRoots, files, outSource, outError, outFileMapping);
}

