# iOS port

The iOS target shares the C++ game, SDL3 renderer/input/audio, OpenGL ES 3 shaders,
FFmpeg decoding, and touch drum controls with Android. It builds a landscape app
for iPhone and iPad running iOS 16.3 or later. This is a source port; physical-device
latency and distribution signing still need validation.

To install a pre-built beta instead of building, join the
[TestFlight beta](https://testflight.apple.com/join/ZzrXBUsb).

## Build on a Mac

Install Xcode (including the iOS SDK) and CMake 3.24 or newer. Select the full Xcode
installation with `sudo xcode-select -s /Applications/Xcode.app/Contents/Developer`
if the command-line tools are selected instead. Python 3, Git, and curl are also
required. The first build downloads and compiles dependencies.

Skins are not bundled in the iOS app. On first launch the app opens the onboarding
screen and downloads the selected skins in-game, which keeps the IPA small.
`-DIOS_SONGS_DIR=/absolute/path/to/Songs` changes the bundled song library.

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
Signing is disabled only for that command-line build, not in the generated Xcode
project. Select your team and a unique Bundle Identifier in Xcode before running.
Pass these same values to the script on subsequent builds so regeneration keeps
your signing configuration. If an older generated project reports “No code
signature found”, regenerate it and rebuild with signing enabled.

The build does not publish to TestFlight or the App Store. Distribution requires
appropriate signing, artwork, and rights to the assets you include.

## Build an unsigned IPA with GitHub Actions

The existing [Release workflow](../.github/workflows/build-mobile.yml) includes a
`build-ios` job. In GitHub, open **Actions → Build YataiDON (Release) → Run workflow**
and select the branch containing the iOS changes. This runs all platform builds.

The iOS job uses a `macos-15` runner and `./build_ios.sh device` to build an ARM64
Release app for iOS 16.3 or later, with Bundle ID `com.yataidon.app` and code signing
disabled. It packages the app as `Payload/YataiDON.app` inside
`YataiDON-iOS-unsigned.ipa`, alongside `checksums-ios.sha256`. No Apple certificate,
provisioning profile, or App Store Connect credentials are required. There is no
TestFlight or App Store upload step. Sign the downloaded IPA with your own signing
tool and credentials before installing it on an iPhone or iPad.

The job reuses the existing repository secrets:

- `NETWORK_URL` and `NETWORK_AUTH_KEY` to enable the online client. When either is
  absent, the build uses the offline implementation; runtime network switches
  still follow the configuration described below.

Download the `YataiDON-iOS` artifact from the workflow run after the iOS job
succeeds. Once all platform builds succeed, the existing `latest` GitHub Release
also receives the unsigned IPA and its SHA-256 checksum file. An iOS failure is
included in the build summary and prevents that combined Release from publishing.

The FFmpeg cache is separate from macOS and Simulator builds and includes the
host architecture, device target, minimum iOS version, Xcode version/build, and
FFmpeg build-script hash. The workflow logs the selected Xcode and iOS SDK versions.
Runner images may update; available toolchains are listed in GitHub's
[runner image documentation](https://github.com/actions/runner-images/blob/main/images/macos/macos-15-arm64-Readme.md).
The artifact uses
[compression level 0](https://github.com/actions/upload-artifact#altering-compressions-level-speed-v-size)
because the IPA is already a compressed ZIP archive.

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
After an Xcode upgrade, configuration automatically clears cached dependency paths
inside removed SDK directories so they are discovered in the current SDK.

## Audio latency

The iOS build retains the reduced AudioQueue buffering and 5 ms hardware I/O
preference validated on an iPad Pro speaker. Temporary latency instrumentation
and its build switch have been removed. See [the latency fix log](LATENCY_FIX.md)
for the investigation, measurements, and instructions for rebuilding diagnostics
if the problem recurs.

## Online services

iOS uses the existing Hiroba client for registration, profile and score sync,
remote song selection, and online/version indicators. CPR and its pinned curl
are built separately for Device and Simulator. HTTPS uses Apple's Secure Transport
and system trust store, with certificate verification enabled; no Android CA
bundle or host macOS OpenSSL installation is needed.

Configure the same backend values used by the other platforms in an untracked
repository-root `.env` file:

```dotenv
NETWORK_URL=https://your-test-backend.example
NETWORK_AUTH_KEY=your-backend-key
```

Reconfigure after changing these values. CMake caches them, so clear just those
two entries to reload `.env` (preserve your signing team and Bundle ID):

```sh
IOS_DEVELOPMENT_TEAM=YOUR_TEAM_ID \
IOS_BUNDLE_IDENTIFIER=com.yourname.yataidon \
./build_ios.sh device -UNETWORK_URL -UNETWORK_AUTH_KEY
```

Without both values, the offline implementation is built. In the installed app's
Documents `config.toml`, set `[network] online_play = true` to enable requests;
set `sync_scores = true` if you also want startup score downloads, then restart.
Leave `access_code_1` and `access_code_2` empty for first-time registration, or use your own existing
codes. Existing configurations are preserved on upgrade (old `access_code` and
`player_1_id`/`player_2_id` are migrated automatically), so rebuilding alone does
not turn these switches on. Local gameplay/saves remain available offline.

The [network integration checks](../tests/network/README.md) exercise the actual
client against an isolated fixture and test HTTPS trust on the Simulator. Real
server credentials and physical-device sync still need end-to-end validation.
Optional Fumen support still requires the same seeds as other platforms.

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
