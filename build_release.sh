#!/bin/bash

# YataiDON Release Build Script
# Builds the project in Release mode and copies the executable to root

set -e  # Exit on error

echo "Building YataiDON (Release)..."
echo ""

# Clean and build
rm -rf build
find .cmake-deps -maxdepth 1 -name '*-subbuild' -type d -exec rm -rf {} + 2>/dev/null || true
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G Ninja \
  $(command -v ccache &>/dev/null && echo "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
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

# Generate checksums for all shipped files
echo ""
echo "Generating checksums.sha256..."
{
    sha256sum YataiDON
    find shader -type f | sort | xargs sha256sum
    find Skins -type f -not -path '*/.git/*' | sort | xargs sha256sum
} > checksums.sha256
echo "checksums.sha256 written ($(wc -l < checksums.sha256) files)"
