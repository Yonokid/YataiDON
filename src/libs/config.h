#pragma once

#include <toml++/toml.h>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

struct GeneralConfig {
    std::string access_code;
    bool fps_counter;
    int audio_offset;
    int visual_offset;
    std::string language;
    bool timer_frozen;
    bool song_timer;
    bool judge_counter;
    bool nijiiro_notes;
    std::string log_level;
    int practice_mode_bar_delay;
    std::string score_method;
    bool display_bpm;
    int song_limit;
    int webcam_number;
    int player_1_id;
    int player_2_id;
    bool touch_input;
};


struct PathsConfig {
    std::vector<fs::path> tja_path;
    fs::path skin;
};

struct KeysConfig {
    int exit_key;
    int fullscreen_key;
    int borderless_key;
    int pause_key;
    int back_key;
    int restart_key;
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

struct AudioConfig {
    int device_type;
    int sample_rate;
    int buffer_size;
};

struct VolumeConfig {
    float global;
    float sound;
    float music;
    float voice;
    float hitsound;
    float attract_mode;
};

struct VideoConfig {
    bool fullscreen;
    bool borderless;
    int target_fps;
    bool vsync;
};

struct Config {
    GeneralConfig general;
    PathsConfig paths;
    KeysConfig keys;
    Keys1PConfig keys_1p;
    Keys2PConfig keys_2p;
    GamepadConfig gamepad_1p;
    GamepadConfig gamepad_2p;
    AudioConfig audio;
    VolumeConfig volume;
    VideoConfig video;
};

std::string getKeyString(int key_code);

static int getKeyCode(const std::string& key);

static std::vector<int> parseKeyArray(const toml::array& arr);

std::vector<int> parseIntArray(const toml::array& arr);

static std::vector<fs::path> parsePathArray(const toml::array& arr);

Config get_config();

void save_config(const Config& config);
