#include "platform_ios.h"

#include <SDL3/SDL.h>
#include <algorithm>

#include "../libs/gfx/sdl_compat.h"
#include "../libs/global_data.h"
#include "../libs/input/input.h"
#include "platform.h"
#include "platform_mobile.h"

// Platform code that needs no Objective-C. Apple's Foundation headers declare
// a global QuickDraw Rect that clashes with ours, so keep gfx.h out of the .mm.

namespace {
    void (*g_ios_frame_callback)() = nullptr;
} // namespace

// Crash reports come from the OS (Xcode Organizer / device logs).
void platform_install_crash_handlers() {}

void platform_close_window() {
    close_window();
}

void ios_configure_window_flags(unsigned int& flags) {
    // UIKit owns the event loop. SDL_WaitEvent while minimized would prevent
    // UIKit from delivering the foreground event that wakes the game again.
    flags = window_flag::VSYNC | window_flag::ALWAYS_RUN;
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback");
    SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
}

void platform_initialize_after_window() {
    ios_request_audio_buffer();
    mobile_install_lifecycle_handlers();
}

int ios_run_main_loop(void (*run_frame)()) {
    poll_touch_once();
    g_ios_frame_callback = run_frame;

    SDL_Window* win = main_window();
    if (!win) {
        return 1;
    }

    bool registered = SDL_SetiOSAnimationCallback(win, 1, [](void*) { g_ios_frame_callback(); }, nullptr);

    if (!registered) {
        return 1;
    }

    // UIKit owns the loop. No background input thread may outlive main().
    return 0;
}

bool platform_touch_back_button() {
    return true;
}

bool platform_threaded_renderer() {
    return false;
}
