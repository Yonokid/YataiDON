#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
: "${DEVKITPRO:?Set DEVKITPRO to your devkitPro installation with ounce-devkit installed}"
: "${YATAIDON_SHADERC:?Set YATAIDON_SHADERC to matching host bgfx shaderc}"
python3 "$root/platform/switch/prepare-game.py"
cmake -S "$root/platform/switch2" -B "$root/platform/switch2/build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/libounce/cmake/Switch2.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DYATAIDON_SHADERC="$YATAIDON_SHADERC" \
  -DSWITCH_MESA_PREFIX="${SWITCH_MESA_PREFIX:-$DEVKITPRO/portlibs/switch2}" \
  -DYATAIDON_MESA_LIBRARY="${YATAIDON_MESA_LIBRARY:-}" \
  -DYATAIDON_MEDIA_LIBRARY_DIR="${YATAIDON_MEDIA_LIBRARY_DIR:-}"
cmake --build "$root/platform/switch2/build" --parallel "${BUILD_JOBS:-4}"
