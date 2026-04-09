#include "shader_loader.h"

#include <algorithm>
#include <iostream>

// Global registry instance
EffectRegistry& GetEffectRegistry() {
    static EffectRegistry registry;
    return registry;
}

// Parse a single shader file for @EFFECT annotations
void EffectRegistry::parseShaderFile(const std::filesystem::path& path,
                                      const std::string& shaderFileName) {
    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        std::cerr << "EffectRegistry: Failed to open shader file: " << path << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Look for @EFFECT annotation
        auto metadata = parseEffectLine(line, shaderFileName);
        if (metadata && metadata->isValid()) {
            // Check for duplicates
            if (effects_.find(metadata->modeIndex) != effects_.end()) {
                std::cerr << "EffectRegistry: Warning: Duplicate mode index " 
                         << metadata->modeIndex << " (" << metadata->name 
                         << " vs " << effects_[metadata->modeIndex].name << ")" << std::endl;
            }
            effects_[metadata->modeIndex] = *metadata;
            std::cout << "EffectRegistry: Registered effect [" << metadata->modeIndex 
                     << "] \"" << metadata->name << "\" from " << shaderFileName << std::endl;
        }
    }
}

// Parse one @EFFECT line
// Format: // @EFFECT name="Effect Name" index=N [desc="..."] [author="..."] [zoom=F]
std::optional<EffectMetadata> EffectRegistry::parseEffectLine(const std::string& line,
                                                               const std::string& shaderFile) {
    // Check if line contains @EFFECT
    if (line.find("@EFFECT") == std::string::npos) {
        return std::nullopt;
    }

    EffectMetadata metadata;
    metadata.shaderFile = shaderFile;

    // Extract name="..."
    std::regex nameRegex(R"xx(name\s*=\s*"([^"]+)")xx");
    std::smatch nameMatch;
    if (std::regex_search(line, nameMatch, nameRegex)) {
        metadata.name = nameMatch[1].str();
    }

    // Extract index=N
    std::regex indexRegex(R"xx(index\s*=\s*(\d+))xx");
    std::smatch indexMatch;
    if (std::regex_search(line, indexMatch, indexRegex)) {
        metadata.modeIndex = std::stoi(indexMatch[1].str());
    } else {
        // Try alternate format: @EFFECT 1 "Name"
        std::regex altRegex(R"xx(@EFFECT\s+(\d+)\s+"([^"]+)")xx");
        std::smatch altMatch;
        if (std::regex_search(line, altMatch, altRegex)) {
            metadata.modeIndex = std::stoi(altMatch[1].str());
            metadata.name = altMatch[2].str();
        }
    }

    // Extract optional description="..."
    std::regex descRegex(R"xx(desc\s*=\s*"([^"]*)")xx");
    std::smatch descMatch;
    if (std::regex_search(line, descMatch, descRegex)) {
        metadata.description = descMatch[1].str();
    }

    // Extract optional author="..."
    std::regex authorRegex(R"xx(author\s*=\s*"([^"]*)")xx");
    std::smatch authorMatch;
    if (std::regex_search(line, authorMatch, authorRegex)) {
        metadata.author = authorMatch[1].str();
    }

    // Extract optional zoom=F (default camera zoom for this effect)
    std::regex zoomRegex(R"xx(zoom\s*=\s*([+-]?\d*\.?\d+))xx");
    std::smatch zoomMatch;
    if (std::regex_search(line, zoomMatch, zoomRegex)) {
        metadata.defaultZoom = std::stof(zoomMatch[1].str());
    }

    if (metadata.isValid()) {
        return metadata;
    }
    return std::nullopt;
}

// Scan shader files and extract @EFFECT metadata
void EffectRegistry::scanShaderFiles(const std::vector<std::string>& searchRoots,
                                     const std::vector<std::string>& shaderFiles) {
    effects_.clear();
    
    for (const std::string& shaderFile : shaderFiles) {
        // Try to find the file in search roots
        std::filesystem::path resolvedPath;
        bool found = false;
        
        for (const auto& root : searchRoots) {
            std::filesystem::path candidate = std::filesystem::path(root) / shaderFile;
            if (std::filesystem::exists(candidate)) {
                resolvedPath = candidate;
                found = true;
                break;
            }
        }

        if (!found) {
            std::cerr << "EffectRegistry: Shader file not found: " << shaderFile << std::endl;
            continue;
        }

        parseShaderFile(resolvedPath, shaderFile);
    }

    std::cout << "EffectRegistry: Loaded " << effects_.size() << " effects from " 
             << shaderFiles.size() << " shader files" << std::endl;
}

// Get sorted list of effects (sorted by modeIndex)
std::vector<EffectMetadata> EffectRegistry::getAllEffects() const {
    std::vector<EffectMetadata> result;
    result.reserve(effects_.size());
    
    for (const auto& [index, metadata] : effects_) {
        result.push_back(metadata);
    }
    
    // Sort by modeIndex
    std::sort(result.begin(), result.end(),
              [](const EffectMetadata& a, const EffectMetadata& b) {
                  return a.modeIndex < b.modeIndex;
              });
    
    return result;
}

// Get effect by mode index
std::optional<EffectMetadata> EffectRegistry::getEffectByMode(int modeIndex) const {
    auto it = effects_.find(modeIndex);
    if (it != effects_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// Get effect by name
std::optional<EffectMetadata> EffectRegistry::getEffectByName(const std::string& name) const {
    for (const auto& [index, metadata] : effects_) {
        if (metadata.name == name) {
            return metadata;
        }
    }
    return std::nullopt;
}

// Get display names for UI (sorted by modeIndex)
// Returns a vector of string names that can be used to build const char* array
std::vector<std::string> EffectRegistry::getEffectNameStrings() const {
    auto effects = getAllEffects();
    std::vector<std::string> names;
    names.reserve(effects.size());
    
    for (const auto& effect : effects) {
        names.push_back(effect.name);
    }
    
    return names;
}

// Helper: Get effect index by name (returns -1 if not found)
int EffectRegistry::getEffectIndexByName(const std::string& name) const {
    for (const auto& [index, metadata] : effects_) {
        if (metadata.name == name) {
            return index;
        }
    }
    return -1;
}

// Helper: Get effect name by index (returns empty if not found)
std::string EffectRegistry::getEffectNameByIndex(int index) const {
    auto it = effects_.find(index);
    if (it != effects_.end()) {
        return it->second.name;
    }
    return "";
}

