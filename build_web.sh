#!/usr/bin/env bash
set -e

EMSDK="${EMSDK:-$HOME/Documents/GitHub/emsdk}"
export PATH="$EMSDK:$EMSDK/upstream/emscripten:$PATH"

BUILD_DIR="build-em"

emcmake cmake -S . -B "$BUILD_DIR" -G Ninja
cmake --build "$BUILD_DIR" -- -j$(nproc)

cp "$BUILD_DIR/bin/YataiDON.html" "$BUILD_DIR/bin/YataiDON.js" \
   "$BUILD_DIR/bin/YataiDON.wasm" "$BUILD_DIR/bin/YataiDON.data" .

echo "Build complete: YataiDON.html"
