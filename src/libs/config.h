#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <toml++/toml.h>
#include <vector>

namespace fs = std::filesystem;

struct GeneralConfig {
    bool fps_counter = false;
    int audio_offset = 0;
    int visual_offset = 0;
    std::string language;
    bool song_timer = false;
    bool judge_counter = false;
    bool show_timing_offset = false;
    std::string log_level;
    int practice_mode_bar_delay = 0;
    std::string score_method;
    bool display_bpm = false;
    int webcam_number = -1;
    bool touch_input = false;
    bool skin_updater = true;
    bool fast_transitions = false;
};

struct LimitsConfig {
    bool timer_frozen = false;
    int song_limit = 0;
    int practice_time_limit = 0; // seconds, 0 = unlimited
};

struct CardReaderConfig {
    bool enabled = true;
    std::string port = "/dev/ttyUSB0";
    int baudrate = 38400;
    int poll_interval_ms = 1000;
};

struct NetworkConfig {
    std::string api_key;
    std::string access_code_1;
    std::string access_code_2;
    bool online_play = false;
    bool sync_scores = false;
    bool auto_login = false;
};

struct PathsConfig {
    std::vector<fs::path> tja_path;
    fs::path skin;
};

struct KeysConfig {
    int exit_key = 0;
    int fullscreen_key = 0;
    int borderless_key = 0;
    int pause_key = 0;
    int back_key = 0;
    int restart_key = 0;
};

struct Keys1PConfig {
    std::vector<int> left_kat;
    std::vector<int> left_don;
    std::vector<int> right_don;
    std::vector<int> right_kat;
};

struct Keys2PConfig {
    std::vector<int> left_kat;
    std::vector<int> left_don;
    std::vector<int> right_don;
    std::vector<int> right_kat;
};

struct GamepadConfig {
    std::vector<int> left_kat;
    std::vector<int> left_don;
    std::vector<int> right_don;
    std::vector<int> right_kat;
};

struct MidiConfig {
    // Case-sensitive substring of the MIDI port name; empty disables MIDI.
    std::string device;
    // MIDI channels are exposed to config as 1-16; 0 accepts any channel.
    int channel = 0;
    // Parallel arrays: MIDI note -> gamepad button number.
    std::vector<int> notes;
    std::vector<int> buttons;
};

struct AudioConfig {
    int device_type = 0;
    std::string device; // Output device name (empty = system default)
    int sample_rate = 44100;
    int buffer_size = 512;
    std::vector<int> asio_channel;
};

struct VolumeConfig {
    float global = 1.0f;
    float sound = 1.0f;
    float music = 1.0f;
    float voice = 1.0f;
    float hitsound = 1.0f;
    float attract_mode = 1.0f;
};

struct VideoConfig {
    bool fullscreen = false;
    bool borderless = false;
    int target_fps = 60;
    bool vsync = true;
};

struct Config {
    GeneralConfig general;
    LimitsConfig limits;
    CardReaderConfig card_reader;
    NetworkConfig network;
    PathsConfig paths;
    KeysConfig keys;
    Keys1PConfig keys_1p;
    Keys2PConfig keys_2p;
    GamepadConfig gamepad_1p;
    GamepadConfig gamepad_2p;
    MidiConfig midi;
    AudioConfig audio;
    VolumeConfig volume;
    VideoConfig video;
};

std::string getKeyString(int key_code);

std::vector<int> parseIntArray(const toml::array& arr);

Config get_config();

void save_config(const Config& config);

// Writes a default config.toml (or dev-config.toml) when none exists yet, so
// the game ships without a bundled config and generates one on first boot.
void ensure_config_file(const Config& config);
