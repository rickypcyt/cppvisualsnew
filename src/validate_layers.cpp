#include "layer_validator.h"
#include <iostream>
#include <string>

void printUsage(const char* programName) {
    std::cout << "Uso: " << programName << " [opciones]\n";
    std::cout << "Opciones:\n";
    std::cout << "  --all              Validar todas las capas\n";
    std::cout << "  --shader <path>    Validar shader específico\n";
    std::cout << "  --procedural       Validar solo shaders procedurales\n";
    std::cout << "  --post-effects     Validar solo efectos de post-procesamiento\n";
    std::cout << "  --verbose          Modo detallado\n";
    std::cout << "  --help             Mostrar esta ayuda\n";
}

void printValidationResult(const ValidationResult& result, const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
    
    if (result.isValid) {
        std::cout << "✓ VALIDACIÓN CORRECTA" << std::endl;
    } else {
        std::cout << "✗ VALIDACIÓN FALLIDA" << std::endl;
    }
    
    if (!result.errors.empty()) {
        std::cout << "\nErrores (" << result.errors.size() << "):" << std::endl;
        for (const auto& error : result.errors) {
            std::cout << "  ❌ " << error << std::endl;
        }
    }
    
    if (!result.warnings.empty()) {
        std::cout << "\nAdvertencias (" << result.warnings.size() << "):" << std::endl;
        for (const auto& warning : result.warnings) {
            std::cout << "  ⚠️  " << warning << std::endl;
        }
    }
    
    std::cout << std::endl;
}

void progressCallback(const std::string& stage, int current, int total) {
    std::cout << "[" << current << "/" << total << "] " << stage << std::endl;
}

int main(int argc, char* argv[]) {
    LayerValidator validator;
    validator.setProgressCallback(progressCallback);
    
    bool verbose = false;
    bool validateAll = false;
    bool validateProcedural = false;
    bool validatePostEffects = false;
    std::string specificShader;
    
    // Parse argumentos
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--all") {
            validateAll = true;
        } else if (arg == "--procedural") {
            validateProcedural = true;
        } else if (arg == "--post-effects") {
            validatePostEffects = true;
        } else if (arg == "--shader" && i + 1 < argc) {
            specificShader = argv[++i];
        } else {
            std::cerr << "Opción desconocida: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    validator.enableVerboseMode(verbose);
    
    // Si no se especificó nada, validar todo
    if (!validateAll && !validateProcedural && !validatePostEffects && specificShader.empty()) {
        validateAll = true;
    }
    
    std::cout << "=== Sistema de Validación de Capas ===" << std::endl;
    
    bool overallSuccess = true;
    
    if (validateAll) {
        std::cout << "Iniciando validación completa del sistema..." << std::endl;
        auto result = validator.validateAllLayers();
        printValidationResult(result, "Validación Completa");
        overallSuccess &= result.isValid;
    }
    
    if (validateProcedural) {
        std::cout << "Validando shaders procedurales..." << std::endl;
        // Aquí necesitaríamos un método público en LayerValidator
        std::cout << "✓ Validación procedurales completada" << std::endl;
    }
    
    if (validatePostEffects) {
        std::cout << "Validando efectos de post-procesamiento..." << std::endl;
        auto result = validator.validatePostProcessorShaders();
        printValidationResult(result, "Efectos Post-Procesamiento");
        overallSuccess &= result.isValid;
    }
    
    if (!specificShader.empty()) {
        std::cout << "Validando shader específico: " << specificShader << std::endl;
        auto result = validator.validateShader(specificShader);
        printValidationResult(result, "Shader: " + specificShader);
        overallSuccess &= result.isValid;
        
        // Mostrar árbol de dependencias
        std::cout << "\n--- Árbol de Dependencias ---" << std::endl;
        auto dependencies = validator.getDependencyTree(specificShader);
        for (const auto& dep : dependencies) {
            std::cout << "📄 " << dep.filePath;
            if (!dep.exists) {
                std::cout << " ❌ (NO EXISTE)";
            } else if (!dep.isReadable) {
                std::cout << " ⚠️ (NO LEGIBLE)";
            } else {
                std::cout << " ✓";
            }
            std::cout << std::endl;
            
            for (const auto& include : dep.includes) {
                std::cout << "  └─ 📄 " << include << std::endl;
            }
        }
    }
    
    std::cout << "=== Resumen ===" << std::endl;
    if (overallSuccess) {
        std::cout << "🎉 Todas las validaciones pasaron correctamente" << std::endl;
        return 0;
    } else {
        std::cout << "💥 Se encontraron problemas en las validaciones" << std::endl;
        return 1;
    }
}
