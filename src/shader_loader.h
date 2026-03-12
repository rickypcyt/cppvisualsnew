#pragma once

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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
inline bool LoadShaderSources(const std::vector<std::string>& searchRoots,
                              const std::vector<std::string>& shaderFiles,
                              std::string& outSource,
                              std::string* outError) {
    namespace fs = std::filesystem;

    outSource.clear();

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

        std::ostringstream buffer;
        buffer << file.rdbuf();

        outSource += "\n// --- Begin " + shaderFile + " ---\n";
        outSource += buffer.str();
        outSource += "\n// --- End " + shaderFile + " ---\n";
    }

    return true;
}

template <size_t N>
bool LoadShaderSources(const std::vector<std::string>& searchRoots,
                       const std::array<const char*, N>& shaderFiles,
                       std::string& outSource,
                       std::string* outError) {
    std::vector<std::string> files;
    files.reserve(N);
    for (const char* file : shaderFiles) {
        files.emplace_back(file);
    }
    return LoadShaderSources(searchRoots, files, outSource, outError);
}

