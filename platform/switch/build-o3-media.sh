#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
: "${DEVKITPRO:?Set DEVKITPRO to your devkitPro installation}"
source "$DEVKITPRO/switchvars.sh"
export CFLAGS="$ARCH -O3 -ffunction-sections -fdata-sections"
export CXXFLAGS="$CFLAGS"
prefix="${YATAIDON_MEDIA_WORKDIR:-$root/platform/switch/build-o3-media}"
export YATAIDON_MEDIA_SOURCE_DIR="$prefix/sources"
python3 "$root/platform/switch/prepare-o3-media.py"
mkdir -p "$prefix/lib" "$prefix/ffmpeg" "$prefix/iconv"
sdk_version=$("$PORTLIBS_PREFIX/bin/aarch64-none-elf-pkg-config" --modversion libavcodec)
if [ "$sdk_version" != 61.19.100 ]; then
  echo "Expected the matching FFmpeg 7.1 SDK headers (libavcodec 61.19.100), found $sdk_version" >&2
  exit 1
fi

cd "$prefix/ffmpeg"
"$YATAIDON_MEDIA_SOURCE_DIR/ffmpeg-7.1/configure" \
  --prefix="$prefix" --enable-gpl --disable-shared --enable-static \
  --cross-prefix=aarch64-none-elf- --enable-cross-compile \
  --arch=aarch64 --cpu=cortex-a57 --target-os=horizon --enable-pic \
  --optflags=-O3 --extra-cflags="$CPPFLAGS -D_GNU_SOURCE -O3" \
  --extra-cxxflags="$CPPFLAGS -D_GNU_SOURCE -O3" --extra-ldflags="$LDFLAGS" \
  --disable-runtime-cpudetect --disable-programs --disable-debug --disable-doc --disable-autodetect \
  --enable-asm --enable-neon --disable-avdevice --disable-encoders --disable-muxers \
  --enable-swscale --enable-swresample --enable-network \
  --disable-protocols --enable-protocol=file,http,ftp,tcp,udp,rtmp,tls,httpproxy \
  --enable-zlib --enable-bzlib --enable-libass --enable-libfreetype --enable-libfribidi --enable-libdav1d \
  --enable-libnx --enable-nvtegra
libraries=(libavformat/libavformat.a libavcodec/libavcodec.a libavutil/libavutil.a
           libswscale/libswscale.a libswresample/libswresample.a)
make -j"${BUILD_JOBS:-4}" "${libraries[@]}"
for library in "${libraries[@]}"; do
  cp "$library" "$prefix/lib/"
done

cd "$prefix/iconv"
LIBS= CPPFLAGS= LDFLAGS="$ARCH" "$YATAIDON_MEDIA_SOURCE_DIR/libiconv-1.18/configure" \
  --host=aarch64-none-elf --prefix="$prefix" --disable-shared --enable-static --disable-nls \
  CFLAGS="$ARCH -O3"
make -C lib -j"${BUILD_JOBS:-4}"
cp lib/.libs/libiconv.a "$prefix/lib/"
echo "Optimized media archives: $prefix/lib"
