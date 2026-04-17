#pragma once

#include "global_data.h"
#include "texture.h"
#include <mutex>
#include <atomic>
#ifndef __EMSCRIPTEN__
#include <thread>

extern std::atomic<bool> input_thread_running;
extern std::thread input_thread;
#endif

extern std::mutex input_mutex;
extern std::vector<int> pressed_keys;
extern std::vector<int> released_keys;

// Start the input polling thread (desktop only).
void input_polling_thread();

// On web, call this once per frame instead of spawning a thread.
void poll_keyboard_once();

// Check if a key was pressed since the last check
// This consumes the key press event
bool check_key_pressed(int key);

// Check if a key was released since the last check
// This consumes the key release event
bool check_key_released(int key);

// Clear all buffered input events
// Useful when changing screens or locking input
void clear_input_buffers();

#ifndef __EMSCRIPTEN__
// Close any open SDL joystick handles (call before SDL shutdown)
void shutdown_sdl_joysticks();
#endif

bool is_input_key_pressed(const std::vector<int>& keys, const std::vector<int>& gamepad_buttons);
bool is_l_don_pressed(PlayerNum player_num = PlayerNum::ALL);
bool is_r_don_pressed(PlayerNum player_num = PlayerNum::ALL);
bool is_l_kat_pressed(PlayerNum player_num = PlayerNum::ALL);
bool is_r_kat_pressed(PlayerNum player_num = PlayerNum::ALL);

inline bool operator==(const ray::Color& a, const ray::Color& b)
{
    return a.r == b.r &&
           a.g == b.g &&
           a.b == b.b &&
           a.a == b.a;
}

inline bool operator!=(const ray::Color& a, const ray::Color& b)
{
    return !(a == b);
}
