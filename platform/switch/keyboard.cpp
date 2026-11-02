#include "keyboard.h"

#include <SDL.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <switch.h>

namespace {
    bool shown = false;
    bool replacement = false;
    bool pending = false;
    bool accepted = false;
    unsigned released_frames = 0;
    std::string committed;

    void send_key(SDL_Scancode scancode, SDL_Keycode key) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.state = SDL_PRESSED;
        event.key.keysym.scancode = scancode;
        event.key.keysym.sym = key;
        SDL_PushEvent(&event);
        event.type = SDL_KEYUP;
        event.key.state = SDL_RELEASED;
        SDL_PushEvent(&event);
    }
} // namespace

extern "C" void yataidon_switch_show_keyboard() {
    SwkbdConfig keyboard;
    Result result = swkbdCreate(&keyboard, 0);
    if (R_FAILED(result)) {
        accepted = false;
        pending = true;
        released_frames = 0;
        return;
    }
    swkbdConfigMakePresetDefault(&keyboard);
    swkbdConfigSetStringLenMax(&keyboard, 511);
    swkbdConfigSetStringLenMin(&keyboard, 0);
    if (const char* initial = SDL_GetHint("YATAIDON_TEXT_INPUT_INITIAL")) {
        swkbdConfigSetInitialText(&keyboard, initial);
    }
    char text[2048]{};
    shown = true;
    result = swkbdShow(&keyboard, text, sizeof(text));
    shown = false;
    swkbdClose(&keyboard);
    accepted = R_SUCCEEDED(result);
    committed = accepted ? text : "";
    pending = true;
    released_frames = 0;
}

void switch_poll_keyboard() {
    if (!pending) {
        return;
    }
    PadState pad;
    padInitializeAny(&pad);
    padUpdate(&pad);
    if (padGetButtons(&pad) & 0xffff) {
        released_frames = 0;
        return;
    }
    // Let the opener's button guard observe its release before publishing text.
    if (++released_frames < 2) {
        return;
    }
    pending = false;
    if (!accepted) {
        send_key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
        return;
    }
    replacement = true;
    const char* cursor = committed.c_str();
    size_t remaining = committed.size();
    while (remaining) {
        SDL_Event event{};
        event.type = SDL_TEXTINPUT;
        size_t count = std::min(remaining, sizeof(event.text.text) - 1);
        while (count < remaining && (static_cast<unsigned char>(cursor[count]) & 0xC0) == 0x80) {
            --count;
        }
        if (!count) {
            break;
        }
        std::memcpy(event.text.text, cursor, count);
        SDL_PushEvent(&event);
        cursor += count;
        remaining -= count;
    }
    send_key(SDL_SCANCODE_RETURN, SDLK_RETURN);
}

extern "C" int yataidon_switch_keyboard_shown() {
    return shown;
}

extern "C" void yataidon_switch_cancel_keyboard_result() {
    pending = false;
    replacement = false;
    committed.clear();
}

bool switch_take_text_replacement() {
    const bool value = replacement;
    replacement = false;
    return value;
}
