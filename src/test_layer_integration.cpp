#include "layer_validator.h"
#include "modular_layer.h"
#include "post_processor.h"
#include "shader_loader.h"

#include <iostream>
#include <vector>
#include <memory>

class LayerIntegrationTester {
public:
    LayerIntegrationTester() : validator_(std::make_unique<LayerValidator>()) {
        validator_->enableVerboseMode(true);
    }
    
    bool runAllTests() {
        std::cout << "=== Iniciando Tests de Integración de Capas ===" << std::endl;
        
        bool allPassed = true;
        
        allPassed &= testShaderDependencies();
        allPassed &= testModularLayers();
        allPassed &= testPostProcessor();
        allPassed &= testEffectChains();
        allPassed &= testResourceManagement();
        
        std::cout << "\n=== Resultados ===" << std::endl;
        std::cout << "Tests " << (allPassed ? "APROBADOS" : "REPROBADOS") << std::endl;
        
        return allPassed;
    }

private:
    std::unique_ptr<LayerValidator> validator_;
    
    bool testShaderDependencies() {
        std::cout << "\n--- Test: Dependencias de Shaders ---" << std::endl;
        
        bool passed = true;
        
        // Test shaders procedurales
        const std::vector<std::string> proceduralShaders = {
            "procedural_header.glsl",
            "procedural_helpers.glsl",
            "procedural_main.glsl"
        };
        
        for (const auto& shader : proceduralShaders) {
            auto result = validator_->validateShaderDependencies(shader);
            if (!result.isValid) {
                std::cerr << "ERROR en dependencias de " << shader << ":" << std::endl;
                for (const auto& error : result.errors) {
                    std::cerr << "  - " << error << std::endl;
                }
                passed = false;
            } else {
                std::cout << "✓ " << shader << " - dependencias OK" << std::endl;
            }
        }
        
        return passed;
    }
    
    bool testModularLayers() {
        std::cout << "\n--- Test: Capas Modulares ---" << std::endl;
        
        bool passed = true;
        
        // Test inicialización de capas
        auto layer = std::make_unique<ModularLayer>();
        if (!layer->initialize(1920, 1080)) {
            std::cerr << "ERROR: No se pudo inicializar capa modular" << std::endl;
            passed = false;
        } else {
            std::cout << "✓ Capa modular inicializada correctamente" << std::endl;
            
            // Test diferentes modos
            for (int mode = 0; mode < 10; ++mode) {
                layer->setMode(mode);
                LayerContext context{};
                context.screenWidth = 1920;
                context.screenHeight = 1080;
                context.time = 0.0f;
                context.tempo = 120.0f;
                context.audio = nullptr;
                context.intensity = 0.5f;
                
                try {
                    layer->render(context);
                    std::cout << "✓ Modo " << mode << " renderizado correctamente" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "ERROR en modo " << mode << ": " << e.what() << std::endl;
                    passed = false;
                }
            }
            
            layer->shutdown();
        }
        
        return passed;
    }
    
    bool testPostProcessor() {
        std::cout << "\n--- Test: Post-Procesador ---" << std::endl;
        
        bool passed = true;
        
        auto postProcessor = std::make_unique<PostProcessor>();
        if (!postProcessor->initialize(1920, 1080)) {
            std::cerr << "ERROR: No se pudo inicializar post-procesador" << std::endl;
            passed = false;
        } else {
            std::cout << "✓ Post-procesador inicializado correctamente" << std::endl;
            
            // Test efectos individuales
            std::vector<int> testEffects = {0, 1, 2, 3, 4, 5, 6, 7};
            for (int effect : testEffects) {
                try {
                    postProcessor->apply(effect, 0.5f, 0.0f, {1.0f, 1.0f, 1.0f}, 0.0f);
                    std::cout << "✓ Efecto " << effect << " aplicado correctamente" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "ERROR en efecto " << effect << ": " << e.what() << std::endl;
                    passed = false;
                }
            }
            
            postProcessor->shutdown();
        }
        
        return passed;
    }
    
    bool testEffectChains() {
        std::cout << "\n--- Test: Cadenas de Efectos ---" << std::endl;
        
        bool passed = true;
        
        // Test cadenas válidas
        std::vector<std::vector<int>> validChains = {
            {0},  // Solo passthrough
            {1, 0},  // Grayscale + passthrough
            {2, 3, 0},  // Múltiples efectos
            {4, 5, 6, 0}  // Cadena larga
        };
        
        for (const auto& chain : validChains) {
            auto result = validator_->validateEffectChain(chain);
            if (!result.isValid) {
                std::cerr << "ERROR en cadena de efectos válida:" << std::endl;
                for (const auto& error : result.errors) {
                    std::cerr << "  - " << error << std::endl;
                }
                passed = false;
            } else {
                std::cout << "✓ Cadena de efectos válida: ";
                for (int effect : chain) std::cout << effect << " ";
                std::cout << std::endl;
            }
        }
        
        // Test cadenas inválidas
        std::vector<std::vector<int>> invalidChains = {
            {-1},  // Modo negativo
            {100},  // Modo fuera de rango
            {0, 1, 0}  // Passthrough en medio
        };
        
        for (const auto& chain : invalidChains) {
            auto result = validator_->validateEffectChain(chain);
            if (result.isValid) {
                std::cerr << "ERROR: Cadena inválida pasó validación: ";
                for (int effect : chain) std::cout << effect << " ";
                std::cout << std::endl;
                passed = false;
            } else {
                std::cout << "✓ Cadena inválida detectada correctamente: ";
                for (int effect : chain) std::cout << effect << " ";
                std::cout << std::endl;
            }
        }
        
        return passed;
    }
    
    bool testResourceManagement() {
        std::cout << "\n--- Test: Gestión de Recursos ---" << std::endl;
        
        bool passed = true;
        
        // Test múltiples inicializaciones y shutdowns
        for (int i = 0; i < 3; ++i) {
            auto layer = std::make_unique<ModularLayer>();
            if (!layer->initialize(800, 600)) {
                std::cerr << "ERROR: Inicialización " << i << " fallida" << std::endl;
                passed = false;
                continue;
            }
            
            // Test resize
            layer->resize(1920, 1080);
            layer->resize(1280, 720);
            
            layer->shutdown();
            std::cout << "✓ Ciclo de recursos " << i << " completado" << std::endl;
        }
        
        // Test gestión de memoria
        std::vector<std::unique_ptr<ModularLayer>> layers;
        for (int i = 0; i < 10; ++i) {
            auto layer = std::make_unique<ModularLayer>();
            if (layer->initialize(640, 480)) {
                layers.push_back(std::move(layer));
            }
        }
        
        std::cout << "✓ Creadas " << layers.size() << " capas simultáneamente" << std::endl;
        
        // Limpieza automática al salir del scope
        layers.clear();
        std::cout << "✓ Memoria liberada correctamente" << std::endl;
        
        return passed;
    }
};

int main() {
    try {
        LayerIntegrationTester tester;
        bool success = tester.runAllTests();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "ERROR CRÍTICO: " << e.what() << std::endl;
        return 1;
    }
}
