#pragma once

#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "../gfx.h" // IWYU pragma: keep
#include "../gfx/sdl_compat.h"
#include "../global_data.h"

extern std::atomic<bool> input_thread_running;
extern std::thread input_thread;

extern std::atomic<bool> touch_drum_pressed;

void input_polling_thread();
void poll_keyboard_once();
void poll_touch_once();

// --- Raw keyboard / mouse / clipboard (SDL) --------------------------------
// Keys are SDL_Scancode values. Per-frame state (pressed edges, typed characters, wheel) is
// snapshotted by poll_input_events(), which replaces poll_window_events().
void poll_input_events();
bool key_down(int scancode);
bool key_pressed(int scancode);
// Next key pressed this frame (scancode), 0 when none left; consumes it.
int key_pressed_next();
// Next typed codepoint this frame, 0 when none left; consumes it.
int char_pressed();
float mouse_wheel();
Vec2 mouse_position();
bool mouse_down(int button); // SDL_BUTTON_LEFT etc.
bool mouse_pressed(int button);
void show_cursor();
void hide_cursor();

// Check if a key was pressed since the last check
// This consumes the key press event
bool check_key_pressed(int key, float* strength = nullptr);

// Check if a key was released since the last check
// This consumes the key release event
bool check_key_released(int key);

// Most recent controller button press, as the bare button number the
// config stores (-1 if none since the last call, which this consumes; 0 is
// a valid real button, so it can't double as the "no event" sentinel).
// Covers mapped gamepads and plain SDL joysticks alike.
int take_gamepad_button_pressed();

// Inject a discrete gamepad-style press into the shared input buffer. Button
// numbers use the same IDs stored in config.toml.
void submit_gamepad_button_press(int button, float strength = 1.0f);

void start_midi_input(const MidiConfig& config);
void process_midi_events();
void shutdown_midi_input();

double get_last_input_ms();

// Clear all buffered input events
// Useful when changing screens or locking input
void clear_input_buffers();
void shutdown_sdl_joysticks();

// Platform-independent keyboard visibility control
void set_keyboard_visible(bool visible, const std::string& initial_text = {});

bool is_text_input_key(int key);

// Enable/disable touch drum
void set_touch_drum_enabled(bool enabled);

// Latest finger press since the last call, 0..1 across the window; works with the drum disabled
std::optional<Vec2> take_touch_tap();

// Draw the touch drum overlay (returns true if drawn, false if hidden)
bool draw_touch_drum();

bool is_input_key_pressed(const std::vector<int>& keys, const std::vector<int>& gamepad_buttons,
                          float* strength = nullptr);
bool is_l_don_pressed(PlayerNum player_num = PlayerNum::ALL, float* strength = nullptr);
bool is_r_don_pressed(PlayerNum player_num = PlayerNum::ALL, float* strength = nullptr);
bool is_l_kat_pressed(PlayerNum player_num = PlayerNum::ALL, float* strength = nullptr);
bool is_r_kat_pressed(PlayerNum player_num = PlayerNum::ALL, float* strength = nullptr);
// Menu navigation: drum input plus arrow keys / Enter (not for P2)
bool menu_left_pressed(PlayerNum player_num = PlayerNum::ALL);
bool menu_right_pressed(PlayerNum player_num = PlayerNum::ALL);
bool menu_confirm_pressed(PlayerNum player_num = PlayerNum::ALL);

bool is_kat_held(bool left, PlayerNum player_num = PlayerNum::ALL);

// --- Unified keyboard text editing ---------------------------------------
// Result of one frame of keyboard-driven text-field editing.
enum class TextEditAction {
    None,
    Confirm,
    Cancel
};

// Polls the keyboard for text-field editing and applies the edits to `text`:
//   * Backspace removes one UTF-8 code point.
//   * Ctrl+V pastes the clipboard (first line only, UTF-8).
//   * Typed characters (GetCharPressed) are appended as UTF-8; `accept` may
//     reject a code point (return false to skip it).
//   * Enter / Escape (IsKeyPressed) and a typed '\n'/'\r' map to
//     Confirm / Cancel, which are returned so the caller can finalize or
//     abort its edit session.
TextEditAction poll_text_edit(std::string& text, const std::function<bool(int)>& accept = nullptr);

// Desktop: keeps SDL text input (and with it the OS IME) on only while a text field is being
// edited, i.e. poll_text_edit ran since the last call. Call once per frame after the screen update.
void sync_text_input();
