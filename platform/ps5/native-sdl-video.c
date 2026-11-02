/* SPDX-License-Identifier: GPL-3.0-or-later
 * SDL owns input and the logical window; bgfx owns the Vulkan display surface.
 */
#include "SDL_internal.h"
#include "SDL_hints.h"
#include "video/SDL_sysvideo.h"
#include "events/SDL_keyboard_c.h"
#include "native-ime.h"
extern int yataidon_ps5_display_size(int*, int*);
static int video_init(_THIS) {
    int w = 0, h = 0;
    if (yataidon_ps5_display_size(&w, &h) != 0) return SDL_SetError("Vulkan display discovery failed");
    SDL_DisplayMode mode = {SDL_PIXELFORMAT_ABGR8888, w, h, 60, NULL};
    int index = SDL_AddBasicVideoDisplay(&mode);
    if (index < 0) return -1;
    return SDL_AddDisplayMode(&_this->displays[index], &mode) ? 0 : -1;
}
static int create_window(_THIS, SDL_Window* window) {
    (void)_this;
    SDL_SetKeyboardFocus(window);
    return 0;
}
static void video_quit(_THIS) { (void)_this; yataidon_ime_close(); }
static SDL_bool has_keyboard(_THIS) { (void)_this; return SDL_TRUE; }
static SDL_bool keyboard_shown(_THIS, SDL_Window* window) {
    (void)_this; (void)window; return yataidon_ime_active() ? SDL_TRUE : SDL_FALSE;
}
static void show_keyboard(_THIS, SDL_Window* window) {
    (void)_this; (void)window;
    if (yataidon_ime_open(SDL_GetHint("YATAIDON_TEXT_INPUT_INITIAL")) != 0) SDL_SendKeyboardKeyAutoRelease(SDL_SCANCODE_ESCAPE);
}
static void hide_keyboard(_THIS, SDL_Window* window) {
    (void)_this; (void)window; yataidon_ime_close();
}
static void pump_events(_THIS) {
    (void)_this;
    char text[1025];
    int result = yataidon_ime_poll(text, sizeof(text));
    if (result == YATAIDON_IME_CONFIRMED) {
        // SDL text events hold 31 bytes. Preserve UTF-8 boundaries for long queries.
        const char* at = text;
        while (*at) {
            char chunk[SDL_TEXTINPUTEVENT_TEXT_SIZE];
            size_t length = SDL_strlen(at);
            size_t n = length < sizeof(chunk) ? length : sizeof(chunk) - 1;
            while (n && ((unsigned char)at[n] & 0xc0) == 0x80) --n;
            if (!n) break;
            SDL_memcpy(chunk, at, n); chunk[n] = 0;
            SDL_SendKeyboardText(chunk); at += n;
        }
        SDL_SendKeyboardKeyAutoRelease(SDL_SCANCODE_RETURN);
    } else if (result == YATAIDON_IME_CANCELED) {
        SDL_SendKeyboardKeyAutoRelease(SDL_SCANCODE_ESCAPE);
    }
}
static void delete_device(SDL_VideoDevice* device) { SDL_free(device); }
static SDL_VideoDevice* create_device(void) {
    SDL_VideoDevice* device = SDL_calloc(1, sizeof(*device));
    if (!device) { SDL_OutOfMemory(); return NULL; }
    device->VideoInit = video_init;
    device->VideoQuit = video_quit;
    device->PumpEvents = pump_events;
    device->CreateSDLWindow = create_window;
    device->HasScreenKeyboardSupport = has_keyboard;
    device->ShowScreenKeyboard = show_keyboard;
    device->HideScreenKeyboard = hide_keyboard;
    device->IsScreenKeyboardShown = keyboard_shown;
    device->free = delete_device;
    return device;
}
VideoBootStrap PS5_bootstrap = {"ps5", "PS5 native Vulkan display", create_device, NULL};
