#pragma once

#if !defined(YATAIDON_USE_SDL2) && defined(__has_include)
#if !__has_include(<SDL3/SDL.h>)
#define YATAIDON_USE_SDL2 1
#endif
#endif

#ifndef YATAIDON_USE_SDL2

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#define YATAIDON_SDL_CAMERA 1
using YD_EventFilterRet = bool;

inline SDL_Scancode yd_key_scancode(const SDL_Event& e) {
    return e.key.scancode;
}
inline SDL_FingerID yd_finger_id(const SDL_Event& e) {
    return e.tfinger.fingerID;
}

#else // SDL2

#if defined(__has_include) && __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

#define YATAIDON_SDL_CAMERA 0
struct SDL_Camera; // opaque, never created on SDL2
using YD_EventFilterRet = int;

static_assert(SDL_VERSION_ATLEAST(2, 0, 14), "SDL2 compat needs SDL 2.0.14 or newer");

using SDL_IOStream = SDL_RWops;

#define SDL_BUTTON_MASK SDL_BUTTON
#define SDL_EVENT_QUIT SDL_QUIT
#define SDL_EVENT_TEXT_INPUT SDL_TEXTINPUT
#define SDL_EVENT_KEY_DOWN SDL_KEYDOWN
#define SDL_EVENT_KEY_UP SDL_KEYUP
#define SDL_EVENT_FINGER_DOWN SDL_FINGERDOWN
#define SDL_EVENT_FINGER_UP SDL_FINGERUP
#define SDL_EVENT_FINGER_MOTION SDL_FINGERMOTION
#define SDL_EVENT_MOUSE_WHEEL SDL_MOUSEWHEEL
#define SDL_EVENT_MOUSE_BUTTON_DOWN SDL_MOUSEBUTTONDOWN
#define SDL_SCANCODE_COUNT SDL_NUM_SCANCODES
#define SDL_EVENT_FINGER_CANCELED (SDL_USEREVENT + 0x7ff) // no SDL2 equivalent; never fires
#define SDL_KMOD_NONE KMOD_NONE
#define SDL_AUDIO_F32 AUDIO_F32SYS

#ifndef SDL_HINT_ENABLE_SCREEN_KEYBOARD
#define SDL_HINT_ENABLE_SCREEN_KEYBOARD "SDL_ENABLE_SCREEN_KEYBOARD"
#endif
#define SDL_HINT_AUDIO_DRIVER SDL_HINT_AUDIODRIVER
// Read back by SDL_OpenAudioDeviceStream() below to fill SDL_AudioSpec::samples.
#define SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES "SDL_AUDIO_DEVICE_SAMPLE_FRAMES"
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK ((SDL_AudioDeviceID)0xFFFFFFFFu)

#define SDL_InitSubSystem(flags) (::SDL_InitSubSystem(flags) == 0)

inline SDL_IOStream* SDL_IOFromFile(const char* file, const char* mode) {
    return SDL_RWFromFile(file, mode);
}
inline Sint64 SDL_GetIOSize(SDL_IOStream* io) {
    return SDL_RWsize(io);
}
inline size_t SDL_ReadIO(SDL_IOStream* io, void* buf, size_t size) {
    return SDL_RWread(io, buf, 1, size); // SDL3 returns bytes; with a 1-byte element so does SDL2
}
inline bool SDL_CloseIO(SDL_IOStream* io) {
    return SDL_RWclose(io) == 0;
}

inline SDL_Scancode yd_key_scancode(const SDL_Event& e) {
    return e.key.keysym.scancode;
}
inline SDL_FingerID yd_finger_id(const SDL_Event& e) {
    return e.tfinger.fingerId;
}
inline SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key, SDL_Keymod* modstate) {
    if (modstate) {
        *modstate = KMOD_NONE;
    }
    return ::SDL_GetScancodeFromKey(key);
}
inline void SDL_StartTextInput(SDL_Window*) {
    ::SDL_StartTextInput();
}
inline void SDL_StopTextInput(SDL_Window*) {
    ::SDL_StopTextInput();
}

// ---- Joysticks (SDL3 uses instance IDs, SDL2 enumerates by index) --------------------------

inline int yd_joy_index(SDL_JoystickID id) {
    const int n = SDL_NumJoysticks();
    for (int i = 0; i < n; i++) {
        if (SDL_JoystickGetDeviceInstanceID(i) == id) {
            return i;
        }
    }
    return -1;
}
inline SDL_JoystickID* SDL_GetJoysticks(int* count) {
    const int n = SDL_NumJoysticks();
    auto* ids = static_cast<SDL_JoystickID*>(SDL_malloc((n + 1) * sizeof(SDL_JoystickID)));
    for (int i = 0; i < n; i++) {
        ids[i] = SDL_JoystickGetDeviceInstanceID(i);
    }
    ids[n] = 0;
    if (count) {
        *count = n;
    }
    return ids;
}
inline bool SDL_IsGamepad(SDL_JoystickID id) {
    const int i = yd_joy_index(id);
    return i >= 0 && SDL_IsGameController(i) == SDL_TRUE;
}
using SDL_Gamepad = SDL_GameController;
using SDL_GamepadButton = SDL_GameControllerButton;
#define SDL_ScreenKeyboardShown SDL_IsScreenKeyboardShown
#define SDL_GAMEPAD_BUTTON_SOUTH SDL_CONTROLLER_BUTTON_A
#define SDL_GAMEPAD_BUTTON_EAST SDL_CONTROLLER_BUTTON_B
#define SDL_GAMEPAD_BUTTON_WEST SDL_CONTROLLER_BUTTON_X
#define SDL_GAMEPAD_BUTTON_NORTH SDL_CONTROLLER_BUTTON_Y
#define SDL_GAMEPAD_BUTTON_TOUCHPAD SDL_CONTROLLER_BUTTON_TOUCHPAD
#define SDL_GAMEPAD_BUTTON_BACK SDL_CONTROLLER_BUTTON_BACK
#define SDL_GAMEPAD_BUTTON_GUIDE SDL_CONTROLLER_BUTTON_GUIDE
#define SDL_GAMEPAD_BUTTON_START SDL_CONTROLLER_BUTTON_START
#define SDL_GAMEPAD_BUTTON_LEFT_STICK SDL_CONTROLLER_BUTTON_LEFTSTICK
#define SDL_GAMEPAD_BUTTON_RIGHT_STICK SDL_CONTROLLER_BUTTON_RIGHTSTICK
#define SDL_GAMEPAD_BUTTON_LEFT_SHOULDER SDL_CONTROLLER_BUTTON_LEFTSHOULDER
#define SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
#define SDL_GAMEPAD_BUTTON_DPAD_UP SDL_CONTROLLER_BUTTON_DPAD_UP
#define SDL_GAMEPAD_BUTTON_DPAD_DOWN SDL_CONTROLLER_BUTTON_DPAD_DOWN
#define SDL_GAMEPAD_BUTTON_DPAD_LEFT SDL_CONTROLLER_BUTTON_DPAD_LEFT
#define SDL_GAMEPAD_BUTTON_DPAD_RIGHT SDL_CONTROLLER_BUTTON_DPAD_RIGHT
#define SDL_GAMEPAD_AXIS_LEFT_TRIGGER SDL_CONTROLLER_AXIS_TRIGGERLEFT
#define SDL_GAMEPAD_AXIS_RIGHT_TRIGGER SDL_CONTROLLER_AXIS_TRIGGERRIGHT
inline SDL_Gamepad* SDL_OpenGamepad(SDL_JoystickID id) {
    const int i = yd_joy_index(id);
    return i >= 0 ? SDL_GameControllerOpen(i) : nullptr;
}
inline void SDL_CloseGamepad(SDL_Gamepad* gp) {
    SDL_GameControllerClose(gp);
}
inline const char* SDL_GetGamepadName(SDL_Gamepad* gp) {
    return SDL_GameControllerName(gp);
}
inline bool SDL_GetGamepadButton(SDL_Gamepad* gp, SDL_GamepadButton b) {
    return SDL_GameControllerGetButton(gp, b) != 0;
}
inline Sint16 SDL_GetGamepadAxis(SDL_Gamepad* gp, SDL_GameControllerAxis a) {
    return SDL_GameControllerGetAxis(gp, a);
}
inline SDL_Joystick* SDL_OpenJoystick(SDL_JoystickID id) {
    const int i = yd_joy_index(id);
    return i >= 0 ? SDL_JoystickOpen(i) : nullptr;
}
inline void SDL_CloseJoystick(SDL_Joystick* j) {
    SDL_JoystickClose(j);
}
inline const char* SDL_GetJoystickName(SDL_Joystick* j) {
    return SDL_JoystickName(j);
}
inline int SDL_GetNumJoystickButtons(SDL_Joystick* j) {
    return SDL_JoystickNumButtons(j);
}
inline bool SDL_GetJoystickButton(SDL_Joystick* j, int b) {
    return SDL_JoystickGetButton(j, b) != 0;
}
inline int SDL_GetNumJoystickAxes(SDL_Joystick* j) {
    return SDL_JoystickNumAxes(j);
}
inline Sint16 SDL_GetJoystickAxis(SDL_Joystick* j, int a) {
    return SDL_JoystickGetAxis(j, a);
}

#define SDL_AudioStream YD_AudioStream // SDL2 has its own, unrelated SDL_AudioStream
struct YD_AudioStream;
using SDL_AudioStreamCallback = void (*)(void* userdata, YD_AudioStream* stream, int additional_amount,
                                         int total_amount);

struct YD_AudioStream {
    SDL_AudioDeviceID dev = 0;
    SDL_AudioSpec obtained{};
    SDL_AudioStreamCallback callback = nullptr;
    void* userdata = nullptr;
    Uint8* out = nullptr; // valid only inside the SDL2 callback
    int out_len = 0;
    int written = 0;
};

inline YD_AudioStream*& yd_last_audio_stream() {
    static YD_AudioStream* last = nullptr;
    return last;
}

inline void SDLCALL yd_sdl2_audio_trampoline(void* userdata, Uint8* buf, int len) {
    auto* s = static_cast<YD_AudioStream*>(userdata);
    std::memset(buf, 0, len);
    s->out = buf;
    s->out_len = len;
    s->written = 0;
    if (s->callback) {
        s->callback(s->userdata, s, len, len);
    }
    s->out = nullptr;
}

inline bool SDL_PutAudioStreamData(YD_AudioStream* s, const void* data, int len) {
    if (!s || !s->out) {
        return false; // SDL2 only accepts data from inside the device callback
    }
    const int n = std::min(len, s->out_len - s->written);
    std::memcpy(s->out + s->written, data, n);
    s->written += n;
    return true;
}

// Device IDs on SDL2 are enumeration index + 1 so that 0 stays invalid.
inline SDL_AudioDeviceID* SDL_GetAudioPlaybackDevices(int* count) {
    if (count) {
        *count = 0;
    }
    const int n = SDL_GetNumAudioDevices(0);
    if (n <= 0) {
        return nullptr;
    }
    auto* ids = static_cast<SDL_AudioDeviceID*>(SDL_malloc((n + 1) * sizeof(SDL_AudioDeviceID)));
    if (!ids) {
        return nullptr;
    }
    for (int i = 0; i < n; i++) {
        ids[i] = static_cast<SDL_AudioDeviceID>(i + 1);
    }
    ids[n] = 0;
    if (count) {
        *count = n;
    }
    return ids;
}
// Overload next to SDL2's (int index, int iscapture) version.
inline const char* SDL_GetAudioDeviceName(SDL_AudioDeviceID id) {
    return ::SDL_GetAudioDeviceName(static_cast<int>(id) - 1, 0);
}

inline YD_AudioStream* SDL_OpenAudioDeviceStream(SDL_AudioDeviceID devid, const SDL_AudioSpec* spec,
                                                 SDL_AudioStreamCallback cb, void* userdata) {
    auto* s = new YD_AudioStream;
    s->callback = cb;
    s->userdata = userdata;

    SDL_AudioSpec want = *spec;
    want.samples = 1024;
    if (const char* h = SDL_GetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES)) {
        const int frames = std::atoi(h);
        if (frames > 0) {
            want.samples = static_cast<Uint16>(frames);
        }
    }
    want.callback = yd_sdl2_audio_trampoline;
    want.userdata = s;

    const char* name = devid == SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK ? nullptr : SDL_GetAudioDeviceName(devid);
    s->dev = SDL_OpenAudioDevice(name, 0, &want, &s->obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (s->dev == 0) {
        delete s;
        return nullptr;
    }
    yd_last_audio_stream() = s;
    return s; // starts paused, like SDL3
}
inline bool SDL_PauseAudioStreamDevice(YD_AudioStream* s) {
    SDL_PauseAudioDevice(s->dev, 1);
    return true;
}
inline bool SDL_ResumeAudioStreamDevice(YD_AudioStream* s) {
    SDL_PauseAudioDevice(s->dev, 0);
    return true;
}
inline void SDL_DestroyAudioStream(YD_AudioStream* s) {
    if (!s) {
        return;
    }
    SDL_CloseAudioDevice(s->dev); // waits for the callback to finish
    if (yd_last_audio_stream() == s) {
        yd_last_audio_stream() = nullptr;
    }
    delete s;
}
inline SDL_AudioDeviceID SDL_GetAudioStreamDevice(YD_AudioStream* s) {
    return s ? s->dev : 0;
}
inline bool SDL_GetAudioDeviceFormat(SDL_AudioDeviceID, SDL_AudioSpec* spec, int* sample_frames) {
    YD_AudioStream* s = yd_last_audio_stream();
    if (!s) {
        return false;
    }
    if (spec) {
        *spec = s->obtained;
    }
    if (sample_frames) {
        *sample_frames = s->obtained.samples;
    }
    return true;
}

#endif // YATAIDON_USE_SDL2

#ifdef YATAIDON_USE_SDL2
inline void SDL_GetWindowSizeInPixels(SDL_Window* window, int* w, int* h) {
    SDL_GetWindowSize(window, w, h);
}
#endif

// The game's (only) window, or nullptr before init_window(). Owned by the gfx backend.
SDL_Window* main_window();
