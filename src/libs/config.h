#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <toml++/toml.h>

#include "ray.h"

namespace fs = std::filesystem;

struct GeneralConfig {
    bool fps_counter;
    int audio_offset;
    int visual_offset;
    std::string language;
    int hard_judge;
    bool touch_enabled;
    bool timer_frozen;
    bool judge_counter;
    bool nijiiro_notes;
    int log_level;
    bool fake_online;
    int practice_mode_bar_delay;
    std::string score_method;
};

struct NameplateConfig {
    std::string name;
    std::string title;
    int title_bg;
    int dan;
    bool gold;
    bool rainbow;
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
    NameplateConfig nameplate_1p;
    NameplateConfig nameplate_2p;
    PathsConfig paths;
    KeysConfig keys;
    Keys1PConfig keys_1p;
    Keys2PConfig keys_2p;
    GamepadConfig gamepad;
    AudioConfig audio;
    VolumeConfig volume;
    VideoConfig video;
};

static std::string getKeyString(int key_code);

static int getKeyCode(const std::string& key);

static std::vector<int> parseKeyArray(const toml::array& arr);

static std::vector<fs::path> parsePathArray(const toml::array& arr);

Config get_config();

static void save_config(const Config& config);
