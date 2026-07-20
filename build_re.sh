#!/bin/bash

# YataiDON Fast Rebuild Script
# Rebuilds without cleaning and copies the executable to root

set -e  # Exit on error

echo "Building YataiDON (Rebuild)..."
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

# Generate per-skin checksums (used by updater for individual file updates)
echo ""
echo "Generating skin checksums..."
git submodule status | while IFS=' ' read -r commit path _; do
    skin_name=$(basename "$path")
    (
        cd "$path"
        find . -type f -not -path './.git/*' -not -name '.git' -not -name 'checksums.sha256' | sort | xargs sha256sum --binary
    ) > "$path/checksums.sha256"
    echo "  $path/checksums.sha256 written"
done
