#!/usr/bin/env bash
# Build static FFmpeg libraries for exactly one iOS SDK/architecture.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK="${IOS_SDK:-iphoneos}"
ARCH="${IOS_ARCH:-arm64}"
MIN_IOS="${IOS_DEPLOYMENT_TARGET:-16.3}"
OUT_DIR="${OUT_DIR:-$REPO_ROOT/.ios-ffmpeg/$SDK-$ARCH}"
case "$SDK:$ARCH" in
    iphoneos:arm64) TRIPLE="arm64-apple-ios$MIN_IOS" ;;
    iphonesimulator:arm64|iphonesimulator:x86_64) TRIPLE="$ARCH-apple-ios$MIN_IOS-simulator" ;;
    *) echo "Unsupported IOS_SDK/IOS_ARCH: $SDK/$ARCH" >&2; exit 1 ;;
esac
SYSROOT="$(xcrun --sdk "$SDK" --show-sdk-path)"
CC="$(xcrun --sdk "$SDK" --find clang)"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/ffmpeg-ios.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT
# Match the Android port's FFmpeg API version.
FFMPEG_VERSION=7.1
curl --fail --location --retry 3 "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.bz2" -o "$BUILD_DIR/ffmpeg.tar.bz2"
tar -xjf "$BUILD_DIR/ffmpeg.tar.bz2" -C "$BUILD_DIR"
cd "$BUILD_DIR/ffmpeg-$FFMPEG_VERSION"
FF_ARCH="$ARCH"
if [[ "$ARCH" == arm64 ]]; then FF_ARCH=aarch64; fi
./configure \
    --prefix="$OUT_DIR" --target-os=darwin --arch="$FF_ARCH" \
    --enable-cross-compile --sysroot="$SYSROOT" \
    --cc="$CC" --cxx="$(xcrun --sdk "$SDK" --find clang++)" \
    --ar="$(xcrun --sdk "$SDK" --find ar)" \
    --nm="$(xcrun --sdk "$SDK" --find nm)" \
    --ranlib="$(xcrun --sdk "$SDK" --find ranlib)" \
    --strip="$(xcrun --sdk "$SDK" --find strip)" \
    --extra-cflags="-target $TRIPLE -isysroot $SYSROOT" \
    --extra-ldflags="-target $TRIPLE -isysroot $SYSROOT" \
    --enable-static --disable-shared --enable-pic --disable-asm \
    --disable-autodetect --disable-programs --disable-doc --disable-debug \
    --disable-avdevice --disable-postproc --disable-avfilter \
    --disable-network --disable-videotoolbox --disable-audiotoolbox \
    --disable-vulkan --disable-metal --disable-securetransport \
    --enable-avformat --enable-avcodec --enable-avutil --enable-swscale --enable-swresample
make -j"${JOBS:-$(sysctl -n hw.logicalcpu)}"
make install
printf '\nFFmpeg ready: %s\n' "$OUT_DIR"
