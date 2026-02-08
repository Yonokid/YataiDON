#!/bin/bash

# YataiDON Debug Build Script
# Builds the project in Debug mode with sanitizers and copies the executable to root

set -e  # Exit on error

echo "Building YataiDON (Debug)..."
echo ""

# Clean and build
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# Copy executable to root directory
if [ -f build/bin/YataiDON ]; then
    echo ""
    echo "Copying executable to root directory..."
    cp build/bin/YataiDON ./YataiDON
    chmod +x ./YataiDON
    echo "Build complete! Executable is ready at ./YataiDON"
else
    echo "Error: Executable not found at build/bin/YataiDON"
    exit 1
fi
