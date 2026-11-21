# YataiDON for Nintendo Switch 2

This target uses [ounce-devkit](https://github.com/ouncebrew/ounce-devkit):
libounce provides the libnx-compatible runtime and OunceVK provides the statically
linked Vulkan driver. The existing devkitPro SDL2 and media libraries are reused.

## Build

Install devkitPro and ounce-devkit, then provide a host `shaderc` built from
bgfx.cmake v1.167.9590-598. In a devkitPro shell:

```sh
export YATAIDON_SHADERC=/path/to/host/shaderc
bash platform/switch2/build-game.sh
```

The output is `platform/switch2/build/YataiDON.nro`. Copy it to
`sdmc:/switch/YataiDON/` and use the same `Skins/` and `Songs/` directories as
on Switch.

## Display

Handheld rendering uses 1920x1080. The video settings offer 60 and 120 FPS;
docked rendering uses 3840x2160 at 60 FPS and 2560x1440 at 120 FPS. The window
requests the matching native presentation rate, with 60 FPS as the 120 Hz fallback.
The system and connected display must permit 120 Hz for presentation to reach 120 Hz.
Vulkan is the only graphics API on this target.

Controller input, storage, audio, configuration, and game behavior follow the
Switch version.
