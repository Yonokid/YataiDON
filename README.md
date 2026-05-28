<img src="/docs/logo.png">

A TJA player and Taiko simulator written in C++ using the [raylib](https://www.raylib.com/) library.

![License](https://img.shields.io/github/license/Yonokid/YataiDON)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-blue)
[![GitHub Stars](https://img.shields.io/github/stars/Yonokid/YataiDON?style=flat&label=stars)](https://github.com/Yonokid/YataiDON/stargazers)
[![Discord Members](https://img.shields.io/discord/722513061419810946.svg?label=Discord&logo=discord)](https://discord.gg/XHcVYKW)
[![Builds](https://github.com/Yonokid/YataiDON/actions/workflows/build.yml/badge.svg)](https://github.com/Yonokid/YataiDON/actions/workflows/build.yml)

## Features

- Cross-platform compatibility (Windows, Linux)
- Controller Support
- Low latency audio via ASIO or WDM-KS
- Recursive and Dynamic Song Select Menu
- Multithreaded Input
- Multiple chart format support (TJA, Fumen, OSU/OSZ)
- Background video playback via FFmpeg
- Skin system with customizable graphics, sounds, and Lua scripting
- Branches (Normal/Expert/Master difficulty branches)
- Song search and favorites system
- Multi-language support
- 3D Don-chan character with animations

## Modes

- **1 Player**: Single player mode.
- **2 Player**: Up to 2 players simultaneously.
- **Dan Dojo**: Challenge mode for back to back songs.
- **Practice Mode**: Freely scrobble through a song and practice

## System Requirements

- **Windows**: Windows 10 or higher
- **macOS**: Unsupported at this time
- **Linux**: Ubuntu 20.04 or higher (other distributions may work but are untested)

## FAQ

Q: I'm on Windows and my latency is really high!<br>
A: Change your `device_type` in game settings to `WDM-KS`, `WASAPI`, or `ASIO` if your computer supports it

## Installation

### Pre-built Binaries

Download the latest release for your operating system from the [releases page](https://github.com/Yonokid/YataiDON/releases).

#### Windows
1. Install the [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe) from Microsoft
2. Run `YataiDON.exe`

#### Linux
1. Run `YataiDON.bin`

## Building from Source

See the wiki when it is available

## Controls

- Press **F1** during gameplay for quick restart
- Press **ESC** during any screen to go back
- Press **Space Bar** during song select to favorite a song
- Press **Left Control** or **Right Control** to skip songs by 7
- Press **F5** during song select to refresh
- Generic drum keybinds can be customized in `config.toml` or through the in-game settings menu

## Contributing

Contributions are welcome! Please keep in mind:
- Check the [issues page](https://github.com/Yonokid/YataiDON/issues) for enhancements and bugs before starting work
- Feel free to open new issues for bugs or feature requests

## Known Issues

See the [issues page](https://github.com/Yonokid/YataiDON/issues) or the Discord for current bugs and planned enhancements.

## License

This project is licensed under the terms specified in the LICENSE file.

## Acknowledgments

Dependencies used in the project:
- [raylib](https://www.raylib.com/) - A simple and easy-to-use library to enjoy videogames programming.
- [SDL3](https://github.com/libsdl-org/SDL) - Cross-platform development library for audio, input, and graphics.
- [SQLite](https://www.sqlite.org/) - Self-contained, serverless SQL database engine.
- [RapidJSON](https://github.com/Tencent/rapidjson) - Fast JSON parser/generator for C++.
- [toml++](https://github.com/marzer/tomlplusplus) - TOML config file parser and writer for C++.
- [spdlog](https://github.com/gabime/spdlog) - Fast C++ logging library.
- [Lua](https://github.com/marovira/lua) - Lightweight, embeddable scripting language.
- [sol2](https://github.com/ThePhD/sol2) - C++ binding library for Lua.
- [cpptrace](https://github.com/jeremy-rifkin/cpptrace) - Stacktrace library for C++.
- [libsndfile](https://github.com/libsndfile/libsndfile) - Library for reading and writing audio files.
- [libsamplerate](https://github.com/libsndfile/libsamplerate) - Sample rate converter for audio.
- [FFmpeg](https://ffmpeg.org/) - Complete, cross-platform multimedia framework.
- [digestpp](https://github.com/kerukuro/digestpp) - C++11 header-only message digest library.
- [PortAudio](http://www.portaudio.com/) - Cross-platform audio I/O library.
- [miniz](https://github.com/richgel999/miniz) - Single-file ZIP/DEFLATE library.

People (in no particular order):
- [IID](https://github.com/IepIweidieng/)
- [mc08](https://github.com/splitlane/)
- [QBaraki](https://github.com/QBaraki)
- [KabanFriends](https://github.com/KabanFriends/)
- [DragonRatTiger](https://github.com/DragonRatTiger/)
- [Komi](https://github.com/0auBSQ/)
- [sumandrew](https://github.com/somepin)
- [WallK](https://github.com/WallK)
- [dpkgluci](https://github.com/germe-deb)
- [chuchy](https://github.com/magickale) (osu parser)
- [churrochef](github.com/churro-chef) (vertical scroll)
- [cainan](https://github.com/cainan-c/) (3d don chan)
- [RyutoSetsujin](https://github.com/RyutoSetsujin/)
- [Nyannurs](https://github.com/nyannurs) (testing)
- [fehpizza](https://github.com/fehh44) (logo)
- Radix (testing)
- Thonktheta (testing)
- SomehowScarlet (nix os)
- SunnyDeez (testing)

---
