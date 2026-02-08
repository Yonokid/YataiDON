#!/bin/bash

# YataiDON Fast Debug Build Script
# Rebuilds without cleaning and copies the executable to root

set -e  # Exit on error

echo "Building YataiDON (Fast Debug Build)..."
echo ""

# Build without cleaning
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
