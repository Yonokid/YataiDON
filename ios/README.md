# iOS port

The iOS target shares the C++ game, SDL3 renderer/input/audio, OpenGL ES 3 shaders,
FFmpeg decoding, and touch drum controls with Android. It builds a landscape app
for iPhone and iPad running iOS 16.3 or later. This is a source port; physical-device
latency and distribution signing still need validation.

## Build on a Mac

Install Xcode (including the iOS SDK) and CMake 3.24 or newer. Select the full Xcode
installation with `sudo xcode-select -s /Applications/Xcode.app/Contents/Developer`
if the command-line tools are selected instead. Python 3, Git, and curl are also
required. The first build downloads and compiles dependencies.

Populate the skin submodules before building on a fresh checkout:

```sh
git submodule update --init --recursive
```

Do not run that command over skin changes you want to keep. If your assets live
elsewhere, pass `-DYATAIDON_SKINS_DIR=/absolute/path/to/Skins` to the build script.
Use the complete `Skins` directory from the matching submodule revisions, including
`PyTaikoGreen` and `YataiDONNijiiro`: texture IDs are generated across skins. All
skins placed there are included. `-DIOS_SONGS_DIR=/absolute/path/to/Songs` changes the bundled
song library. Large skin videos increase both app size and first-launch copy time.

### Simulator

```sh
./build_ios.sh simulator
open build-ios-simulator/YataiDON.xcodeproj
```

Select the YataiDON scheme and an installed iPhone or iPad Simulator, then Run.
The script uses your Mac's architecture; set `IOS_ARCH=x86_64` on Intel if needed.
No Apple development team is required for the unsigned Simulator build.

### iPhone or iPad

```sh
IOS_DEVELOPMENT_TEAM=YOUR_TEAM_ID \
IOS_BUNDLE_IDENTIFIER=com.yourname.yataidon \
./build_ios.sh device
open build-ios-device/YataiDON.xcodeproj
```

Select your connected device and the YataiDON scheme. Check Signing & Capabilities
and select your Apple team, then Run. Enable Developer Mode on the device when
Xcode requests it. Without a team, the script builds an unsigned `.app` for compile
checks; it cannot be installed on a physical device until it is signed.

The build does not publish to TestFlight or the App Store. Distribution requires
appropriate signing, artwork, and rights to the assets you include.

## Songs, skins, and saves

On first launch, bundled resources are copied into the app's Documents directory.
Open **Files → On My iPhone/iPad → YataiDON**, or use Finder's device File Sharing.
Add song folders under `Songs`, and skins under `Skins`. TJA files and their audio
files must stay together. Restart the app to rescan new content. The shared folder
also contains `config.toml`, score databases, caches, and `latest.log`.

Existing files, including settings and scores, are preserved on app upgrades.
Missing bundled files are restored at launch; bundled shaders are refreshed to
match the executable. Uninstalling the app deletes its data container, so copy
out any songs and scores you want to keep first.

Touch input is enabled in the bundled default config. Tap inside the drum for Don
and outside for Kat; left and right halves retain the Android mappings. The top
**Back** control replaces Android's system Back button. **Pause** toggles pause in
single-player and two-player gameplay. Song search and text settings use the iOS
keyboard. SDL also handles supported game controllers.

The UIKit animation callback drives rendering. On backgrounding, the game clock
and SDL audio stream pause; foregrounding resumes them together. Frame rate follows
the display callback rather than the desktop FPS limiter.

## Dependency builds and options

`build_ios.sh` builds FFmpeg automatically when its static libraries are absent.
Device and Simulator libraries are separate even when both use ARM64:

```sh
IOS_SDK=iphoneos tools/build_ffmpeg_ios.sh
IOS_SDK=iphonesimulator tools/build_ffmpeg_ios.sh
cmake --preset ios-simulator
cmake --build --preset ios-simulator
```

The presets assume ARM64. Override `CMAKE_OSX_ARCHITECTURES` and
`IOS_FFMPEG_PREFIX` together for Intel Simulators. The scripts accept
`IOS_DEPLOYMENT_TARGET` (default `16.3`), `IOS_FFMPEG_PREFIX`, `JOBS`, `CMAKE`, and
`CONFIGURATION` (default `Release`). Use a separate build directory when changing
SDK or architecture. `CONFIGURATION=Debug ./build_ios.sh simulator` builds symbols
without the desktop sanitizer flags.

Online profile sync is currently disabled for iOS: the desktop curl/TLS dependency
setup is not cross-compiled by this port. Local gameplay and local scores do not
require the server. Optional Fumen support still requires the same seeds as other
platforms.

Platform references: [SDL's iOS integration](https://wiki.libsdl.org/SDL3/README-ios)
and [CMake Apple cross-compilation](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-ios-tvos-visionos-or-watchos).

## Validation

The ARM64 Release build was compiled with Xcode and exercised on an iPhone 17 Pro
Simulator running iOS 26.5. Checks covered first-run asset setup, SDL/CoreAudio
initialization, touch navigation through player entry and song selection, the
bundled TRIPLE HELIX chart, 3D rendering, the Pause control, and returning from the
background. The clock's suspend/resume behavior and the local SQLite database's
integrity were also checked. Physical-device performance, signing, and all alternate
skins have not been validated.
