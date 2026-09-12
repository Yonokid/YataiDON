#!/usr/bin/env bash
# Generate an Xcode project and build an iOS app. No signing account is needed
# for Simulator builds; supply IOS_DEVELOPMENT_TEAM for device signing.
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
MODE="${1:-simulator}"
if [[ $# -gt 0 ]]; then shift; fi
case "$MODE" in
    simulator) SDK=iphonesimulator; ARCH="${IOS_ARCH:-$(uname -m)}" ;;
    device) SDK=iphoneos; ARCH=arm64 ;;
    *) echo "Usage: $0 [simulator|device] [extra cmake configure arguments...]" >&2; exit 1 ;;
esac
CMAKE="${CMAKE:-cmake}"
command -v "$CMAKE" >/dev/null || { echo "CMake is required (brew install cmake)." >&2; exit 1; }
xcrun --sdk "$SDK" --show-sdk-path >/dev/null
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-ios-$MODE}"
FFMPEG_PREFIX="${IOS_FFMPEG_PREFIX:-$REPO_ROOT/.ios-ffmpeg/$SDK-$ARCH}"
NEEDS_FFMPEG=0
for lib in avformat avcodec avutil swscale swresample; do
    if [[ ! -f "$FFMPEG_PREFIX/lib/lib$lib.a" ]]; then NEEDS_FFMPEG=1; fi
done
if [[ "$NEEDS_FFMPEG" == 1 ]]; then
    IOS_SDK="$SDK" IOS_ARCH="$ARCH" OUT_DIR="$FFMPEG_PREFIX" bash "$REPO_ROOT/tools/build_ffmpeg_ios.sh"
fi
"$CMAKE" -S "$REPO_ROOT" -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT="$SDK" \
    -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${IOS_DEPLOYMENT_TARGET:-16.3}" \
    -DCMAKE_BUILD_TYPE="${CONFIGURATION:-Release}" \
    -DIOS_FFMPEG_PREFIX="$FFMPEG_PREFIX" \
    -DIOS_DEVELOPMENT_TEAM="${IOS_DEVELOPMENT_TEAM:-}" \
    -DIOS_BUNDLE_IDENTIFIER="${IOS_BUNDLE_IDENTIFIER:-com.yataidon.app}" "$@"
"$CMAKE" --build "$BUILD_DIR" --config "${CONFIGURATION:-Release}" --target YataiDON --parallel "${JOBS:-$(sysctl -n hw.logicalcpu)}"
printf '\nBuild complete. Xcode project: %s/YataiDON.xcodeproj\n' "$BUILD_DIR"
