# YataiDON

A TJA player and Taiko simulator written in C++ using the [raylib](https://www.raylib.com/) library.

![License](https://img.shields.io/github/license/Yonokid/YataiDON)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-blue)
[![GitHub Stars](https://img.shields.io/github/stars/Yonokid/YataiDON?style=flat&label=stars)](https://github.com/Yonokid/YataiDON/stargazers)
[![Discord Members](https://img.shields.io/discord/722513061419810946.svg?label=Discord&logo=discord)](https://discord.gg/XHcVYKW)
[![Builds](https://github.com/Yonokid/YataiDON/actions/workflows/build.yml/badge.svg)](https://github.com/Yonokid/YataiDON/actions/workflows/build.yml)

<img src="/docs/demo.gif">

## Features

- Cross-platform compatibility (Windows, macOS, Linux)
- Controller Support
- Low latency audio via ASIO or WDM-KS
- Recursive and Dynamic Song Select Menu
- Multithreaded Input

## Modes

- **1 Player**: Single player mode.

## System Requirements

- **Windows**: Windows 10 or higher
- **macOS**: macOS 10.14 (Mojave) or higher
- **Linux**: Ubuntu 20.04 or higher (other distributions may work but are untested)

> **Note**: Operating systems below these requirements are not supported.

## FAQ

Q: I'm on Windows and my latency is really high!<br>
A: Change your `device_type` in game settings to `WDM-KS`, `WASAPI`, or `ASIO` if your computer supports it<br>
<br>
Q: I want to add more song paths!<br>
A: You can either append new folders:<br>
`tja_path = ["/run/media/yonokid/HDD/Games/PyTaiko/Songs", "Songs", "Cool Folder"]`<br>
or replace the base one:<br>
`tja_path = ["/run/media/yonokid/HDD/Games/PyTaiko/Songs"]`<br>
Just make sure to use `/` and not `\`!<br>

## Installation

### Pre-built Binaries

Download the latest release for your operating system from the [releases page](https://github.com/Yonokid/YataiDON/releases).

#### Windows
1. Install the [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe) from Microsoft
2. Run `YataiDON.exe`


#### Linux
- Run `YataiDON.bin`

## Building from Source

### Prerequisites

- I don't know

### Build Steps

1. Clone the repository:
```bash
git clone https://github.com/Yonokid/YataiDON
cd YataiDON
```

2. Compile:
```bash
./build_release.sh
```

## Controls

- Press **F1** during gameplay for quick restart
- Press **ESC** during any screen to go back
- Press **Space Bar** during song select to favorite a song
- Press **Left Control** or **Right Control** to skip songs by 10
- Generic drum keybinds can be customized in `config.toml` or through the in-game settings menu

## Contributing
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Yonokid/YataiDON)

Contributions are welcome! Please keep in mind:
- Be mindful of existing built-in functions for animations, videos, and other features. Nearly everything has been abstracted and the libs folder has proper documentation for usage.
- You can also check the [DeepWiki](https://deepwiki.com/Yonokid/YataiDON) page for a detailed explanation of any code.
- Check the [issues page](https://github.com/Yonokid/YataiDON/issues) for enhancements and bugs before starting work
- Feel free to open new issues for bugs or feature requests

## Known Issues

See the [issues page](https://github.com/Yonokid/YataiDON/issues) or the Discord for current bugs and planned enhancements.

## License

This project is licensed under the terms specified in the LICENSE file.

## Acknowledgments

Built with [raylib](https://www.raylib.com/) - A simple and easy-to-use library to enjoy videogames programming.
More credits coming soon

## Video demo

[![DEMO VIDEO](https://img.youtube.com/vi/b-2pODPl0II/0.jpg)](https://www.youtube.com/watch?v=b-2pODPl0II)

---
