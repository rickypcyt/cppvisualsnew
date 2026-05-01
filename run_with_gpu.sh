#!/bin/bash

# Script to build and run the audio visualizer with AMD GPU support
echo "Building Audio Visualizer..."

# Force copy shaders from source to build directory BEFORE compiling
echo "Copying shaders from source to build directory..."
if [ -d "shaders" ] && [ -d "build" ]; then
    rm -rf build/shaders
    cp -r shaders build/shaders
    echo "Shaders copied successfully!"
fi

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

echo "Starting Audio Visualizer with AMD GPU..."

# Use DRI_PRIME=1 to force AMD GPU on hybrid systems
cd build
DRI_PRIME=1 ./audio_visualizer "$@"
cd ..
