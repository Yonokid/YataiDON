# AGENTS.md

Guidance for AI coding agents working on YataiDON, a C++23 TJA player / Taiko simulator.
User-facing docs live in [README.md](../README.md); this file covers what an agent needs to change code safely.

## Layout

| Path | What |
|------|------|
| `src/YataiDON.cpp` | Entry point and main loop |
| `src/scenes/` | One screen per file pair (`title`, `song_select`, `game`, `result`, ...). Screen IDs in `src/libs/screen.h` |
| `src/objects/<screen>/` | Drawable objects owned by a scene; `global/` is shared across screens |
| `src/libs/` | Engine: `audio/`, `gfx/`, `input/`, `parsers/` (TJA, osu), `file/` (config, scores, Lua `script`), `network/`, `video/`, `optional/` (feature-gated formats) |
| `src/libs/gfx.h` | Backend-neutral graphics facade; implemented by `gfx_raylib.cpp` or `gfx_bgfx.cpp` |
| `src/platform/` | One implementation of `platform.h` per platform; CMake compiles exactly one |
| `cmake/` | Dependencies (`deps.cmake`), platform selection, patches for third-party code |
| `android/`, `ios/`, `ps5/` | Platform projects; see their READMEs |
| `Skins/` | Gitignored. Each skin is its own git repo (URL in `Skins/<name>/.skin-repo`) |
| `Songs/` | Local chart library, not source |

## Build

Desktop (Ninja, wipes `build/`, copies binary to `./YataiDON`):

```sh
./build_debug.sh     # Debug
./build_release.sh   # Release + regenerates Skins/*/checksums.sha256
```

Incremental rebuild without wiping: `build_re.sh`.
Other targets: `build_android.sh`, `build_ios.sh`, `build_web.sh`, `ps5/build-game.sh`.

Useful CMake options:

- `-DYATAIDON_GFX_BACKEND=raylib|bgfx` (bgfx: Linux desktop and Android/SDL3 only)
- `-DYATAIDON_PROFILER=ON` — Tracy client (v0.14.1)

`.env` in the repo root (never commit it) supplies `NETWORK_URL`, `NETWORK_AUTH_KEY`,
`DATATABLE_SEED`, `FUMEN_SEED`. Without them, networking / Fumen support are compiled out.
Android Gradle must forward the network values as CMake args, or linking fails.

Do not compile using cmake/ninja yourself. Always use the provided shell scripts.

## Checks

There is no unit test suite. Before finishing a change:

1. It builds (`cmake --build build`).
2. Formatting passes — CI runs clang-format **23.1.3**:
   ```sh
   find src -type f \( -name '*.cpp' -o -name '*.h' \) -exec clang-format -i {} +
   ```
3. Lint: `run-clang-tidy -quiet -p build "^$(pwd)/src/"`.
4. For behavior changes, run the game (`./YataiDON`) and exercise the affected screen.
   Use the `PyTaikoGreen` skin for test runs.

## Conventions

- **No platform `#ifdef`s in shared code.** Platform differences go behind a `platform_*`
  hook in `src/platform/platform.h`; optional libraries are gated with `YATAIDON_HAS_*`
  macros set in `CMakeLists.txt`, never platform names.
- **No backend types outside the gfx layer.** Scenes and objects use `gfx.h` / `gfx/` only;
  raylib or bgfx calls belong in `gfx_raylib.cpp` / `gfx_bgfx.cpp`.
- Style: LLVM-based `.clang-format` — 4-space indent, 120 columns, braces always,
  `Type* ptr`. Match surrounding naming (snake_case functions and members).
- Keep comments sparse and about *why*, as in existing code.
- Commit messages: short, and imperative.

## Skins and Lua

- Skins contain `Graphics/` (textures + `texture.json` positions), `Scripts/` (Lua per screen),
  `Sounds/`, `Fonts/`, `Models/` (3D characters), `Videos/`.
- Lua globals exposed by the engine: `tex`, `anim`, `text`, `audio` (see `.luarc.json`;
  bindings in `src/libs/file/script.cpp`, via sol2).
- A skin can inherit from another; `require` resolves modules in the skin's own folder
  before ancestors.
- Skin changes are committed in the skin's own repo, not this one.

## Don't

- Commit `.env`, `config.toml`, `scores.db`, build outputs, keystores, or anything under `Skins/`.
- Add a dependency for something a few lines (or an existing dep in `cmake/deps.cmake`) can do.
- Edit vendored/fetched code in `.cmake-deps/`; patch it via `cmake/patches/` instead.
