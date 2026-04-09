# Guía: Añadir Nuevos Procedural Layers

Esta guía documenta el proceso completo para añadir nuevos shaders procedurales al visualizador.

## Archivos a Modificar

### 1. Crear el Shader (`shaders/procedural_pack{N}.glsl`)

**Formato requerido:**
```glsl
// @EFFECT name="Nombre del Efecto" index=X desc="Descripción" author="Autor"

vec4 renderNombreFuncion(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    // Tu código shader aquí
    // Usa uPrimaryColor y uSecondaryColor para colores reactivos
    return vec4(color, alpha);
}
```

**Reglas importantes:**
- El `index=X` debe ser único y consecutivo (verificar qué índices existen)
- La función debe empezar con `render` y retornar `vec4`
- Parámetros obligatorios: `(vec2 st, float time, float tempo, float energy, float bass, float mid, float high)`
- Usar uniforms del proyecto: `uPrimaryColor`, `uSecondaryColor`, `uColorBlend`

### 2. Registrar el Shader (`src/modular_layer.cpp`)

Añadir el archivo a **dos arrays**:

**Array 1 - `kProceduralShaderFiles`:**
```cpp
const std::array<const char*, N> kProceduralShaderFiles = {
    // ... shaders existentes
    "procedural_pack23.glsl",  // Añadir aquí
    "procedural_main.glsl"
};
```
- Incrementar el número de elementos (`N`)
- Orden no importa, pero mantener consistencia

**Array 2 - `kProceduralPackFiles`:**
```cpp
const std::array<const char*, M> kProceduralPackFiles = {
    // ... packs existentes
    "procedural_pack23.glsl"  // Añadir aquí
};
```
- Incrementar el número de elementos (`M`)
- Esto permite la carga dinámica del pack

### 3. Añadir al Switch Principal (`shaders/procedural_main.glsl`)

```glsl
} else if (uMode == X) {
    color = renderNombreFuncion(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
} else {
```
- El número `X` debe coincidir con el `index=X` del shader
- Colocar antes del `else` final

### 4. Actualizar Límites (`src/settings_manager.cpp`)

Buscar todos los `std::clamp` relacionados con `proceduralLayerMode_` y actualizar el máximo:

```cpp
// Buscar todos los lugares con:
proceduralLayerMode_ = std::clamp(loadedMode, 0, 46);

// Cambiar a (nuevo máximo):
proceduralLayerMode_ = std::clamp(loadedMode, 0, 48);
```

**Ubicaciones típicas:**
- Línea ~69: `ui["proceduralLayerMode"]`
- Línea ~118: `proceduralSlots_[i].mode` (fallback)
- Línea ~124: `proceduralSlots_[i].mode` (legacy)
- Línea ~151: fallback en carga por nombre
- Línea ~156: carga legacy del main layer

## Verificación

1. **Índice único:** Verificar que `index=X` no esté usado en otro shader
2. **Función registrada:** Confirmar que la función `renderXXX` está en el switch
3. **Compilación:** Recompilar y verificar que no hay errores de shader
4. **UI:** Comprobar que aparece en el combo de IMGUI
5. **JSON:** Verificar que se guarda/carga correctamente en settings

## Troubleshooting

| Síntoma | Causa probable | Solución |
|---------|----------------|----------|
| No aparece en UI | No añadido a `kProceduralShaderFiles` | Revisar paso 2 |
| Mismo shader en múltiples efectos | No añadido al switch en `procedural_main.glsl` | Revisar paso 3 |
| No se guarda en JSON | Límite de clamp muy bajo | Revisar paso 4 |
| Error de compilación de shader | Uniform incorrecto | Verificar uso de `uStrength` vs `uIntensity`, etc. |

## Ejemplo Completo

Añadir efecto "Nuevo Efecto" con índice 48:

```cpp
// 1. Crear shaders/procedural_pack23.glsl
// @EFFECT name="Nuevo Efecto" index=48 desc="Descripción" author="Yo"
vec4 renderNuevoEfecto(vec2 st, float time, float tempo, float energy, float bass, float mid, float high) {
    vec3 c = uPrimaryColor * energy;
    return vec4(c, 1.0);
}

// 2. Añadir a kProceduralShaderFiles y kProceduralPackFiles en modular_layer.cpp

// 3. Añadir case en procedural_main.glsl
} else if (uMode == 48) {
    color = renderNuevoEfecto(st, uTime, uTempo, uEnergy, uBass, uMid, uHigh);
} else {

// 4. Cambiar clamp de 46 a 48 en settings_manager.cpp (4 lugares)
```

## Notas para IAs

- Siempre verificar el índice máximo actual antes de añadir
- Los índices deben ser consecutivos y únicos
- El nombre de la función `renderXXX` debe coincidir exactamente en todos los archivos
- No olvidar incrementar los números de los arrays (`const std::array<const char*, N>`)
