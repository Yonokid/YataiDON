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
expected sources. Dependency checkouts remain under `switch/vendor/`.

```sh
export DEVKITPRO=/opt/devkitpro
export YATAIDON_SHADERC=/path/to/host/shaderc
# Optional if Mesa is installed outside the standard Switch portlibs prefix:
export SWITCH_MESA_PREFIX=/path/to/mesa-switch-sdk
# Optional: use an optimized archive from a matching Mesa build directly:
export YATAIDON_MESA_LIBRARY=/path/to/mesa-build/src/nouveau/vulkan/libvulkan.a
bash switch/build-game.sh
```

The intended output is `switch/build/YataiDON.nro`. The root CMake project also
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
bash switch/build-o3-media.sh
export YATAIDON_MEDIA_LIBRARY_DIR="$YATAIDON_MEDIA_WORKDIR/lib"
bash switch/build-game.sh
```

Without `YATAIDON_MEDIA_WORKDIR`, the helper uses `switch/build-o3-media`.
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

The Switch heap starts small and commits RAM as the allocator requests it. Large
free blocks at the end of the heap can be returned to the system. Startup releases
the unused part of hbloader's full-memory reservation, preserving the loaded NRO;
the original reservation is restored before returning to hbmenu. A system memory
overlay reports committed memory, including free allocator space and GPU resources,
rather than just live game allocations.

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

The first successful cross-build was on 29 October 2026 using devkitA64 GCC
15.2.0, SDL2 2.28.5, GNU libiconv 1.18 and mesa-switch commit
`0ff52079ca9e9e837ed27eb95a8d239f439a1864` (Mesa 26.2.4). Mesa was built with
NVK and the Switch platform enabled, without Gallium or optional libelf support.
Its host tools used LLVM/Clang 19 and Rust 1.99.1 with the
`aarch64-unknown-linux-gnu` standard library. The Meson 1.7 Rust probe needed
`sanity.rs` added to the wrapper's native sanity-check filename patterns.

Validation covered the AArch64 executable (no undefined symbols), NRO segment
bounds/alignment, embedded icon and NACP title, platform-source lint, Python and
shell syntax, and clang-format 23.1.3 for the new C/C++ headers and source.

Further validation should cover suspend/resume, clean exit, docked output and
memory use with large skins/libraries. A sustained 60 FPS is not yet verified.

## Credits

[mesa-switch](https://github.com/danfromtico/mesa-switch), Mesa/Nouveau/NVK,
[devkitPro](https://devkitpro.org/), libnx, SDL, bgfx/bx/bimg and bgfx.cmake provide
the platform and graphics stack. Existing dependency licenses remain in their
source distributions. [GNU libiconv](https://www.gnu.org/software/libiconv/)
provides Japanese chart text conversion. The project follows `docs/AGENTS.md`: the platform hooks
live in `src/platform/platform_switch.cpp`, dependencies/build rules in `switch/`,
and scenes continue using the shared graphics facade.

## Logging

Temporary boot/controller/performance diagnostics and stdout/stderr SD-file
redirection have been removed. Standard YataiDON logging is controlled by
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

## Storage and rendering

Native directory batches collect chart paths, sizes and timestamps together.
Each scan refreshes that metadata, including timestamps for same-size edits.
Song Select uses the shared song library and background navigation loader.
Failed native walks fall back to the portable filesystem implementation.
SQLite's SD VFS preserves transactions, rollback, EOF reads and metadata access
for already-open writable databases.

The shared asset caches retain skin resources between scenes. Settings reloads
assets when skin, language, audio or volume changes require it. The bgfx render
thread and VSync pace frames; character poses and inverse bind matrices are
cached, and 2D draws share transient pages. Character meshes with up to 64 bones
keep their bind vertices, joint indices and weights in GPU buffers. Mesh and
outline vertex shaders apply the current bone palette; pose changes upload bone
transforms instead of rebuilding and uploading every vertex. Cached per-joint
boxes conservatively bound the animated mesh without reading vertices back.
Repeated integer animation frames reuse a cached palette and animated bounds.
Each model retains at most 4 MiB of pose samples for its current animation;
animation replacement, model destruction or mesh edits invalidate those samples.
Immutable cached palettes are referenced directly by mesh submissions rather than
copied into the encoder for every mesh. Three frame slots keep those references
alive until the threaded renderer has consumed them, including across cache eviction.
Pose samples store normal rows without a duplicate normal-matrix array.
Fractional frames and CPU-skinned models retain the normal calculation path.
Unsupported meshes retain CPU skinning; large meshes split it between the game
thread and one persistent worker, and small meshes run directly.
The game thread prefers core 0 and the character worker prefers core 2 while
keeping the process's allowed CPU mask, so the scheduler can still migrate them.
Fallback vertex packing shares the skinning pass. Character render bounds
transform eight corners of a local containing box rather than every vertex.
Standard 2D sprites outside the render target and transparent alpha-blended
sprites are rejected before submission. Unchanged shader uniforms keep the
current sprite batch intact. Custom vertex shaders bypass the visibility check.
Lua metadata calls return independent writable tables directly on the caller's
Lua stack, avoiding temporary registry references and correctly supporting coroutines.
Draw bindings read their parameter tables directly from the Lua stack, including
text draws. Texture lookups reuse Lua userdata through a weak cache, so repeated
lookups avoid allocating wrappers while unloaded textures remain collectible.
Lua uses generational collection for short-lived draw tables, and song-time
overrides skip the system-clock call. Switch release builds keep Lua's opcode
dispatch branches separate rather than merging them through cross-jumping;
this compiler setting applies only to the interpreter source file.

The FPS counter displays only FPS and refreshes its text four times per second.
Detailed per-draw timing, call counters and CPU-tick queries have been removed.
Note and label passes reuse calculated positions, while uploaded outlined text
skips its completed image-upload synchronization path. Setting an unchanged
texture filter or wrap mode preserves the current batch.
Standard 2D sprites use GPU instancing when the renderer provides the required
instance attributes and eight texture samplers. One 64-byte record supplies rectangle,
UV range, origin, rotation, tint and colour transform; packed colour and texture-index
values reduce instance traffic by 20%. A fixed quad and vertex shader generate the corners. Custom
shader draws and renderers without the required instance attributes keep the
CPU-expanded path. Sprite order, blend modes, scissors and render targets are
preserved. The output remains 720p and Don-chan renders at 100% resolution.
Character passes calculate the shared world transform once and draw with local
material overrides, avoiding temporary shader/transform arrays and model mutations.
On renderers with eight texture samplers, each sprite batch can reference eight
different textures. A per-sprite index selects the texture in the fragment shader,
preserving input order without sorting. Real state changes, a full texture table,
or mutation/unloading of any texture used by the batch submit the queued work
before continuing.
Plain filled rectangles join those sprite batches rather than breaking a batch
to switch to CPU-generated geometry.
The known colour-transform shader also joins these batches. Its HSV transform
is calculated once when its colours change and carried with each sprite, so
switching between differently coloured folders preserves sprite order without
submitting a separate batch for each shader change.
Finished animations retain their final state without recalculating it; loop and
restart behavior is preserved. Inactive song boxes skip preview checks, and
folder voice names are built only when opening or closing triggers playback.

Long ASCII text uses a per-string glyph lookup instead of searching the whole
font for every character. Outlined labels copy only their requested glyph
bitmaps and fallback/line-height glyphs, with independent pixel ownership.
These CPU-composited labels reuse the bitmap cache without packing or uploading
an unused font atlas. GPU-drawn text still builds its atlas on demand.
Rendered outlined labels share a 48 MiB bounded texture cache, keyed by font
generation and every layout/colour parameter. Skin/font reloads invalidate old
entries; visible labels retain their texture safely if a cache entry is evicted.
Song Select rasterizes labels when they become visible. Folder summaries find each
folder's chart range by binary search in the completed scan; loading reuses scan timestamps.
TJA chart reads use the native whole-file reader, with the existing portable
reader as fallback and the same encoding and line-splitting rules.
Settled closed boxes skip finished animation updates; only active previews and
focused songs advance the shared difficulty fade.
