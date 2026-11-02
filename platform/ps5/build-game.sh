#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
: "${PS5_PAYLOAD_SDK:?Set PS5_PAYLOAD_SDK}"
: "${PS5_FFMPEG_PREFIX:?Set PS5_FFMPEG_PREFIX}"
: "${PS5_VULKAN_PREFIX:?Set PS5_VULKAN_PREFIX to PS5 Vulkan radv-release}"
bgfx="$root/platform/ps5/vendor/bgfx.cmake"
bash "$root/platform/ps5/prepare.sh"
# Build shaderc for the host, never attempt to run a console executable here.
host_build=${YATAIDON_HOST_BGFX_BUILD:-$root/platform/ps5/build-host-bgfx}
cmake -S "$bgfx" -B "$host_build" -G Ninja -DCMAKE_BUILD_TYPE=Release   -DYATAIDON_BGFX_TOOLS_ONLY=ON -DBGFX_WITH_WAYLAND=OFF -DBGFX_BUILD_EXAMPLES=OFF -DBGFX_BUILD_TESTS=OFF   -DBGFX_BUILD_TOOLS=ON -DBGFX_BUILD_TOOLS_TEXTURE=OFF -DBGFX_BUILD_TOOLS_GEOMETRY=OFF -DBGFX_BUILD_TOOLS_BIN2C=OFF
cmake --build "$host_build" --target shaderc --parallel "${BUILD_JOBS:-4}"
shaderc="$host_build/cmake/bgfx/shaderc"
cmake -S "$root/platform/ps5" -B "$root/platform/ps5/build-vulkan" -G Ninja   -DCMAKE_TOOLCHAIN_FILE="$PS5_PAYLOAD_SDK/toolchain/prospero.cmake"   -DCMAKE_BUILD_TYPE=Release -DPS5_FFMPEG_PREFIX="$PS5_FFMPEG_PREFIX"   -DPS5_VULKAN_PREFIX="$PS5_VULKAN_PREFIX" -DYATAIDON_SHADERC="$shaderc"
cmake --build "$root/platform/ps5/build-vulkan" --target YataiDON-PS5 --parallel "${BUILD_JOBS:-4}"
