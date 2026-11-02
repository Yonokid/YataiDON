#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
: "${PS5_PAYLOAD_SDK:?Set PS5_PAYLOAD_SDK}"
: "${FFMPEG_SOURCE:?Set FFMPEG_SOURCE to an unpacked FFmpeg 8.1.2 source tree}"
: "${PS5_OPUS_PREFIX:?Set PS5_OPUS_PREFIX to the PS5 libopus installation}"
prefix=${PS5_FFMPEG_PREFIX:-$root/platform/ps5/vendor/ffmpeg-ps5}
build=${FFMPEG_BUILD_DIR:-$root/platform/ps5/build/ffmpeg}
mkdir -p "$prefix" "$build"
export PKG_CONFIG_LIBDIR="$PS5_OPUS_PREFIX/lib/pkgconfig"
export PKG_CONFIG_PATH="$PKG_CONFIG_LIBDIR"
cd "$build"
"$FFMPEG_SOURCE/configure" --prefix="$prefix" \
  --disable-autodetect --disable-everything --disable-iconv \
  --disable-gpl --disable-nonfree --disable-version3 \
  --enable-avcodec --enable-avformat --enable-avutil --enable-swresample --enable-swscale \
  --disable-avdevice --disable-avfilter \
  --enable-decoder=atrac3p,h264,aac,mpeg4,mjpeg,mp3,mp3float,vorbis,flac,opus,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,pcm_u8 \
  --enable-demuxer=mov,wav,mp3,ogg,flac,mpegps,h264 \
  --enable-parser=aac,mpeg4video,mpegaudio,vorbis,flac,opus,h264 \
  --enable-libopus --pkg-config-flags=--static --enable-encoder=libopus --enable-muxer=ogg \
  --enable-protocol=file --disable-network --disable-programs --disable-doc --disable-debug \
  --enable-small --enable-static --disable-shared --enable-pic --arch=x86_64 \
  --disable-x86asm --enable-cross-compile --target-os=freebsd --enable-pthreads --disable-symver \
  --cc="$PS5_PAYLOAD_SDK/bin/prospero-clang" --cxx="$PS5_PAYLOAD_SDK/bin/prospero-clang++" \
  --ar="$PS5_PAYLOAD_SDK/bin/prospero-ar" --ranlib="$PS5_PAYLOAD_SDK/bin/prospero-ranlib" \
  --pkg-config=pkg-config --host-cc=clang-19
make -j "${BUILD_JOBS:-4}"
make install
mkdir -p "$prefix/lib" "$prefix/include/opus"
cp "$PS5_OPUS_PREFIX/lib/libopus.a" "$prefix/lib/"
cp "$PS5_OPUS_PREFIX/include/opus/"*.h "$prefix/include/opus/"
