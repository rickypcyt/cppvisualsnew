# PowerShell build script for Windows
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Audio Visualizer - Windows Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if vcpkg is installed
if (-not (Test-Path "vcpkg")) {
    Write-Host "vcpkg not found. Please install vcpkg first:" -ForegroundColor Red
    Write-Host "1. Clone vcpkg: git clone https://github.com/Microsoft/vcpkg" -ForegroundColor Yellow
    Write-Host "2. Run bootstrap: .\vcpkg\bootstrap-vcpkg.bat" -ForegroundColor Yellow
    Write-Host "3. Integrate: .\vcpkg\vcpkg integrate install" -ForegroundColor Yellow
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

# Install dependencies via vcpkg
Write-Host "Installing dependencies with vcpkg..." -ForegroundColor Green
& .\vcpkg\vcpkg install glfw3 glew portaudio nlohmann-json --triplet x64-windows

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to install dependencies" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

# Create build directory
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}
Set-Location build

# Configure with CMake using vcpkg toolchain
Write-Host "Configuring with CMake..." -ForegroundColor Green
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -A x64

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed" -ForegroundColor Red
    Set-Location ..
    Read-Host "Press Enter to exit"
    exit 1
}

# Build
Write-Host "Building..." -ForegroundColor Green
cmake --build . --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed" -ForegroundColor Red
    Set-Location ..
    Read-Host "Press Enter to exit"
    exit 1
}

Set-Location ..

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Executable location: build\Release\audio_visualizer.exe" -ForegroundColor Yellow
Write-Host ""
Write-Host "To run:" -ForegroundColor Yellow
Write-Host "cd build\Release" -ForegroundColor Yellow
Write-Host ".\audio_visualizer.exe" -ForegroundColor Yellow
Write-Host ""
Read-Host "Press Enter to exit"
