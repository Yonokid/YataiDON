#!/usr/bin/env bash
# Build a minimal static FFmpeg for Linux desktop releases.
# Keeps only the containers/codecs YataiDON plays, so the binary links no
# shared libav*/codec libraries and the release needs no lib/ bundle.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FFMPEG_VERSION="${FFMPEG_VERSION:-8.1.2}"
OUT_DIR="${OUT_DIR:-$REPO_ROOT/.linux-ffmpeg}"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/ffmpeg-linux.XXXXXX")"
trap 'rm -rf "$BUILD_DIR"' EXIT

curl --fail --location --retry 3 "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz" -o "$BUILD_DIR/ffmpeg.tar.xz"
tar -xJf "$BUILD_DIR/ffmpeg.tar.xz" -C "$BUILD_DIR"
cd "$BUILD_DIR/ffmpeg-$FFMPEG_VERSION"

./configure \
    --prefix="$OUT_DIR" \
    --disable-autodetect --disable-everything --disable-iconv \
    --disable-gpl --disable-nonfree --disable-version3 \
    --enable-avcodec --enable-avformat --enable-avutil --enable-swscale --enable-swresample \
    --disable-avdevice --disable-avfilter \
    --disable-network --disable-programs --disable-doc --disable-debug \
    --disable-shared --enable-static --enable-pic \
    --enable-protocol=file \
    --enable-demuxer=avi,flv,flac,matroska,mov,mp3,aac,ogg,wav,mpegts,mpegps \
    --enable-decoder=aac,ac3,flv,flac,h264,hevc,mjpeg,mp3,mp3float,mpeg4,opus,pcm_f32le,pcm_s16le,pcm_s24le,pcm_s32le,pcm_u8,theora,vorbis,vp8,vp9 \
    --enable-parser=aac,ac3,flac,h264,hevc,mpegaudio,mpeg4video,opus,vorbis
make -j"${JOBS:-$(nproc)}"
make install
printf '\nFFmpeg ready: %s\n' "$OUT_DIR"
