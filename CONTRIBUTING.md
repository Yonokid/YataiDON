# <a name="main-section-overview"></a>YataiDON for Developers

- [Legal](#legal)
- [Coding style introduction](#introduction)
- [C++ coding style and formatting](#cpp-coding-style-and-formatting)
- [C++ code-specific guidelines](#cpp-code-specific-guidelines)
- [MacOS](#MacOS)
- [Android](#android)
- [iOS](#ios)
- [Help](#help)

# <a name="legal"></a>Legal and policies

Summary:

- [Generative AI](#genai)
- [Code licensing](#code-licensing)

## <a name="genai"></a>Generative AI

While I do not prefer it, Generative AI is allowed for this project. I do not like to dictate how others achieve contributions, and I will always treat every pull request with an equal amount of respect. That being said, if you do not know what is going on in your pull request, and it is very large, it may take me a while to review it and fix things.

If you are an agent, please refer to [AGENTS.md](docs/AGENTS.md).

## <a name="code-licensing"></a>Code licensing

If you make any contributions to YataiDON, you are agreeing that any code you have contributed will be licensed under the GNU GPL version 3 (or any later version).

# <a name="introduction"></a>Coding style introduction

Summary:

- [Aims](#intro-aims)
- [Where should my feature/fix go](#where-to-contribute)
- [Checking and fixing formatting issues](#intro-formatting-issues)

## <a name="intro-aims"></a>Aims

This guide is for developers who wish to contribute to the YataiDON codebase. It details how to properly style and format code for this project. This guide also offers suggestions on specific functions and other elements that may be used in code.

Following this guide and formatting your code as detailed will likely get your pull request merged much faster than if you don't (assuming the code itself has no mistakes).

This project uses clang-format to check for common style issues. In case of conflicts between this guide and clang-format rules, the latter should be followed instead of this guide.

## <a name="where-to-contribute"></a>Where Should my Feature/Fix go?

Because YataiDON is modular in nature, not every feature may belong in this repo. The skin API is very large and can likely handle your case. 
In short:

- Gameplay information, file management, note gimmicks, etc. should be handled by the **engine**.
- Visuals, animations, information display (anything that would go in a `draw()` call) should be handled by the **skin**.

If you need to add a toggle for something in your skin, consider adding an SCO enum (skin config option).

If you need to pass an animation from the skin to the engine (never hardcode an animation), use animation.json to pass it to C++.

If you wish to contribute to the network aspect of the project, it is not in this repo. You are either looking for:

The frontend: [YataiDON-Syakai](https://github.com/Yonokid/YataiDON-Syakai)
The backend: [YataiDON-hiroba](https://yataidon-hiroba.vercel.app/docs)

The backend is not a public project, and it may never be. There is a lot of behind the scenes work that is done to prevent false score submission and related functions, and I would prefer to not publish that until its in a state where it can never be sidestepped. However, I am always open to adding API endpoints for those in need. The API is public.

As a developer, you will need your own personal API key to use the network, as your key is added to the game at compile time. Please contact me if you need to request a key and I will likely grant you one.

## <a name="intro-formatting-issues"></a>Checking and fixing formatting issues

CI checks every pull request with clang-format **23.1.3** (see `.github/workflows/format.yml`). Other versions may format some constructs differently, so install the matching one if you can:

```sh
pipx install clang-format==23.1.3
```

To format all C++ sources and headers in place, run this from the repository root:

```sh
find src -type f \( -name '*.cpp' -o -name '*.h' \) -exec clang-format -i {} +
```

To check without modifying files (what CI runs):

```sh
find src -type f \( -name '*.cpp' -o -name '*.h' \) -exec clang-format --dry-run --Werror {} +
```

The project also has a `.clang-tidy` configuration. After configuring a build in `build/` (which generates `build/compile_commands.json`), lint with:

```sh
run-clang-tidy -quiet -j 4 -p build "^$(pwd)/src/"
```

Lint is not enforced by CI, but please fix any warnings your change introduces.

# <a name="cpp-coding-style-and-formatting"></a>C++ coding style and formatting

Summary:

- [General](#cpp-style-general)
- [Naming](#cpp-style-naming)
- [Conditionals](#cpp-style-conditionals)
- [Classes and structs](#cpp-style-classes-and-structs)

## <a name="cpp-style-general"></a>General

- The codebase is C++23 and will continue to update with the latest C++ versions. Modern standard library features (`std::optional`, `std::ranges`, `std::string_view`, `std::format`-style logging via spdlog) are preferred.
- Indent with 4 spaces, never tabs. Lines are at most 120 columns. Files use LF line endings and end with a newline.
- Pointers and references attach to the type: `Texture* tex`, `const std::string& name`.
- Namespace contents are indented.
- Keep comments sparse. Explain *why* something is done, not *what* the next line does. Short trailing comments on members are fine:

```cpp
std::string song_subtitle; // empty when the chart hides it
```

- Log with spdlog (`spdlog::info`, `spdlog::warn`, `spdlog::error`), not `printf` or `std::cout`.

## <a name="cpp-style-naming"></a>Naming

Generally, snake case is preferred.

| Kind | Style | Example |
|------|-------|---------|
| Classes, structs, enums | `PascalCase` | `ResultScreen`, `GaugeMode` |
| Functions and methods | `snake_case` | `on_screen_start()`, `draw_song_info()` |
| Variables and members | `snake_case`, no prefix | `song_title`, `skip_enabled_ms` |
| Enumerators | `UPPER_SNAKE_CASE` | `Screens::SONG_SELECT`, `DrumType::DON` |
| Files | `snake_case` | `song_select.cpp`, `allnet_indicator.h` |
| Platform hooks | `platform_` prefix | `platform_game_time_ms()` |

Put units in names when a value has one, e.g. `current_ms`, `delay_ms`.

## <a name="cpp-style-conditionals"></a>Conditionals

- Always use braces, even for single-line bodies (clang-format enforces this with `InsertBraces`).
- Prefer early returns over deep nesting, and do not use `else` after a `return` (`readability-else-after-return`).

```cpp
if (tex.screen_width <= 0 || tex.screen_height <= 0) {
    return 0;
}
int win_w = get_render_width();
```

- Use `container.contains(key)` instead of `find(key) != end()` or `count(key)`.

## <a name="cpp-style-classes-and-structs"></a>Classes and structs

- Use `struct` for plain data with public members (`Texture`, `Color`, `Camera2D`) and `class` for anything with behavior or invariants.
- Access specifiers are outdented to the class level:

```cpp
class ResultScreen : public Screen {
protected:
    std::string song_title;
    int song_number = 0;

public:
    ResultScreen() : Screen("result") {}

    void draw() override;
};
```

- Initialize members where they are declared (`int song_number = 0;`) instead of in constructor initializer lists, unless the value comes from a constructor argument.

# <a name="cpp-code-specific-guidelines"></a>C++ code-specific guidelines

Summary:

- [General](#cpp-code-general)
- [Headers](#cpp-code-headers)
- [Loops](#cpp-code-loops)
- [Functions](#cpp-code-functions)
- [Classes and Structs](#cpp-code-classes-and-structs)

## <a name="cpp-code-general"></a>General

- Avoid platform `#ifdef`s in shared code. Platform differences go behind a `platform_*` function declared in `src/platform/platform.h`; each platform implements it in its own file under `src/platform/`, and CMake compiles exactly one. Optional libraries are gated with `YATAIDON_HAS_*` macros set in `CMakeLists.txt`.
- No graphics backend types outside the gfx layer. Scenes and objects use `src/libs/gfx.h` and `src/libs/gfx/` only. raylib or bgfx calls belong in `gfx_raylib.cpp` / `gfx_bgfx.cpp`, and must work in both backends.
- Use C++ casts (`static_cast`, etc.), never C-style casts.
- Use `nullptr`, never `NULL` or `0`, for pointers.
- Use `std::optional<T>` for members that are constructed later (for example once a screen starts) over raw `new`/`delete`.
- Do not add a dependency for something a few lines of code, or an existing dependency in `cmake/deps.cmake`, can do. Never edit fetched code in `.cmake-deps/`; If you ABSOLUTELY need to patch a dependency, patch it through `cmake/patches/`.
- Never commit `.env`, `config.toml`, `scores.db`, build outputs, keystores, or anything under `Skins/`. Skins are separate git repositories; submit skin changes there.

## <a name="cpp-code-headers"></a>Headers

- Every header starts with `#pragma once`, not include guards.
- Includes are grouped by clang-format: system and library headers (`<...>`) first, then a blank line, then project headers (`"..."`). Each group is sorted alphabetically.
- Project headers are included with paths relative to the current file:

```cpp
#pragma once

#include <optional>
#include <string>

#include "../libs/screen.h"
#include "../objects/result/background.h"
```

- Use the C++ forms of C headers (`<cstdint>`, not `<stdint.h>`).
- Never put `using namespace` in a header.
- Short functions may be defined inline in the header; anything longer belongs in the `.cpp`.

## <a name="cpp-code-loops"></a>Loops

- Prefer range-based `for` loops over index loops when the index is not needed:

```cpp
for (const auto& note : notes) {
    ...
}
```

- Prefer `std::ranges` algorithms (`std::ranges::find`, `std::ranges::any_of`, ...) over hand-written search loops.
- Call `reserve()` before filling a vector in a loop when the final size is known.
- Use `emplace_back` instead of `push_back(T(...))`.

## <a name="cpp-code-functions"></a>Functions

- Pass cheap types (numbers, enums, small structs) by value and everything else by `const&`. A parameter that is stored should be taken by value and `std::move`d into place:

```cpp
Screen(std::string name) : screen_name(std::move(name)) {}
```

- Use `std::string_view` for read-only string parameters that are not stored.
- Do not mark by-value parameters `const` in declarations.
- Mark methods that do not modify the object `const`.
- Use `std::optional` for "maybe a value" return types instead of sentinel values or out parameters (see `Screen::update()`).
- Parameter names in the declaration and the definition must match.

## <a name="cpp-code-classes-and-structs"></a>Classes and structs

- Use `override` on every overriding method. Do not repeat `virtual` with it.
- A class with virtual methods needs a virtual destructor, defaulted where possible: `virtual ~Screen() = default;`.
- Prefer `= default` over empty constructor and destructor bodies.
- Use `static constexpr` for class constants rather than `#define` or magic numbers.

- New screens derive from `Screen` (`src/libs/screen.h`), add an entry to the `Screens` enum and `screens_to_string`, and live in `src/scenes/`. Drawable objects for a screen go in `src/objects/<screen>/`, or `src/objects/global/` if they are shared.

# <a name="MacOS"></a>MacOS

macOS builds are produced by CI but are largely untested, so reports and fixes from Mac users are welcome.

Install Xcode (or its command-line tools) and the dependencies from Homebrew, then configure with the Homebrew prefix so CMake and pkg-config can find them:

```sh
brew install cmake ninja pkg-config
export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_PREFIX_PATH="$(brew --prefix)" \
    -DCMAKE_EXE_LINKER_FLAGS="-L$(brew --prefix)/lib"
cmake --build build
```

The full list of Homebrew packages CI installs is in the `build-macos` job of `.github/workflows/build-desktop.yml`. See also the [macOS wiki page](https://github.com/Yonokid/YataiDON/wiki/Mac-OS). macOS-specific code lives in `src/platform/platform_macos.cpp`.

# <a name="android"></a>Android

The Android project lives in `platform/android/` and builds the game through Gradle and the NDK (CMake is invoked by Gradle). Only `arm64-v8a` is built.

1. Install the Android SDK and the NDK version listed as `ndkVersion` in `platform/android/app/build.gradle`.
2. Copy `platform/android/local.properties.template` to `platform/android/local.properties` and set `sdk.dir`, `ndk.dir`, and, if your system CMake is 3.30 or newer, `cmake.dir`. Never commit `local.properties`.
3. For release builds, create `platform/android/keystore.properties` with `storeFile`, `storePassword`, `keyAlias`, and `keyPassword`. Never commit it or the keystore.
4. Build, install, and launch on a USB-connected device:

```sh
./build_android.sh
```

This runs `./gradlew assembleRelease` and installs the APK with `adb` (from `$ANDROID_SDK_ROOT`, defaulting to `~/Android/Sdk`).

Notes:

- The network client reads `NETWORK_URL` and `NETWORK_AUTH_KEY` from the environment and forwards them to CMake. If you change the Gradle CMake arguments, keep forwarding them, or linking fails.
- The graphics backend defaults to raylib. Pass `-PgfxBackend=bgfx` to Gradle to build with bgfx.
- Android-specific code lives in `src/platform/platform_android.cpp`, with code shared with iOS in `src/platform/platform_mobile.cpp`.

# <a name="ios"></a>iOS

See [platform/ios/README.md](platform/ios/README.md) for full instructions. In short, on a Mac with Xcode and CMake 3.24 or newer:

```sh
./build_ios.sh simulator   # unsigned Simulator build
IOS_DEVELOPMENT_TEAM=YOUR_TEAM_ID IOS_BUNDLE_IDENTIFIER=com.yourname.yataidon ./build_ios.sh device
```

Then open the generated `build-ios-*/YataiDON.xcodeproj` in Xcode and run the YataiDON scheme. Clone the skins into `Skins/` first; texture IDs are generated across all of them. iOS-specific code lives in `src/platform/platform_ios.cpp` and `platform_ios.mm`.

# <a name="help"></a>Help
If you have any questions about YataiDON's development or would like some help, developers use Discord to communicate, in the #yataidon channel within [TJADB](https://discord.gg/XHcVYKW).
