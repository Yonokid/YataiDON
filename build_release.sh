#!/bin/bash

# YataiDON Release Build Script
# Builds the project in Release mode and copies the executable to root

set -e  # Exit on error

# --bgfx: use the bgfx graphics backend instead of raylib
backend=raylib
[ "$1" = "--bgfx" ] && backend=bgfx

echo "Building YataiDON (Release, $backend)..."
echo ""

# Clean and build
rm -rf build
find .cmake-deps -maxdepth 1 -name '*-subbuild' -type d -exec rm -rf {} + 2>/dev/null || true
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DYATAIDON_GFX_BACKEND="$backend" -G Ninja \
  -DYATAIDON_STATIC_DEPS="${YATAIDON_STATIC_DEPS:-OFF}" \
  ${YATAIDON_FFMPEG_PREFIX:+-DYATAIDON_FFMPEG_PREFIX="$YATAIDON_FFMPEG_PREFIX"} \
  $(command -v ccache &>/dev/null && echo "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
cmake --build build -j"${JOBS:-$(( $(nproc) / 2 ))}"

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
for path in Skins/*/; do
    path=${path%/}
    skin_name=$(basename "$path")
    (
        cd "$path"
        find . -type f -not -path './.git/*' -not -name '.git' -not -name 'checksums.sha256' | sort | xargs sha256sum --binary
    ) > "$path/checksums.sha256"
    echo "  $path/checksums.sha256 written"
done
