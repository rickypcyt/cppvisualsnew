#!/bin/bash

# Script to build and run the audio visualizer with NVIDIA Prime support
echo "Building Audio Visualizer..."

# Build the project
if [ -d "build" ]; then
    cd build
    if make; then
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

echo "Starting Audio Visualizer with Prime Run..."

# Check if prime-run is available
if command -v prime-run &> /dev/null; then
    echo "Using prime-run for NVIDIA GPU acceleration"
    cd build
    prime-run ./audio_visualizer "$@"
    cd ..
else
    echo "prime-run not found, running normally"
    cd build
    ./audio_visualizer "$@"
    cd ..
fi
