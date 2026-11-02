#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
: "${PS5_PAYLOAD_SDK:?Set PS5_PAYLOAD_SDK}"
: "${OPUS_SOURCE:?Set OPUS_SOURCE to an unpacked Opus 1.5.2 source tree}"
prefix=${PS5_OPUS_PREFIX:-$root/ps5/vendor/opus-ps5}
build=${OPUS_BUILD_DIR:-$root/ps5/build/opus}
cmake -S "$OPUS_SOURCE" -B "$build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PS5_PAYLOAD_SDK/toolchain/prospero.cmake" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF \
  -DOPUS_BUILD_TESTING=OFF -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_INSTALL_PKG_CONFIG_MODULE=ON
cmake --build "$build" --parallel "${BUILD_JOBS:-4}"
cmake --install "$build"
