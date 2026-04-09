# Audio Visualizer en Tiempo Real

Visualizador de música en tiempo real en C++ reactivo al audio del micrófono, orientado a música electrónica.

## Arquitectura

El sistema se divide en cuatro subsistemas principales:

1. **Captura de Audio** - PortAudio para baja latencia
2. **Análisis de Señal** - FFT y extracción de features
3. **Motor Visual** - OpenGL con shaders reactivos
4. **Sincronización** - Pipeline DSP + render

## Características

- Captura de audio en tiempo real desde el micrófono
- Análisis FFT con extracción de bandas de frecuencia (bass, mid, high)
- Detección de onsets y beats
- Visualización con raymarching SDF reactivo al audio
- Shaders psicodélicos para música electrónica
- Smoothing de features para visualización fluida

## Dependencias

- **PortAudio** - Captura de audio
- **OpenGL** - Renderizado gráfico  
- **GLFW** - Ventana y contexto OpenGL
- **GLEW** - Carga de funciones OpenGL
- **CMake** - Sistema de construcción

## Compilación

### Ubuntu/Debian
```bash
# Instalar dependencias
sudo apt update
sudo apt install build-essential cmake
sudo apt install libportaudio-dev libgl1-mesa-dev libglu1-mesa-dev
sudo apt install libglfw3-dev libglew-dev pkg-config

# Compilar
mkdir build && cd build
cmake ..
make -j4

# Ejecutar
./audio_visualizer
```

### Arch Linux
```bash
# Instalar dependencias
sudo pacman -S base-devel cmake
sudo pacman -S portaudio glfw glew

# Compilar
mkdir build && cd build
cmake ..
make -j4

# Ejecutar
./audio_visualizer
```

## Uso

1. **Lista los dispositivos de audio disponibles:**
   ```bash
   ./audio_visualizer --list-devices
   # o
   ./audio_visualizer -l
   ```

2. **Ejecuta con el dispositivo por defecto:**
   ```bash
   ./audio_visualizer
   ```

3. **Ejecuta con un dispositivo específico:**
   ```bash
   ./audio_visualizer --device 8
   # o
   ./audio_visualizer -d 8
   ```

4. **Muestra ayuda:**
   ```bash
   ./audio_visualizer --help
   # o
   ./audio_visualizer -h
   ```

### Dispositivos Comunes

- **Dispositivo 4**: HD-Audio Generic: ALC233 Analog (micrófono integrado) [DEFAULT]
- **Dispositivo 7**: pipewire (sistema de audio moderno)
- **Dispositivo 8**: pulse (PulseAudio)
- **Dispositivo 6**: jack (Jack Audio Connection Kit)

Selecciona el dispositivo que tenga canales de entrada (Max Inputs > 0) y que corresponda a tu hardware.

## Features de Audio Extraídos

- **Energy** - Intensidad general del audio
- **Bass (20-120 Hz)** - Reacciona a los kicks
- **Mid (120-2000 Hz)** - Sintetizadores y voces
- **High (2k-12k Hz)** - Hats y agudos
- **Onset Detection** - Detección de golpes
- **Beat Detection** - Detección de tempo

## Sistema Visual

El visualizador usa **raymarching SDF** en shaders GLSL para crear geometría procedural que reacciona al audio:

- Esferas y cajas que pulsan con el bass
- Deformaciones basadas en las frecuencias medias
- Detalles fractales con alta energía
- Iluminación psicodélica reactiva
- Efectos de post-procesamiento para estética rave

## Arquitectura del Pipeline

```
Audio Thread (PortAudio)
    └── Captura samples (512 frames)
        └── Aplica ventana (Hann)
            └── FFT (1024 puntos)
                └── Extrae features
                    └── Smooth (EMA)

Main Thread
    └── Actualiza uniforms del shader
        └── Render frame (OpenGL)
            └── Raymarching SDF reactivo
```

## Personalización

Para modificar los visuales, edita el fragment shader en `src/visualizer.cpp`. Las uniforms disponibles son:

- `uTime` - Tiempo transcurrido
- `uBass` - Energía de bajos
- `uMid` - Energía de medios  
- `uHigh` - Energía de agudos
- `uEnergy` - Energía total
- `uOnset` - Detección de golpes
- `uBeat` - Detección de tempo
- `uResolution` - Resolución de pantalla

## Cómo añadir un nuevo shader procedural

1. **Crear/editar el shader GLSL**
   - Trabaja dentro de `shaders/` (puedes usar un `procedural_packXX.glsl` existente o crear uno nuevo).
   - Declara tu función `vec4 renderMiShader(vec2 st, float time, float tempo, float energy, float bass, float mid, float high)` y retorna un color RGBA.

2. **Registrar el efecto con `@EFFECT`**
   - Añade un comentario en el shader con el formato:
     ```glsl
     // @EFFECT name="Mi Shader" index=50 desc="Breve descripción" author="Tu Nombre" [zoom=1.2]
     ```
   - El `index` debe ser único y corresponderá al `uMode`. Mantén la numeración continua (actualmente van del 0 al 49).

3. **Actualizar los include lists**
   - Si creaste un archivo nuevo, agrégalo en `src/modular_layer.cpp` dentro de `kProceduralShaderFiles` (y en `kProceduralPackFiles` si forma parte de un paquete).

4. **Despachar el modo en `procedural_main.glsl`**
   - Dentro de `shaders/procedural_main.glsl`, agrega un `else if (uMode == X) { color = renderMiShader(...); }` para tu índice.

5. **Incluir la función en los paquetes**
   - Si usaste un `procedural_packXX.glsl` nuevo, asegúrate de que el archivo esté listado en la constante `kProceduralShaderFiles` para que el `EffectRegistry` lo escanee.

6. **Sincronizar el límite de modos**
   - Si agregas un índice nuevo, verifica que `kProceduralModeCount` en `visualizer.h` cubra el rango (ej. 50 modos ⇒ valor 50).

7. **Recompilar y ejecutar**
   - Usa el script `./run_visualizer.sh` (o `cmake && make`) para copiar shaders, compilar y lanzar el binario.
   - Tu shader aparecerá en la UI con el nombre declarado en `@EFFECT` y también podrá usarse en el randomizer.

## Rendimiento

- Latencia de audio: ~10-20ms
- FPS objetivo: 60
- Sample rate: 44.1 kHz
- Buffer size: 512 samples
- FFT size: 1024 puntos

## Troubleshooting

**No hay audio:**
- Verifica permisos del micrófono
- Asegúrate que PortAudio detecta el dispositivo

**Error de OpenGL:**
- Actualiza drivers gráficos
- Verifica compatibilidad con OpenGL 3.3+

**Compilación falla:**
- Instala todas las dependencias
- Verifica versiones de CMake (>=3.10)

## Licencia

MIT License - Libre para modificar y distribuir
