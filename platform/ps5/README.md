# YataiDON for PS5 / prospero

Experimental native folder app `PPSA99329`.

## Controls

| Action | DualSense buttons |
|---|---|
| Left Ka | D-pad Up/Left, L1, L2 |
| Left Don | D-pad Down/Right, L3 |
| Right Ka | Circle, Triangle, R1, R2 |
| Right Don | Square, Cross, R3 |
| Restart song | Options/Start |
| Back / leave song | Touchpad |

Menus use D-pad navigation, Cross to confirm, Circle/touchpad to go back.

## Build

Use Linux or WSL with CMake 3.24+, Ninja, Python 3.11+, Git, Clang and a
host C++ compiler. Install the public PS5 payload SDK and build the release RADV
archive using [PS5 Vulkan](https://github.com/mihawk-99/PS5_Vulkan).
Cross-compile libopus with `platform/ps5/build-opus.sh` (installs into `PS5_OPUS_PREFIX`), then
build FFmpeg using `platform/ps5/build-ffmpeg.sh`; its installation must include MOV/MP4,
H.264, AAC and Opus support. No Sony SDK or proprietary shader compiler is used.

```sh
export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
export PS5_FFMPEG_PREFIX=/path/to/ffmpeg-ps5
export PS5_VULKAN_PREFIX=/path/to/PS5_Vulkan/.deps/native/radv-release
BUILD_JOBS=8 bash platform/ps5/build-game.sh
python3 platform/ps5/package.py --skip-media
python3 platform/ps5/tools/verify-bundle.py
```

`build-game.sh` fetches pinned source dependencies, applies a checksum-verified
bgfx patch, builds shaderc for the host, then cross-compiles into
`platform/ps5/build-vulkan`. Set `YATAIDON_HOST_BGFX_BUILD` to reuse a host shaderc build.
The main CMake entry point also selects `platform/ps5/` when the SDK toolchain sets `PS5`.

Packaging expects a public native-app template in `platform/ps5/vendor/native-template`
(or `--template /path/to/template`), including `tooling/native`, the clean-room
`runtime/libc.prx` plus its checksum, and host zlib under
`.deps/native/zlib/root/usr`. Generated import stubs describe AGC/Videodec2
firmware imports; their placeholder bodies never ship in the executable.

Output: `platform/ps5/dist/YataiDON-PS5-experimental.zip`. Replace the complete
`PPSA99329` app folder using your folder-app installation workflow. With
`--skip-media`, existing skins, songs, settings and saves remain external;
install complete skins and songs under `/data/YataiDON`. The build receipt
records the executable, game archive and linked driver hashes.

## Verification

Run `python3 platform/ps5/tools/verify-bundle.py` after packaging to check executable
hashes, metadata and archive integrity. These checks do not prove rendering,
HDMI output, audio timing or console stability.

Before treating a build as a release, check onboarding, entry, song selection,
settings, song playback, Options restart/touchpad back, video playback, 60/120 Hz
switching and shutdown on a console. Keep the last working build for comparison.
