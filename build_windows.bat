@echo off
REM Build script for Windows using vcpkg

echo ========================================
echo Audio Visualizer - Windows Build Script
echo ========================================
echo.

REM Check if vcpkg is installed
if not exist "vcpkg" (
    echo vcpkg not found. Please install vcpkg first:
    echo 1. Clone vcpkg: git clone https://github.com/Microsoft/vcpkg
    echo 2. Run bootstrap: .\vcpkg\bootstrap-vcpkg.bat
    echo 3. Integrate: .\vcpkg\vcpkg integrate install
    echo.
    pause
    exit /b 1
)

REM Install dependencies via vcpkg
echo Installing dependencies with vcpkg...
call vcpkg\vcpkg install glfw3 glew portaudio nlohmann-json --triplet x64-windows

if %errorlevel% neq 0 (
    echo Failed to install dependencies
    pause
    exit /b 1
)

REM Create build directory
if not exist "build" mkdir build
cd build

REM Configure with CMake using vcpkg toolchain
echo Configuring with CMake...
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -A x64

if %errorlevel% neq 0 (
    echo CMake configuration failed
    cd ..
    pause
    exit /b 1
)

REM Build
echo Building...
cmake --build . --config Release

if %errorlevel% neq 0 (
    echo Build failed
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo Executable location: build\Release\audio_visualizer.exe
echo.
echo To run:
echo cd build\Release
echo audio_visualizer.exe
echo.
pause
