#!/bin/bash
# Script para lanzar el visualizador con selección de GPU
# Uso: ./run_with_gpu.sh [amd|nvidia|auto]

GPU_CHOICE=${1:-auto}

case $GPU_CHOICE in
    amd)
        echo "Usando GPU AMD (forzando card2)"
        # Forzar uso directo de /dev/dri/card2 (AMD)
        unset __NV_PRIME_RENDER_OFFLOAD
        unset __GLX_VENDOR_LIBRARY_NAME
        export MESA_LOADER_DRIVER_OVERRIDE=amdgpu
        export DRI_PRIME=1
        export EGL_PLATFORM=x11
        ;;
    nvidia)
        echo "Usando GPU NVIDIA (activando NVIDIA PRIME)"
        # Activar NVIDIA PRIME
        export __NV_PRIME_RENDER_OFFLOAD=1
        export __GLX_VENDOR_LIBRARY_NAME=nvidia
        unset MESA_LOADER_DRIVER_OVERRIDE
        unset DRI_PRIME
        ;;
    auto)
        echo "Usando GPU automática (por defecto del sistema)"
        unset __NV_PRIME_RENDER_OFFLOAD
        unset __GLX_VENDOR_LIBRARY_NAME
        unset MESA_LOADER_DRIVER_OVERRIDE
        unset DRI_PRIME
        ;;
    *)
        echo "Uso: $0 [amd|nvidia|auto]"
        echo "  amd   - Forzar uso de GPU AMD (iGPU)"
        echo "  nvidia - Forzar uso de GPU NVIDIA (dGPU)"
        echo "  auto  - Usar GPU por defecto del sistema"
        exit 1
        ;;
esac

echo "Variables de entorno OpenGL:"
env | grep -E "(DRI_PRIME|MESA_LOADER|__NV_|EGL)" | sort

# Build the project
if [ -d "build" ]; then
    cd build
    if cmake --build . -j$(nproc); then
        echo "Build successful!"
    else
        echo "Build failed!"
        exit 1
    fi
    cd ..
else
    echo "Build directory not found!"
    exit 1
fi

./build/audio_visualizer
