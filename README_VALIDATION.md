# Sistema de Validación de Capas

Este proyecto incluye un sistema completo de validación para verificar que todas las capas del visualizador estén correctamente configuradas antes de compilar.

## Componentes del Sistema

### 1. LayerValidator (`src/layer_validator.h/cpp`)
Clase principal que proporciona validación integral de:
- **Shaders**: Sintaxis GLSL, existencia de archivos, dependencias
- **Dependencias**: Análisis recursivo de `#include`
- **Configuración**: Validación de capas modulares y post-procesamiento
- **Integración**: Compatibilidad entre componentes

### 2. Utilidad de Línea de Comandos (`src/validate_layers.cpp`)
Herramienta para validación manual:
```bash
# Validar todo el sistema
./validate_layers --all

# Validar shader específico
./validate_layers --shader procedural_main.glsl

# Validar solo componentes específicos
./validate_layers --procedural
./validate_layers --post-effects

# Modo detallado
./validate_layers --verbose --all
```

### 3. Tests de Integración (`src/test_layer_integration.cpp`)
Suite de pruebas automáticas que verifica:
- Inicialización de capas modulares
- Post-procesador y efectos
- Cadenas de efectos
- Gestión de recursos y memoria

## Uso en el Sistema de Compilación

### Integración con CMake
El sistema está integrado automáticamente en el proceso de compilación:

1. **Validación Pre-compilación**: Se ejecuta automáticamente antes de compilar el ejecutable principal
2. **Dependencias**: El ejecutable principal depende de la validación exitosa
3. **Reporte**: Errores detienen la compilación con mensajes descriptivos

### Comandos de Compilación
```bash
# Compilación normal (con validación automática)
make

# Compilación sin validación (si es necesario)
make audio_visualizer

# Ejecutar tests de integración
make test_layer_integration
./test_layer_integration

# Validación manual
make validate_layers
./validate_layers --all
```

## Tipos de Validación

### 1. Validación de Shaders
- ✅ Existencia y legibilidad de archivos
- ✅ Sintaxis GLSL básica
- ✅ Directivas `#version`
- ✅ Funciones `main` presentes
- ✅ Balance de llaves

### 2. Validación de Dependencias
- ✅ Análisis de directivas `#include`
- ✅ Búsqueda recursiva de dependencias
- ✅ Verificación de existencia de archivos incluidos
- ✅ Detección de referencias circulares

### 3. Validación de Capas Modulares
- ✅ Shaders procedurales críticos
- ✅ Packs de shaders (1-10)
- ✅ Configuración de modos
- ✅ Recursos OpenGL necesarios

### 4. Validación de Post-Procesamiento
- ✅ Shader común (`post_common.glsl`)
- ✅ Efectos individuales
- ✅ Cadenas de efectos válidas
- ✅ Compatibilidad entre modos

## Reporte de Errores

### Formato de Salida
```
=== Validación de Shader: procedural_main.glsl ===
✗ VALIDACIÓN FALLIDA

Errores (2):
  ❌ Shader file no encontrado: procedural_main.glsl
  ❌ Include no encontrado: helpers.glsl (requerido por procedural_main.glsl)

Advertencias (1):
  ⚠️  Shader sin directiva #version
```

### Códigos de Salida
- `0`: Validación exitosa
- `1`: Errores encontrados
- `2`: Error crítico del sistema

## Configuración

### Rutas de Búsqueda
Por defecto, el sistema busca shaders en:
- `shaders/`
- `shaders/post_effects/`
- `shaders/procedural/`
- `build/shaders/`
- `build/shaders/post_effects/`
- `build/shaders/procedural/`

### Variables de Entorno
- `PROCEDURAL_DEBUG_PACK`: Para depuración de shaders procedurales específicos

## Flujo de Trabajo Recomendado

### 1. Desarrollo Normal
```bash
# Editar shaders
vim shaders/procedural_main.glsl

# Validar cambios
./validate_layers --shader procedural_main.glsl

# Compilar si es válido
make
```

### 2. Integración Continua
```bash
# Validación completa
./validate_layers --all

# Tests de integración
./test_layer_integration

# Compilación final
make
```

### 3. Depuración
```bash
# Modo detallado
./validate_layers --verbose --shader problematic_shader.glsl

# Ver árbol de dependencias
./validate_layers --shader complex_shader.glsl
```

## Extensión del Sistema

### Añadir Nuevas Validaciones
Para añadir nuevos tipos de validación:

1. **Extender `LayerValidator`**: Añadir métodos públicos
2. **Actualizar CLI**: Añadir opciones en `validate_layers.cpp`
3. **Añadir Tests**: Incluir en `test_layer_integration.cpp`
4. **Actualizar CMake**: Si es necesario, añadir nuevos targets

### Validaciones Personalizadas
```cpp
// Ejemplo: Validar uniforms específicos
ValidationResult validateRequiredUniforms(const std::string& shaderPath,
                                         const std::vector<std::string>& requiredUniforms);
```

## Solución de Problemas Comunes

### "Shader no encontrado"
- Verificar rutas de búsqueda
- Comprobar que los archivos existan en `shaders/`
- Ejecutar desde el directorio correcto

### "Error de sintaxis GLSL"
- Validar sintaxis manualmente
- Verificar directivas `#version`
- Comprobar balance de llaves

### "Include no encontrado"
- Verificar nombres de archivos en `#include`
- Comprobar rutas relativas
- Asegurar que los archivos incluidos existan

## Rendimiento

- **Validación rápida**: < 1 segundo para sistema completo
- **Memoria**: < 50MB para validaciones complejas
- **Impacto en compilación**: ~2-3 segundos adicionales

Este sistema garantiza que todos los componentes del visualizador estén correctamente configurados antes de la compilación, reduciendo significativamente errores de runtime y problemas de integración.
