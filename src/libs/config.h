#pragma once

#include <raylib.h>
#include <algorithm>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <toml++/toml.h>

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

static std::string getKeyString(int key_code) {
    // Handle alphanumeric keys
    if (key_code >= 65 && key_code <= 90) {
        return std::string(1, static_cast<char>(key_code));
    }
    if (key_code >= 48 && key_code <= 57) {
        return std::string(1, static_cast<char>(key_code));
    }

    // Map raylib key codes to strings
    static const std::map<int, std::string> key_map = {
        {KEY_SPACE, "space"},
        {KEY_ESCAPE, "escape"},
        {KEY_ENTER, "enter"},
        {KEY_TAB, "tab"},
        {KEY_BACKSPACE, "backspace"},
        {KEY_INSERT, "insert"},
        {KEY_DELETE, "delete"},
        {KEY_RIGHT, "right"},
        {KEY_LEFT, "left"},
        {KEY_DOWN, "down"},
        {KEY_UP, "up"},
        {KEY_PAGE_UP, "page_up"},
        {KEY_PAGE_DOWN, "page_down"},
        {KEY_HOME, "home"},
        {KEY_END, "end"},
        {KEY_CAPS_LOCK, "caps_lock"},
        {KEY_SCROLL_LOCK, "scroll_lock"},
        {KEY_NUM_LOCK, "num_lock"},
        {KEY_PRINT_SCREEN, "print_screen"},
        {KEY_PAUSE, "pause"},
        {KEY_F1, "f1"}, {KEY_F2, "f2"}, {KEY_F3, "f3"}, {KEY_F4, "f4"},
        {KEY_F5, "f5"}, {KEY_F6, "f6"}, {KEY_F7, "f7"}, {KEY_F8, "f8"},
        {KEY_F9, "f9"}, {KEY_F10, "f10"}, {KEY_F11, "f11"}, {KEY_F12, "f12"},
        {KEY_LEFT_SHIFT, "left_shift"},
        {KEY_LEFT_CONTROL, "left_control"},
        {KEY_LEFT_ALT, "left_alt"},
        {KEY_LEFT_SUPER, "left_super"},
        {KEY_RIGHT_SHIFT, "right_shift"},
        {KEY_RIGHT_CONTROL, "right_control"},
        {KEY_RIGHT_ALT, "right_alt"},
        {KEY_RIGHT_SUPER, "right_super"},
        {KEY_KB_MENU, "kb_menu"},
        {KEY_KP_0, "kp_0"}, {KEY_KP_1, "kp_1"}, {KEY_KP_2, "kp_2"},
        {KEY_KP_3, "kp_3"}, {KEY_KP_4, "kp_4"}, {KEY_KP_5, "kp_5"},
        {KEY_KP_6, "kp_6"}, {KEY_KP_7, "kp_7"}, {KEY_KP_8, "kp_8"},
        {KEY_KP_9, "kp_9"},
        {KEY_KP_DECIMAL, "kp_decimal"},
        {KEY_KP_DIVIDE, "kp_divide"},
        {KEY_KP_MULTIPLY, "kp_multiply"},
        {KEY_KP_SUBTRACT, "kp_subtract"},
        {KEY_KP_ADD, "kp_add"},
        {KEY_KP_ENTER, "kp_enter"},
        {KEY_KP_EQUAL, "kp_equal"},
        {KEY_APOSTROPHE, "apostrophe"},
        {KEY_COMMA, "comma"},
        {KEY_MINUS, "minus"},
        {KEY_PERIOD, "period"},
        {KEY_SLASH, "slash"},
        {KEY_SEMICOLON, "semicolon"},
        {KEY_EQUAL, "equal"},
        {KEY_LEFT_BRACKET, "left_bracket"},
        {KEY_BACKSLASH, "backslash"},
        {KEY_RIGHT_BRACKET, "right_bracket"},
        {KEY_GRAVE, "grave"}
    };

    auto it = key_map.find(key_code);
    if (it != key_map.end()) {
        return it->second;
    }

    throw std::runtime_error("Unknown key code: " + std::to_string(key_code));
}

static int getKeyCode(const std::string& key) {
    // Handle single alphanumeric characters
    if (key.length() == 1 && std::isalnum(key[0])) {
        return std::toupper(key[0]);
    }

    // Convert to uppercase for comparison
    std::string upper_key = key;
    std::transform(upper_key.begin(), upper_key.end(), upper_key.begin(), ::toupper);

    // Map strings to raylib key codes
    static const std::map<std::string, int> key_map = {
        {"SPACE", KEY_SPACE},
        {"ESCAPE", KEY_ESCAPE},
        {"ENTER", KEY_ENTER},
        {"TAB", KEY_TAB},
        {"BACKSPACE", KEY_BACKSPACE},
        {"INSERT", KEY_INSERT},
        {"DELETE", KEY_DELETE},
        {"RIGHT", KEY_RIGHT},
        {"LEFT", KEY_LEFT},
        {"DOWN", KEY_DOWN},
        {"UP", KEY_UP},
        {"PAGE_UP", KEY_PAGE_UP},
        {"PAGE_DOWN", KEY_PAGE_DOWN},
        {"HOME", KEY_HOME},
        {"END", KEY_END},
        {"CAPS_LOCK", KEY_CAPS_LOCK},
        {"SCROLL_LOCK", KEY_SCROLL_LOCK},
        {"NUM_LOCK", KEY_NUM_LOCK},
        {"PRINT_SCREEN", KEY_PRINT_SCREEN},
        {"PAUSE", KEY_PAUSE},
        {"F1", KEY_F1}, {"F2", KEY_F2}, {"F3", KEY_F3}, {"F4", KEY_F4},
        {"F5", KEY_F5}, {"F6", KEY_F6}, {"F7", KEY_F7}, {"F8", KEY_F8},
        {"F9", KEY_F9}, {"F10", KEY_F10}, {"F11", KEY_F11}, {"F12", KEY_F12},
        {"LEFT_SHIFT", KEY_LEFT_SHIFT},
        {"LEFT_CONTROL", KEY_LEFT_CONTROL},
        {"LEFT_ALT", KEY_LEFT_ALT},
        {"LEFT_SUPER", KEY_LEFT_SUPER},
        {"RIGHT_SHIFT", KEY_RIGHT_SHIFT},
        {"RIGHT_CONTROL", KEY_RIGHT_CONTROL},
        {"RIGHT_ALT", KEY_RIGHT_ALT},
        {"RIGHT_SUPER", KEY_RIGHT_SUPER},
        {"KB_MENU", KEY_KB_MENU},
        {"KP_0", KEY_KP_0}, {"KP_1", KEY_KP_1}, {"KP_2", KEY_KP_2},
        {"KP_3", KEY_KP_3}, {"KP_4", KEY_KP_4}, {"KP_5", KEY_KP_5},
        {"KP_6", KEY_KP_6}, {"KP_7", KEY_KP_7}, {"KP_8", KEY_KP_8},
        {"KP_9", KEY_KP_9},
        {"KP_DECIMAL", KEY_KP_DECIMAL},
        {"KP_DIVIDE", KEY_KP_DIVIDE},
        {"KP_MULTIPLY", KEY_KP_MULTIPLY},
        {"KP_SUBTRACT", KEY_KP_SUBTRACT},
        {"KP_ADD", KEY_KP_ADD},
        {"KP_ENTER", KEY_KP_ENTER},
        {"KP_EQUAL", KEY_KP_EQUAL},
        {"APOSTROPHE", KEY_APOSTROPHE},
        {"COMMA", KEY_COMMA},
        {"MINUS", KEY_MINUS},
        {"PERIOD", KEY_PERIOD},
        {"SLASH", KEY_SLASH},
        {"SEMICOLON", KEY_SEMICOLON},
        {"EQUAL", KEY_EQUAL},
        {"LEFT_BRACKET", KEY_LEFT_BRACKET},
        {"BACKSLASH", KEY_BACKSLASH},
        {"RIGHT_BRACKET", KEY_RIGHT_BRACKET},
        {"GRAVE", KEY_GRAVE}
    };

    auto it = key_map.find(upper_key);
    if (it != key_map.end()) {
        return it->second;
    }

    throw std::runtime_error("Invalid key: " + key);
}

static std::vector<int> parseKeyArray(const toml::array& arr) {
    std::vector<int> result;
    for (const auto& elem : arr) {
        if (elem.is_string()) {
            result.push_back(getKeyCode(elem.as_string()->get()));
        }
    }
    return result;
}

static std::vector<fs::path> parsePathArray(const toml::array& arr) {
    std::vector<fs::path> result;
    for (const auto& elem : arr) {
        if (elem.is_string()) {
            result.push_back(fs::path(elem.as_string()->get()));
        }
    }
    return result;
}

static Config get_config() {
    fs::path config_path = fs::exists("dev-config.toml") ?
                            fs::path("dev-config.toml") :
                            fs::path("config.toml");

    toml::table config_file;
    try {
        config_file = toml::parse_file(config_path.string());
    } catch (const toml::parse_error& err) {
        throw std::runtime_error("Failed to parse config file: " +
                                std::string(err.what()));
    }

    Config config;

    // Parse general config
    if (auto general = config_file["general"].as_table()) {
        config.general.fps_counter = (*general)["fps_counter"].value_or(false);
        config.general.audio_offset = (*general)["audio_offset"].value_or(0);
        config.general.visual_offset = (*general)["visual_offset"].value_or(0);
        config.general.language = (*general)["language"].value_or("en");
        config.general.hard_judge = (*general)["hard_judge"].value_or(0);
        config.general.touch_enabled = (*general)["touch_enabled"].value_or(false);
        config.general.timer_frozen = (*general)["timer_frozen"].value_or(false);
        config.general.judge_counter = (*general)["judge_counter"].value_or(false);
        config.general.nijiiro_notes = (*general)["nijiiro_notes"].value_or(false);
        config.general.log_level = (*general)["log_level"].value_or(2);
        config.general.fake_online = (*general)["fake_online"].value_or(false);
        config.general.practice_mode_bar_delay = (*general)["practice_mode_bar_delay"].value_or(0);
        config.general.score_method = (*general)["score_method"].value_or("standard");
    }

    // Parse nameplate_1p
    if (auto nameplate = config_file["nameplate_1p"].as_table()) {
        config.nameplate_1p.name = (*nameplate)["name"].value_or("");
        config.nameplate_1p.title = (*nameplate)["title"].value_or("");
        config.nameplate_1p.title_bg = (*nameplate)["title_bg"].value_or(0);
        config.nameplate_1p.dan = (*nameplate)["dan"].value_or(0);
        config.nameplate_1p.gold = (*nameplate)["gold"].value_or(false);
        config.nameplate_1p.rainbow = (*nameplate)["rainbow"].value_or(false);
    }

    // Parse nameplate_2p
    if (auto nameplate = config_file["nameplate_2p"].as_table()) {
        config.nameplate_2p.name = (*nameplate)["name"].value_or("");
        config.nameplate_2p.title = (*nameplate)["title"].value_or("");
        config.nameplate_2p.title_bg = (*nameplate)["title_bg"].value_or(0);
        config.nameplate_2p.dan = (*nameplate)["dan"].value_or(0);
        config.nameplate_2p.gold = (*nameplate)["gold"].value_or(false);
        config.nameplate_2p.rainbow = (*nameplate)["rainbow"].value_or(false);
    }

    // Parse paths
    if (auto paths = config_file["paths"].as_table()) {
        if (auto tja_path = (*paths)["tja_path"].as_array()) {
            config.paths.tja_path = parsePathArray(*tja_path);
        }
        config.paths.skin = fs::path((*paths)["skin"].value_or("default"));
    }

    // Parse keys (converting from strings to key codes)
    if (auto keys = config_file["keys"].as_table()) {
        config.keys.exit_key = getKeyCode((*keys)["exit_key"].value_or("escape"));
        config.keys.fullscreen_key = getKeyCode((*keys)["fullscreen_key"].value_or("f11"));
        config.keys.borderless_key = getKeyCode((*keys)["borderless_key"].value_or("f10"));
        config.keys.pause_key = getKeyCode((*keys)["pause_key"].value_or("p"));
        config.keys.back_key = getKeyCode((*keys)["back_key"].value_or("escape"));
        config.keys.restart_key = getKeyCode((*keys)["restart_key"].value_or("r"));
    }

    // Parse keys_1p
    if (auto keys_1p = config_file["keys_1p"].as_table()) {
        if (auto left_kat = (*keys_1p)["left_kat"].as_array()) {
            config.keys_1p.left_kat = parseKeyArray(*left_kat);
        }
        if (auto left_don = (*keys_1p)["left_don"].as_array()) {
            config.keys_1p.left_don = parseKeyArray(*left_don);
        }
        if (auto right_don = (*keys_1p)["right_don"].as_array()) {
            config.keys_1p.right_don = parseKeyArray(*right_don);
        }
        if (auto right_kat = (*keys_1p)["right_kat"].as_array()) {
            config.keys_1p.right_kat = parseKeyArray(*right_kat);
        }
    }

    // Parse keys_2p
    if (auto keys_2p = config_file["keys_2p"].as_table()) {
        if (auto left_kat = (*keys_2p)["left_kat"].as_array()) {
            config.keys_2p.left_kat = parseKeyArray(*left_kat);
        }
        if (auto left_don = (*keys_2p)["left_don"].as_array()) {
            config.keys_2p.left_don = parseKeyArray(*left_don);
        }
        if (auto right_don = (*keys_2p)["right_don"].as_array()) {
            config.keys_2p.right_don = parseKeyArray(*right_don);
        }
        if (auto right_kat = (*keys_2p)["right_kat"].as_array()) {
            config.keys_2p.right_kat = parseKeyArray(*right_kat);
        }
    }

    // Parse gamepad
    if (auto gamepad = config_file["gamepad"].as_table()) {
        if (auto left_kat = (*gamepad)["left_kat"].as_array()) {
            config.gamepad.left_kat = parseKeyArray(*left_kat);
        }
        if (auto left_don = (*gamepad)["left_don"].as_array()) {
            config.gamepad.left_don = parseKeyArray(*left_don);
        }
        if (auto right_don = (*gamepad)["right_don"].as_array()) {
            config.gamepad.right_don = parseKeyArray(*right_don);
        }
        if (auto right_kat = (*gamepad)["right_kat"].as_array()) {
            config.gamepad.right_kat = parseKeyArray(*right_kat);
        }
    }

    // Parse audio
    if (auto audio = config_file["audio"].as_table()) {
        config.audio.device_type = (*audio)["device_type"].value_or(0);
        config.audio.sample_rate = (*audio)["sample_rate"].value_or(44100);
        config.audio.buffer_size = (*audio)["buffer_size"].value_or(512);
    }

    // Parse volume
    if (auto volume = config_file["volume"].as_table()) {
        config.volume.sound = (*volume)["sound"].value_or(1.0);
        config.volume.music = (*volume)["music"].value_or(1.0);
        config.volume.voice = (*volume)["voice"].value_or(1.0);
        config.volume.hitsound = (*volume)["hitsound"].value_or(1.0);
        config.volume.attract_mode = (*volume)["attract_mode"].value_or(1.0);
    }

    // Parse video
    if (auto video = config_file["video"].as_table()) {
        config.video.fullscreen = (*video)["fullscreen"].value_or(false);
        config.video.borderless = (*video)["borderless"].value_or(false);
        config.video.target_fps = (*video)["target_fps"].value_or(60);
        config.video.vsync = (*video)["vsync"].value_or(true);
    }

    return config;
}

static void save_config(const Config& config) {
    fs::path config_path = fs::exists("dev-config.toml") ?
                            fs::path("dev-config.toml") :
                            fs::path("config.toml");

    toml::table config_table;

    // General
    config_table.insert("general", toml::table{
        {"fps_counter", config.general.fps_counter},
        {"audio_offset", config.general.audio_offset},
        {"visual_offset", config.general.visual_offset},
        {"language", config.general.language},
        {"hard_judge", config.general.hard_judge},
        {"touch_enabled", config.general.touch_enabled},
        {"timer_frozen", config.general.timer_frozen},
        {"judge_counter", config.general.judge_counter},
        {"nijiiro_notes", config.general.nijiiro_notes},
        {"log_level", config.general.log_level},
        {"fake_online", config.general.fake_online},
        {"practice_mode_bar_delay", config.general.practice_mode_bar_delay},
        {"score_method", config.general.score_method}
    });

    // Nameplate 1P
    config_table.insert("nameplate_1p", toml::table{
        {"name", config.nameplate_1p.name},
        {"title", config.nameplate_1p.title},
        {"title_bg", config.nameplate_1p.title_bg},
        {"dan", config.nameplate_1p.dan},
        {"gold", config.nameplate_1p.gold},
        {"rainbow", config.nameplate_1p.rainbow}
    });

    // Nameplate 2P
    config_table.insert("nameplate_2p", toml::table{
        {"name", config.nameplate_2p.name},
        {"title", config.nameplate_2p.title},
        {"title_bg", config.nameplate_2p.title_bg},
        {"dan", config.nameplate_2p.dan},
        {"gold", config.nameplate_2p.gold},
        {"rainbow", config.nameplate_2p.rainbow}
    });

    // Paths
    toml::array tja_path_array;
    for (const auto& path : config.paths.tja_path) {
        tja_path_array.push_back(path.string());
    }
    config_table.insert("paths", toml::table{
        {"tja_path", tja_path_array},
        {"skin", config.paths.skin.string()}
    });

    // Keys
    config_table.insert("keys", toml::table{
        {"exit_key", getKeyString(config.keys.exit_key)},
        {"fullscreen_key", getKeyString(config.keys.fullscreen_key)},
        {"borderless_key", getKeyString(config.keys.borderless_key)},
        {"pause_key", getKeyString(config.keys.pause_key)},
        {"back_key", getKeyString(config.keys.back_key)},
        {"restart_key", getKeyString(config.keys.restart_key)}
    });

    // Keys 1P
    toml::array left_kat_1p, left_don_1p, right_don_1p, right_kat_1p;
    for (int key : config.keys_1p.left_kat) left_kat_1p.push_back(getKeyString(key));
    for (int key : config.keys_1p.left_don) left_don_1p.push_back(getKeyString(key));
    for (int key : config.keys_1p.right_don) right_don_1p.push_back(getKeyString(key));
    for (int key : config.keys_1p.right_kat) right_kat_1p.push_back(getKeyString(key));

    config_table.insert("keys_1p", toml::table{
        {"left_kat", left_kat_1p},
        {"left_don", left_don_1p},
        {"right_don", right_don_1p},
        {"right_kat", right_kat_1p}
    });

    // Keys 2P
    toml::array left_kat_2p, left_don_2p, right_don_2p, right_kat_2p;
    for (int key : config.keys_2p.left_kat) left_kat_2p.push_back(getKeyString(key));
    for (int key : config.keys_2p.left_don) left_don_2p.push_back(getKeyString(key));
    for (int key : config.keys_2p.right_don) right_don_2p.push_back(getKeyString(key));
    for (int key : config.keys_2p.right_kat) right_kat_2p.push_back(getKeyString(key));

    config_table.insert("keys_2p", toml::table{
        {"left_kat", left_kat_2p},
        {"left_don", left_don_2p},
        {"right_don", right_don_2p},
        {"right_kat", right_kat_2p}
    });

    // Gamepad
    toml::array gp_left_kat, gp_left_don, gp_right_don, gp_right_kat;
    for (int key : config.gamepad.left_kat) gp_left_kat.push_back(getKeyString(key));
    for (int key : config.gamepad.left_don) gp_left_don.push_back(getKeyString(key));
    for (int key : config.gamepad.right_don) gp_right_don.push_back(getKeyString(key));
    for (int key : config.gamepad.right_kat) gp_right_kat.push_back(getKeyString(key));

    config_table.insert("gamepad", toml::table{
        {"left_kat", gp_left_kat},
        {"left_don", gp_left_don},
        {"right_don", gp_right_don},
        {"right_kat", gp_right_kat}
    });

    // Audio
    config_table.insert("audio", toml::table{
        {"device_type", config.audio.device_type},
        {"sample_rate", config.audio.sample_rate},
        {"buffer_size", config.audio.buffer_size}
    });

    // Volume
    config_table.insert("volume", toml::table{
        {"sound", config.volume.sound},
        {"music", config.volume.music},
        {"voice", config.volume.voice},
        {"hitsound", config.volume.hitsound},
        {"attract_mode", config.volume.attract_mode}
    });

    // Video
    config_table.insert("video", toml::table{
        {"fullscreen", config.video.fullscreen},
        {"borderless", config.video.borderless},
        {"target_fps", config.video.target_fps},
        {"vsync", config.video.vsync}
    });

    // Write to file
    std::ofstream ofs(config_path);
    ofs << config_table;
};
