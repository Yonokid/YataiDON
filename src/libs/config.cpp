#include "config.h"

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
        {ray::KEY_SPACE, "space"},
        {ray::KEY_ESCAPE, "escape"},
        {ray::KEY_ENTER, "enter"},
        {ray::KEY_TAB, "tab"},
        {ray::KEY_BACKSPACE, "backspace"},
        {ray::KEY_INSERT, "insert"},
        {ray::KEY_DELETE, "delete"},
        {ray::KEY_RIGHT, "right"},
        {ray::KEY_LEFT, "left"},
        {ray::KEY_DOWN, "down"},
        {ray::KEY_UP, "up"},
        {ray::KEY_PAGE_UP, "page_up"},
        {ray::KEY_PAGE_DOWN, "page_down"},
        {ray::KEY_HOME, "home"},
        {ray::KEY_END, "end"},
        {ray::KEY_CAPS_LOCK, "caps_lock"},
        {ray::KEY_SCROLL_LOCK, "scroll_lock"},
        {ray::KEY_NUM_LOCK, "num_lock"},
        {ray::KEY_PRINT_SCREEN, "print_screen"},
        {ray::KEY_PAUSE, "pause"},
        {ray::KEY_F1, "f1"}, {ray::KEY_F2, "f2"}, {ray::KEY_F3, "f3"}, {ray::KEY_F4, "f4"},
        {ray::KEY_F5, "f5"}, {ray::KEY_F6, "f6"}, {ray::KEY_F7, "f7"}, {ray::KEY_F8, "f8"},
        {ray::KEY_F9, "f9"}, {ray::KEY_F10, "f10"}, {ray::KEY_F11, "f11"}, {ray::KEY_F12, "f12"},
        {ray::KEY_LEFT_SHIFT, "left_shift"},
        {ray::KEY_LEFT_CONTROL, "left_control"},
        {ray::KEY_LEFT_ALT, "left_alt"},
        {ray::KEY_LEFT_SUPER, "left_super"},
        {ray::KEY_RIGHT_SHIFT, "right_shift"},
        {ray::KEY_RIGHT_CONTROL, "right_control"},
        {ray::KEY_RIGHT_ALT, "right_alt"},
        {ray::KEY_RIGHT_SUPER, "right_super"},
        {ray::KEY_KB_MENU, "kb_menu"},
        {ray::KEY_KP_0, "kp_0"}, {ray::KEY_KP_1, "kp_1"}, {ray::KEY_KP_2, "kp_2"},
        {ray::KEY_KP_3, "kp_3"}, {ray::KEY_KP_4, "kp_4"}, {ray::KEY_KP_5, "kp_5"},
        {ray::KEY_KP_6, "kp_6"}, {ray::KEY_KP_7, "kp_7"}, {ray::KEY_KP_8, "kp_8"},
        {ray::KEY_KP_9, "kp_9"},
        {ray::KEY_KP_DECIMAL, "kp_decimal"},
        {ray::KEY_KP_DIVIDE, "kp_divide"},
        {ray::KEY_KP_MULTIPLY, "kp_multiply"},
        {ray::KEY_KP_SUBTRACT, "kp_subtract"},
        {ray::KEY_KP_ADD, "kp_add"},
        {ray::KEY_KP_ENTER, "kp_enter"},
        {ray::KEY_KP_EQUAL, "kp_equal"},
        {ray::KEY_APOSTROPHE, "apostrophe"},
        {ray::KEY_COMMA, "comma"},
        {ray::KEY_MINUS, "minus"},
        {ray::KEY_PERIOD, "period"},
        {ray::KEY_SLASH, "slash"},
        {ray::KEY_SEMICOLON, "semicolon"},
        {ray::KEY_EQUAL, "equal"},
        {ray::KEY_LEFT_BRACKET, "left_bracket"},
        {ray::KEY_BACKSLASH, "backslash"},
        {ray::KEY_RIGHT_BRACKET, "right_bracket"},
        {ray::KEY_GRAVE, "grave"}
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
        {"SPACE", ray::KEY_SPACE},
        {"ESCAPE", ray::KEY_ESCAPE},
        {"ENTER", ray::KEY_ENTER},
        {"TAB", ray::KEY_TAB},
        {"BACKSPACE", ray::KEY_BACKSPACE},
        {"INSERT", ray::KEY_INSERT},
        {"DELETE", ray::KEY_DELETE},
        {"RIGHT", ray::KEY_RIGHT},
        {"LEFT", ray::KEY_LEFT},
        {"DOWN", ray::KEY_DOWN},
        {"UP", ray::KEY_UP},
        {"PAGE_UP", ray::KEY_PAGE_UP},
        {"PAGE_DOWN", ray::KEY_PAGE_DOWN},
        {"HOME", ray::KEY_HOME},
        {"END", ray::KEY_END},
        {"CAPS_LOCK", ray::KEY_CAPS_LOCK},
        {"SCROLL_LOCK", ray::KEY_SCROLL_LOCK},
        {"NUM_LOCK", ray::KEY_NUM_LOCK},
        {"PRINT_SCREEN", ray::KEY_PRINT_SCREEN},
        {"PAUSE", ray::KEY_PAUSE},
        {"F1", ray::KEY_F1}, {"F2", ray::KEY_F2}, {"F3", ray::KEY_F3}, {"F4", ray::KEY_F4},
        {"F5", ray::KEY_F5}, {"F6", ray::KEY_F6}, {"F7", ray::KEY_F7}, {"F8", ray::KEY_F8},
        {"F9", ray::KEY_F9}, {"F10", ray::KEY_F10}, {"F11", ray::KEY_F11}, {"F12", ray::KEY_F12},
        {"LEFT_SHIFT", ray::KEY_LEFT_SHIFT},
        {"LEFT_CONTROL", ray::KEY_LEFT_CONTROL},
        {"LEFT_ALT", ray::KEY_LEFT_ALT},
        {"LEFT_SUPER", ray::KEY_LEFT_SUPER},
        {"RIGHT_SHIFT", ray::KEY_RIGHT_SHIFT},
        {"RIGHT_CONTROL", ray::KEY_RIGHT_CONTROL},
        {"RIGHT_ALT", ray::KEY_RIGHT_ALT},
        {"RIGHT_SUPER", ray::KEY_RIGHT_SUPER},
        {"KB_MENU", ray::KEY_KB_MENU},
        {"KP_0", ray::KEY_KP_0}, {"KP_1", ray::KEY_KP_1}, {"KP_2", ray::KEY_KP_2},
        {"KP_3", ray::KEY_KP_3}, {"KP_4", ray::KEY_KP_4}, {"KP_5", ray::KEY_KP_5},
        {"KP_6", ray::KEY_KP_6}, {"KP_7", ray::KEY_KP_7}, {"KP_8", ray::KEY_KP_8},
        {"KP_9", ray::KEY_KP_9},
        {"KP_DECIMAL", ray::KEY_KP_DECIMAL},
        {"KP_DIVIDE", ray::KEY_KP_DIVIDE},
        {"KP_MULTIPLY", ray::KEY_KP_MULTIPLY},
        {"KP_SUBTRACT", ray::KEY_KP_SUBTRACT},
        {"KP_ADD", ray::KEY_KP_ADD},
        {"KP_ENTER", ray::KEY_KP_ENTER},
        {"KP_EQUAL", ray::KEY_KP_EQUAL},
        {"APOSTROPHE", ray::KEY_APOSTROPHE},
        {"COMMA", ray::KEY_COMMA},
        {"MINUS", ray::KEY_MINUS},
        {"PERIOD", ray::KEY_PERIOD},
        {"SLASH", ray::KEY_SLASH},
        {"SEMICOLON", ray::KEY_SEMICOLON},
        {"EQUAL", ray::KEY_EQUAL},
        {"LEFT_BRACKET", ray::KEY_LEFT_BRACKET},
        {"BACKSLASH", ray::KEY_BACKSLASH},
        {"RIGHT_BRACKET", ray::KEY_RIGHT_BRACKET},
        {"GRAVE", ray::KEY_GRAVE}
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

Config get_config() {
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
