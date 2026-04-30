#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

struct ValidationResult {
    bool isValid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    void addError(const std::string& error) {
        errors.push_back(error);
        isValid = false;
    }
    
    void addWarning(const std::string& warning) {
        warnings.push_back(warning);
    }
    
    bool hasIssues() const {
        return !errors.empty() || !warnings.empty();
    }
};

struct ShaderDependency {
    std::string filePath;
    std::vector<std::string> includes;
    bool exists = false;
    bool isReadable = false;
};

struct LayerConfiguration {
    std::string name;
    std::vector<std::string> shaderFiles;
    std::vector<std::string> requiredUniforms;
    bool enabled = true;
};

class LayerValidator {
public:
    LayerValidator();
    ~LayerValidator() = default;
    
    // Validación principal del sistema de capas
    ValidationResult validateAllLayers();
    
    // Validación de shaders individuales
    ValidationResult validateShader(const std::string& shaderPath);
    ValidationResult validateShaderCompilation(const std::string& vertexSource, const std::string& fragmentSource);
    
    // Validación de dependencias
    ValidationResult validateShaderDependencies(const std::string& shaderPath);
    std::vector<ShaderDependency> getDependencyTree(const std::string& shaderPath);
    
    // Validación de configuración de capas
    ValidationResult validateLayerConfiguration(const LayerConfiguration& config);
    ValidationResult validateModularLayerSystem();
    
    // Validación de post-procesamiento
    ValidationResult validatePostProcessorShaders();
    ValidationResult validateEffectChain(const std::vector<int>& effectModes);
    
    // Utilidades
    void setShaderSearchPaths(const std::vector<std::string>& paths);
    void enableVerboseMode(bool enabled) { verbose_ = enabled; }
    
    // Callback para reporte progreso
    using ProgressCallback = std::function<void(const std::string& stage, int progress, int total)>;
    void setProgressCallback(ProgressCallback callback) { progressCallback_ = callback; }

private:
    std::vector<std::string> shaderSearchPaths_;
    bool verbose_ = false;
    ProgressCallback progressCallback_;
    
    // Métodos internos
    bool fileExists(const std::string& path);
    bool fileReadable(const std::string& path);
    std::string readFileContents(const std::string& path);
    std::vector<std::string> extractIncludes(const std::string& source);
    bool validateGLSLSyntax(const std::string& source, std::string& error);
    void validateCommonFunctions(const std::string& source, std::string& error);
    
    void reportProgress(const std::string& stage, int current, int total);
    ValidationResult validateProceduralShaders();
    ValidationResult validatePostEffects();
};
