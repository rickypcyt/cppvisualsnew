#include "layer_validator.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

LayerValidator::LayerValidator() {
    // Rutas de búsqueda por defecto
    shaderSearchPaths_ = {
        "shaders/",
        "shaders/post_effects/",
        "shaders/procedural/",
        "./shaders/",
        "./shaders/post_effects/",
        "./shaders/procedural/",
        "../shaders/",
        "../shaders/post_effects/",
        "../shaders/procedural/"
    };
}

ValidationResult LayerValidator::validateAllLayers() {
    ValidationResult result;
    
    reportProgress("Inicio de validación", 0, 5);
    
    // 1. Validar shaders procedurales
    auto proceduralResult = validateProceduralShaders();
    result.errors.insert(result.errors.end(), proceduralResult.errors.begin(), proceduralResult.errors.end());
    result.warnings.insert(result.warnings.end(), proceduralResult.warnings.begin(), proceduralResult.warnings.end());
    if (!proceduralResult.isValid) {
        result.isValid = false;
    }
    reportProgress("Shaders procedurales validados", 1, 5);
    
    // 2. Validar efectos de post-procesamiento
    auto postEffectsResult = validatePostEffects();
    result.errors.insert(result.errors.end(), postEffectsResult.errors.begin(), postEffectsResult.errors.end());
    result.warnings.insert(result.warnings.end(), postEffectsResult.warnings.begin(), postEffectsResult.warnings.end());
    if (!postEffectsResult.isValid) {
        result.isValid = false;
    }
    reportProgress("Efectos post-procesamiento validados", 2, 5);
    
    // 3. Validar sistema de capas modulares
    auto modularResult = validateModularLayerSystem();
    result.errors.insert(result.errors.end(), modularResult.errors.begin(), modularResult.errors.end());
    result.warnings.insert(result.warnings.end(), modularResult.warnings.begin(), modularResult.warnings.end());
    if (!modularResult.isValid) {
        result.isValid = false;
    }
    reportProgress("Sistema modular validado", 3, 5);
    
    // 4. Validar post-procesador
    auto postProcessorResult = validatePostProcessorShaders();
    result.errors.insert(result.errors.end(), postProcessorResult.errors.begin(), postProcessorResult.errors.end());
    result.warnings.insert(result.warnings.end(), postProcessorResult.warnings.begin(), postProcessorResult.warnings.end());
    if (!postProcessorResult.isValid) {
        result.isValid = false;
    }
    reportProgress("Post-procesador validado", 4, 5);
    
    // 5. Validación final de integración
    if (result.isValid) {
        if (verbose_) {
            std::cout << "LayerValidator: Todas las capas validadas correctamente" << std::endl;
        }
    } else {
        std::cerr << "LayerValidator: Se encontraron " << result.errors.size() << " errores" << std::endl;
    }
    reportProgress("Validación completada", 5, 5);
    
    return result;
}

ValidationResult LayerValidator::validateShader(const std::string& shaderPath) {
    ValidationResult result;
    
    if (!fileExists(shaderPath)) {
        result.addError("Shader file no encontrado: " + shaderPath);
        return result;
    }
    
    if (!fileReadable(shaderPath)) {
        result.addError("Shader file no legible: " + shaderPath);
        return result;
    }
    
    // Validar sintaxis GLSL
    std::string source = readFileContents(shaderPath);
    if (source.empty()) {
        result.addError("Shader file vacío: " + shaderPath);
        return result;
    }
    
    std::string syntaxError;
    if (!validateGLSLSyntax(source, syntaxError)) {
        result.addError("Error de sintaxis GLSL en " + shaderPath + ": " + syntaxError);
    }
    
    // Validar dependencias
    auto depResult = validateShaderDependencies(shaderPath);
    result.errors.insert(result.errors.end(), depResult.errors.begin(), depResult.errors.end());
    result.warnings.insert(result.warnings.end(), depResult.warnings.begin(), depResult.warnings.end());
    
    return result;
}

ValidationResult LayerValidator::validateShaderCompilation(const std::string& vertexSource, const std::string& fragmentSource) {
    ValidationResult result;
    
    // Validar shaders individuales
    std::string vertexError, fragmentError;
    
    if (!validateGLSLSyntax(vertexSource, vertexError)) {
        result.addError("Error sintaxis vertex shader: " + vertexError);
    }
    
    if (!validateGLSLSyntax(fragmentSource, fragmentError)) {
        result.addError("Error sintaxis fragment shader: " + fragmentError);
    }
    
    // Validar compatibilidad entre shaders
    if (vertexSource.find("#version") == std::string::npos) {
        result.addWarning("Vertex shader sin directiva #version");
    }
    
    if (fragmentSource.find("#version") == std::string::npos) {
        result.addWarning("Fragment shader sin directiva #version");
    }
    
    return result;
}

ValidationResult LayerValidator::validateShaderDependencies(const std::string& shaderPath) {
    ValidationResult result;
    
    std::string source = readFileContents(shaderPath);
    if (source.empty()) {
        result.addError("No se pudo leer el shader: " + shaderPath);
        return result;
    }
    
    auto includes = extractIncludes(source);
    
    for (const auto& include : includes) {
        bool found = false;
        
        // Buscar en todas las rutas de búsqueda
        for (const auto& searchPath : shaderSearchPaths_) {
            std::string fullPath = searchPath + include;
            if (fileExists(fullPath)) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            result.addError("Include no encontrado: " + include + " (requerido por " + shaderPath + ")");
        } else {
            // Validar recursivamente los includes
            for (const auto& searchPath : shaderSearchPaths_) {
                std::string fullPath = searchPath + include;
                if (fileExists(fullPath)) {
                    auto depResult = validateShaderDependencies(fullPath);
                    result.errors.insert(result.errors.end(), depResult.errors.begin(), depResult.errors.end());
                    result.warnings.insert(result.warnings.end(), depResult.warnings.begin(), depResult.warnings.end());
                    break;
                }
            }
        }
    }
    
    return result;
}

std::vector<ShaderDependency> LayerValidator::getDependencyTree(const std::string& shaderPath) {
    std::vector<ShaderDependency> dependencies;
    
    std::string source = readFileContents(shaderPath);
    if (source.empty()) {
        return dependencies;
    }
    
    auto includes = extractIncludes(source);
    
    for (const auto& include : includes) {
        ShaderDependency dep;
        dep.filePath = include;
        dep.exists = false;
        dep.isReadable = false;
        
        // Buscar en rutas de búsqueda
        for (const auto& searchPath : shaderSearchPaths_) {
            std::string fullPath = searchPath + include;
            if (fileExists(fullPath)) {
                dep.exists = true;
                dep.isReadable = fileReadable(fullPath);
                
                // Obtener dependencias anidadas
                auto nestedDeps = getDependencyTree(fullPath);
                dep.includes.reserve(nestedDeps.size());
                for (const auto& nested : nestedDeps) {
                    dep.includes.push_back(nested.filePath);
                }
                break;
            }
        }
        
        dependencies.push_back(dep);
    }
    
    return dependencies;
}

ValidationResult LayerValidator::validateLayerConfiguration(const LayerConfiguration& config) {
    ValidationResult result;
    
    if (config.name.empty()) {
        result.addError("Configuración de capa sin nombre");
    }
    
    if (config.shaderFiles.empty()) {
        result.addError("Capa '" + config.name + "' sin shaders definidos");
        return result;
    }
    
    // Validar cada shader
    for (const auto& shaderFile : config.shaderFiles) {
        auto shaderResult = validateShader(shaderFile);
        result.errors.insert(result.errors.end(), shaderResult.errors.begin(), shaderResult.errors.end());
        result.warnings.insert(result.warnings.end(), shaderResult.warnings.begin(), shaderResult.warnings.end());
    }
    
    return result;
}

ValidationResult LayerValidator::validateModularLayerSystem() {
    ValidationResult result;
    
    // Validar shaders procedurales principales
    const std::vector<std::string> proceduralShaders = {
        "procedural_header.glsl",
        "procedural_helpers.glsl",
        "procedural_shadertoy_bridge.glsl",
        "procedural_main.glsl"
    };
    
    for (const auto& shader : proceduralShaders) {
        auto shaderResult = validateShader(shader);
        if (!shaderResult.isValid) {
            result.addError("Shader procedural crítico inválido: " + shader);
        }
        result.warnings.insert(result.warnings.end(), shaderResult.warnings.begin(), shaderResult.warnings.end());
    }
    
    // Validar packs de shaders
    for (int i = 1; i <= 10; ++i) {
        std::string packShader = "procedural_pack" + std::to_string(i) + ".glsl";
        auto shaderResult = validateShader(packShader);
        if (!shaderResult.isValid) {
            result.addWarning("Pack shader no encontrado: " + packShader);
        }
    }
    
    return result;
}

ValidationResult LayerValidator::validatePostProcessorShaders() {
    ValidationResult result;
    
    // Validar shader común
    auto commonResult = validateShader("post_common.glsl");
    if (!commonResult.isValid) {
        result.addError("Shader común de post-procesamiento inválido");
    }
    
    // Validar shaders de efectos
    const std::vector<std::string> effectShaders = {
        "effect_passthrough.glsl",
        "effect_grayscale.glsl",
        "effect_filmic.glsl",
        "effect_crt.glsl",
        "effect_chromatic_pulse.glsl",
        "effect_bass_threshold.glsl",
        "effect_radial_blur.glsl",
        "effect_kaleidoscope.glsl"
    };
    
    for (const auto& shader : effectShaders) {
        std::string fullPath = "post_effects/" + shader;
        auto shaderResult = validateShader(fullPath);
        if (!shaderResult.isValid) {
            result.addError("Efecto de post-procesamiento inválido: " + shader);
        }
    }
    
    return result;
}

ValidationResult LayerValidator::validateEffectChain(const std::vector<int>& effectModes) {
    ValidationResult result;
    
    // Validar que los modos estén en rango
    for (int mode : effectModes) {
        if (mode < 0 || mode > 30) { // Rango típico de efectos
            result.addWarning("Modo de efecto fuera de rango: " + std::to_string(mode));
        }
    }
    
    // Validar compatibilidad de efectos en cadena
    for (size_t i = 0; i < effectModes.size(); ++i) {
        if (effectModes[i] == 0 && i > 0) {
            result.addWarning("Efecto passthrough en medio de la cadena (posición " + std::to_string(i) + ")");
        }
    }
    
    return result;
}

void LayerValidator::setShaderSearchPaths(const std::vector<std::string>& paths) {
    shaderSearchPaths_ = paths;
}

bool LayerValidator::fileExists(const std::string& path) {
    return fs::exists(path);
}

bool LayerValidator::fileReadable(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

std::string LayerValidator::readFileContents(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<std::string> LayerValidator::extractIncludes(const std::string& source) {
    std::vector<std::string> includes;
    
    std::regex includeRegex(R"(#include\s*[<"]([^>"]+)[>"])");
    std::sregex_iterator iter(source.begin(), source.end(), includeRegex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        includes.push_back(iter->str(1));
    }
    
    return includes;
}

bool LayerValidator::validateGLSLSyntax(const std::string& source, std::string& error) {
    // Validación básica de sintaxis GLSL
    if (source.empty()) {
        error = "Fuente vacía";
        return false;
    }
    
    // Si tiene #include, validar solo la estructura básica sin #version
    if (source.find("#include") != std::string::npos) {
        // Validar que tenga función main
        if (source.find("void main") == std::string::npos && source.find("void main()") == std::string::npos) {
            error = "Falta función main";
            return false;
        }
        
        // Validar balance de llaves
        int braceCount = 0;
        for (char c : source) {
            if (c == '{') braceCount++;
            else if (c == '}') braceCount--;
        }
        
        if (braceCount != 0) {
            error = "Llaves desbalanceadas";
            return false;
        }
        
        return true;
    }
    
    // Para shaders sin include, requerir directiva #version
    if (source.find("#version") == std::string::npos) {
        error = "Falta directiva #version";
        return false;
    }
    
    // Validar estructura básica
    if (source.find("void main") == std::string::npos && source.find("void main()") == std::string::npos) {
        error = "Falta función main";
        return false;
    }
    
    // Validar balance de llaves
    int braceCount = 0;
    for (char c : source) {
        if (c == '{') braceCount++;
        else if (c == '}') braceCount--;
    }
    
    if (braceCount != 0) {
        error = "Llaves desbalanceadas";
        return false;
    }
    
    return true;
}

void LayerValidator::reportProgress(const std::string& stage, int current, int total) {
    if (progressCallback_) {
        progressCallback_(stage, current, total);
    }
    
    if (verbose_) {
        std::cout << "LayerValidator: " << stage << " (" << current << "/" << total << ")" << std::endl;
    }
}

ValidationResult LayerValidator::validateProceduralShaders() {
    ValidationResult result;
    
    const std::vector<std::string> requiredShaders = {
        "procedural_header.glsl",
        "procedural_helpers.glsl",
        "procedural_shadertoy_bridge.glsl",
        "procedural_main.glsl"
    };
    
    for (const auto& shader : requiredShaders) {
        auto shaderResult = validateShader(shader);
        if (!shaderResult.isValid) {
            result.addError("Shader procedural requerido inválido: " + shader);
        }
        result.warnings.insert(result.warnings.end(), shaderResult.warnings.begin(), shaderResult.warnings.end());
    }
    
    return result;
}

ValidationResult LayerValidator::validatePostEffects() {
    ValidationResult result;
    
    const std::vector<std::string> effectShaders = {
        "effect_passthrough.glsl",
        "effect_grayscale.glsl",
        "effect_filmic.glsl",
        "effect_crt.glsl",
        "effect_chromatic_pulse.glsl",
        "effect_bass_threshold.glsl",
        "effect_radial_blur.glsl",
        "effect_kaleidoscope.glsl",
        "effect_digital_glitch.glsl",
        "effect_pixelate_64.glsl",
        "effect_lens_distort.glsl",
        "effect_rotating_lens.glsl",
        "effect_rgb_shift.glsl",
        "effect_bloom_aces.glsl"
    };
    
    for (const auto& shader : effectShaders) {
        auto shaderResult = validateShader(shader);
        if (!shaderResult.isValid) {
            result.addError("Efecto de post-procesamiento inválido: " + shader);
        }
        result.warnings.insert(result.warnings.end(), shaderResult.warnings.begin(), shaderResult.warnings.end());
    }
    
    return result;
}
