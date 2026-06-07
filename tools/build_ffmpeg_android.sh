#!/usr/bin/env bash
# Cross-compile FFmpeg for Android arm64-v8a using the NDK.
# Output: $OUT_DIR with include/ and lib/ ready for ANDROID_FFMPEG_PREFIX.
#
# Usage:
#   export ANDROID_NDK_HOME=/path/to/ndk
#   bash tools/build_ffmpeg_android.sh
#
# Or override output dir:
#   OUT_DIR=/some/path bash tools/build_ffmpeg_android.sh

set -euo pipefail

NDK="${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME}"
API=29
ABI=arm64-v8a
ARCH=aarch64
TRIPLE=aarch64-linux-android
TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
SYSROOT="$TOOLCHAIN/sysroot"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="${OUT_DIR:-$REPO_ROOT/.android-ffmpeg}"

FFMPEG_VERSION="7.1"
FFMPEG_URL="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2"

BUILD_DIR="$(mktemp -d /tmp/ffmpeg-android-build.XXXXXX)"
trap 'rm -rf "$BUILD_DIR"' EXIT

echo "==> Downloading FFmpeg ${FFMPEG_VERSION}..."
curl -L "$FFMPEG_URL" | tar -xj -C "$BUILD_DIR"

cd "$BUILD_DIR/ffmpeg-${FFMPEG_VERSION}"

export CC="$TOOLCHAIN/bin/${TRIPLE}${API}-clang"
export CXX="$TOOLCHAIN/bin/${TRIPLE}${API}-clang++"
export AR="$TOOLCHAIN/bin/llvm-ar"
export AS="$CC"
export NM="$TOOLCHAIN/bin/llvm-nm"
export RANLIB="$TOOLCHAIN/bin/llvm-ranlib"
export STRIP="$TOOLCHAIN/bin/llvm-strip"
export LD="$TOOLCHAIN/bin/ld.lld"

echo "==> Configuring FFmpeg for ${ABI} (API ${API})..."
./configure \
    --prefix="$OUT_DIR" \
    --target-os=android \
    --arch=${ARCH} \
    --cpu=armv8-a \
    --cross-prefix="$TOOLCHAIN/bin/${TRIPLE}-" \
    --sysroot="$SYSROOT" \
    --cc="$CC" \
    --cxx="$CXX" \
    --ar="$AR" \
    --as="$CC" \
    --nm="$NM" \
    --ranlib="$RANLIB" \
    --strip="$STRIP" \
    --extra-cflags="--target=${TRIPLE}${API} --sysroot=${SYSROOT}" \
    --extra-ldflags="--target=${TRIPLE}${API} --sysroot=${SYSROOT}" \
    --enable-static \
    --disable-shared \
    --enable-pic \
    --disable-asm \
    --disable-programs \
    --disable-doc \
    --disable-avdevice \
    --disable-postproc \
    --disable-network \
    --disable-debug \
    --enable-avformat \
    --enable-avcodec \
    --enable-avutil \
    --enable-swscale \
    --enable-swresample \
    --enable-demuxer=mp3,ogg,flac,wav,aac,mov,matroska \
    --enable-decoder=mp3,vorbis,flac,pcm_s16le,pcm_s16be,aac,opus \
    --enable-parser=mp3,flac,vorbis,aac,opus \
    --enable-protocol=file \
    --disable-vulkan \
    --disable-vaapi \
    --disable-vdpau \
    --disable-v4l2-m2m

echo "==> Building FFmpeg (this takes a few minutes)..."
make -j"$(nproc)"

echo "==> Installing to ${OUT_DIR}..."
make install

echo ""
echo "Done. Set this for the Android cmake configure:"
echo "  cmake --preset android-arm64 -DANDROID_FFMPEG_PREFIX=${OUT_DIR}"
