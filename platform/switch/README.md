# YataiDON for Nintendo Switch homebrew

Experimental port using the shared bgfx renderer, mesa-switch NVK Vulkan, libnx,
and devkitPro SDL2. Boot, controller input, Settings, the native keyboard, audio
and video have been exercised on hardware through title takeover. Performance
work remains in progress.

## Build requirements

Use a devkitPro environment (Linux or MSYS2) with devkitA64, libnx, Switch CMake
helpers, Ninja, pkg-config, Python 3.11+, Git and Switch-built GNU libiconv. Install
Switch-built FFmpeg with avformat, avcodec, avutil, swscale and swresample and its
static dependency metadata. Host libraries cannot be used for these packages.

Build/install [mesa-switch](https://github.com/danfromtico/mesa-switch) into a
Switch prefix containing `lib/libvulkan.a`, `include/vulkan/`, and
`include/vk_video/`. The port links the archive and its static dependencies.
A Vulkan-only SDK without libelf is supported (NVX binary import is disabled).
The driver is linked into the NRO; no desktop Vulkan loader or ICD installation
is needed.

Build Mesa for release with `optimization=3` and `b_ndebug=true`. The game cannot
change optimization of an already-built driver archive. A lower optimization
level leaves CPU-side Vulkan work less optimized even when YataiDON is built
in Release mode. To test another archive without replacing the installed SDK,
set `YATAIDON_MESA_LIBRARY` to its absolute path; keep the SDK headers and static
dependencies matched to that driver revision.

Provide a **host** `shaderc` built from bgfx.cmake **v1.167.9590-598**, the same
release used by the other bgfx targets. The preparation script verifies the
release archive checksum and applies the bgfx and SDL adaptations only to the
expected sources. Dependency checkouts remain under `platform/switch/vendor/`.

```sh
export DEVKITPRO=/opt/devkitpro
export YATAIDON_SHADERC=/path/to/host/shaderc
# Optional if Mesa is installed outside the standard Switch portlibs prefix:
export SWITCH_MESA_PREFIX=/path/to/mesa-switch-sdk
# Optional: use an optimized archive from a matching Mesa build directly:
export YATAIDON_MESA_LIBRARY=/path/to/mesa-build/src/nouveau/vulkan/libvulkan.a
bash platform/switch/build-game.sh
```

The intended output is `platform/switch/build/YataiDON.nro`. The root CMake project also
selects this platform when configured with devkitPro's Switch toolchain.
The build compiles shaders on the host and embeds SPIR-V and the default font.
Switch Release builds use `-O3` for the game and all C/C++ dependencies built by
this project, including bgfx/bx/bimg, Lua, SDL2, SQLite and the audio libraries.
Prebuilt SDK archives retain their own build settings.

For the matching FFmpeg 7.1 SDK, the optional media helper rebuilds FFmpeg and
GNU libiconv at `-O3` without replacing installed SDK files. It verifies the
source archives and applies devkitPro's existing FFmpeg patches. SDK support
libraries and the compiler/system runtime remain supplied by devkitPro.

```sh
# Optional: place the dependency work on a native Linux filesystem.
export YATAIDON_MEDIA_WORKDIR=/path/to/o3-media
bash platform/switch/build-o3-media.sh
export YATAIDON_MEDIA_LIBRARY_DIR="$YATAIDON_MEDIA_WORKDIR/lib"
bash platform/switch/build-game.sh
```

Without `YATAIDON_MEDIA_WORKDIR`, the helper uses `platform/switch/build-o3-media`.
Only matching rebuilt archives found in `YATAIDON_MEDIA_LIBRARY_DIR` replace
the corresponding SDK libraries; headers continue coming from the installed SDK.

SDL2 is fetched and built from its pinned devkitPro source with OpenGL disabled.
Do not substitute the standard prebuilt Switch SDL2 archive: it pulls in an
older Mesa/EGL stack that conflicts with the linked NVK driver.

## SD card layout

```text
sdmc:/switch/YataiDON/
  YataiDON.nro
  Skins/
    PyTaikoGreen/
  Songs/
```

Configuration, scores and shader caches use this writable directory. Standard
application logging uses `latest.log` unless disabled as described below.
Supply skins and songs separately; they are not included in the NRO.
Launch through homebrew with full application memory (title takeover).

The heap commits RAM on demand and restores hbloader's reservation on exit.

## Platform behavior

- Fixed 1280x720 rendering with a 60 FPS target and vsync; Don-chan renders at
  100% resolution.
- Native HID feeds SDL2 virtual controllers. The libnx keyboard supplies text
  input, and SDL2 provides audio and touch services.
- SD previews decode on a worker. Skin/audio assets use the existing bounded
  caches between scenes. No dedicated input polling thread is enabled.
- FFmpeg provides software video/audio decoding. Hardware video decoding, MIDI,
  online services, webcam, Fumen and desktop crash tracing are disabled.
- The bgfx adaptation uses the linked Mesa entry points and libnx's default
  `NWindow` for `VK_NN_vi_surface`; it does not use Nintendo's proprietary NVN SDK.

Further validation should cover suspend/resume, clean exit, docked output and
memory use with large skins/libraries. A sustained 60 FPS is not yet verified.

## Credits

[mesa-switch](https://github.com/danfromtico/mesa-switch), Mesa/Nouveau/NVK,
[devkitPro](https://devkitpro.org/), libnx, SDL, bgfx/bx/bimg and bgfx.cmake provide
the platform and graphics stack. Existing dependency licenses remain in their
source distributions. [GNU libiconv](https://www.gnu.org/software/libiconv/)
provides Japanese chart text conversion. Platform hooks live in
`src/platform/platform_switch.cpp`; dependencies and build rules in `platform/switch/`.

## Logging

Logging is controlled by
`log_level` in the existing `[general]` section of `config.toml`:

```toml
[general]
log_level = "off"
```

Restart the game after changing it. `off` creates no `latest.log` file and starts
no periodic log-flush worker. Other supported values are `debug`, `info`,
`warning`, `error` and `critical`; the default remains `info`.

## Controllers

| Action | Switch controls |
|---|---|
| Menu movement | D-pad |
| Confirm / back | A / B |
| Restart / quit song | Plus / Minus |
| Left Don | D-pad Right/Down or left stick press |
| Left Ka | D-pad Up/Left, L or ZL |
| Right Don | B/Y or right stick press |
| Right Ka | X/A, R or ZR |

The port polls handheld/Lite controls and Npad 1-8 through libnx. A one-time
`[switch].controller_layout_version = 2` migration installs these bindings;
subsequent user changes are retained.
