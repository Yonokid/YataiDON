#include "config.h"

#include <algorithm>
#include <spdlog/spdlog.h>
#include <string_view>

#include "../gfx.h"
#include "../gfx/sdl_compat.h"
#include "../global_data.h"

// Key codes are SDL_Scancode values. Letters and digits are handled separately in both directions.
static const std::vector<std::pair<std::string_view, int>> key_names = {{"space", SDL_SCANCODE_SPACE},
                                                                        {"escape", SDL_SCANCODE_ESCAPE},
                                                                        {"enter", SDL_SCANCODE_RETURN},
                                                                        {"tab", SDL_SCANCODE_TAB},
                                                                        {"backspace", SDL_SCANCODE_BACKSPACE},
                                                                        {"insert", SDL_SCANCODE_INSERT},
                                                                        {"delete", SDL_SCANCODE_DELETE},
                                                                        {"right", SDL_SCANCODE_RIGHT},
                                                                        {"left", SDL_SCANCODE_LEFT},
                                                                        {"down", SDL_SCANCODE_DOWN},
                                                                        {"up", SDL_SCANCODE_UP},
                                                                        {"page_up", SDL_SCANCODE_PAGEUP},
                                                                        {"page_down", SDL_SCANCODE_PAGEDOWN},
                                                                        {"home", SDL_SCANCODE_HOME},
                                                                        {"end", SDL_SCANCODE_END},
                                                                        {"caps_lock", SDL_SCANCODE_CAPSLOCK},
                                                                        {"scroll_lock", SDL_SCANCODE_SCROLLLOCK},
                                                                        {"num_lock", SDL_SCANCODE_NUMLOCKCLEAR},
                                                                        {"print_screen", SDL_SCANCODE_PRINTSCREEN},
                                                                        {"pause", SDL_SCANCODE_PAUSE},
                                                                        {"f1", SDL_SCANCODE_F1},
                                                                        {"f2", SDL_SCANCODE_F2},
                                                                        {"f3", SDL_SCANCODE_F3},
                                                                        {"f4", SDL_SCANCODE_F4},
                                                                        {"f5", SDL_SCANCODE_F5},
                                                                        {"f6", SDL_SCANCODE_F6},
                                                                        {"f7", SDL_SCANCODE_F7},
                                                                        {"f8", SDL_SCANCODE_F8},
                                                                        {"f9", SDL_SCANCODE_F9},
                                                                        {"f10", SDL_SCANCODE_F10},
                                                                        {"f11", SDL_SCANCODE_F11},
                                                                        {"f12", SDL_SCANCODE_F12},
                                                                        {"left_shift", SDL_SCANCODE_LSHIFT},
                                                                        {"left_control", SDL_SCANCODE_LCTRL},
                                                                        {"left_alt", SDL_SCANCODE_LALT},
                                                                        {"left_super", SDL_SCANCODE_LGUI},
                                                                        {"right_shift", SDL_SCANCODE_RSHIFT},
                                                                        {"right_control", SDL_SCANCODE_RCTRL},
                                                                        {"right_alt", SDL_SCANCODE_RALT},
                                                                        {"right_super", SDL_SCANCODE_RGUI},
                                                                        {"kb_menu", SDL_SCANCODE_APPLICATION},
                                                                        {"kp_0", SDL_SCANCODE_KP_0},
                                                                        {"kp_1", SDL_SCANCODE_KP_1},
                                                                        {"kp_2", SDL_SCANCODE_KP_2},
                                                                        {"kp_3", SDL_SCANCODE_KP_3},
                                                                        {"kp_4", SDL_SCANCODE_KP_4},
                                                                        {"kp_5", SDL_SCANCODE_KP_5},
                                                                        {"kp_6", SDL_SCANCODE_KP_6},
                                                                        {"kp_7", SDL_SCANCODE_KP_7},
                                                                        {"kp_8", SDL_SCANCODE_KP_8},
                                                                        {"kp_9", SDL_SCANCODE_KP_9},
                                                                        {"kp_decimal", SDL_SCANCODE_KP_PERIOD},
                                                                        {"kp_divide", SDL_SCANCODE_KP_DIVIDE},
                                                                        {"kp_multiply", SDL_SCANCODE_KP_MULTIPLY},
                                                                        {"kp_subtract", SDL_SCANCODE_KP_MINUS},
                                                                        {"kp_add", SDL_SCANCODE_KP_PLUS},
                                                                        {"kp_enter", SDL_SCANCODE_KP_ENTER},
                                                                        {"kp_equal", SDL_SCANCODE_KP_EQUALS},
                                                                        {"apostrophe", SDL_SCANCODE_APOSTROPHE},
                                                                        {"comma", SDL_SCANCODE_COMMA},
                                                                        {"minus", SDL_SCANCODE_MINUS},
                                                                        {"period", SDL_SCANCODE_PERIOD},
                                                                        {"slash", SDL_SCANCODE_SLASH},
                                                                        {"semicolon", SDL_SCANCODE_SEMICOLON},
                                                                        {"equal", SDL_SCANCODE_EQUALS},
                                                                        {"left_bracket", SDL_SCANCODE_LEFTBRACKET},
                                                                        {"backslash", SDL_SCANCODE_BACKSLASH},
                                                                        {"right_bracket", SDL_SCANCODE_RIGHTBRACKET},
                                                                        {"grave", SDL_SCANCODE_GRAVE}};

std::string getKeyString(int key_code) {
    if (key_code >= SDL_SCANCODE_A && key_code <= SDL_SCANCODE_Z) {
        return std::string(1, static_cast<char>('A' + key_code - SDL_SCANCODE_A));
    }
    if (key_code >= SDL_SCANCODE_1 && key_code <= SDL_SCANCODE_9) {
        return std::string(1, static_cast<char>('1' + key_code - SDL_SCANCODE_1));
    }
    if (key_code == SDL_SCANCODE_0) {
        return "0";
    }

    for (const auto& [name, code] : key_names) {
        if (code == key_code) {
            return std::string(name);
        }
    }

    throw std::runtime_error("Unknown key code: " + std::to_string(key_code));
}

static int getKeyCode(const std::string& key) {
    // Unambiguous escaped form for round-tripping key codes that have no
    // known string representation (see getKeyStringSafe).
    if (key.starts_with("code:")) {
        std::string num = key.substr(5);
        try {
            std::size_t pos = 0;
            int code = std::stoi(num, &pos);
            if (pos != num.size()) {
                throw std::invalid_argument("trailing characters");
            }
            return code;
        } catch (...) {
            throw std::runtime_error("Invalid key: " + key);
        }
    }

    // Handle single alphanumeric characters
    if (key.length() == 1 && std::isalnum(static_cast<unsigned char>(key[0]))) {
        const int c = std::toupper(static_cast<unsigned char>(key[0]));
        if (c >= 'A' && c <= 'Z') {
            return SDL_SCANCODE_A + (c - 'A');
        }
        return c == '0' ? SDL_SCANCODE_0 : SDL_SCANCODE_1 + (c - '1');
    }

    std::string lower_key = key;
    std::ranges::transform(lower_key, lower_key.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (const auto& [name, code] : key_names) {
        if (name == lower_key) {
            return code;
        }
    }

    throw std::runtime_error("Invalid key: " + key);
}

static int getKeyCodeOrDefault(const std::string& v, const char* def) {
    try {
        return getKeyCode(v);
    } catch (const std::runtime_error& e) {
        spdlog::warn("{} -- using {}", e.what(), def);
        try {
            return getKeyCode(def);
        } catch (const std::runtime_error& e2) {
            spdlog::error("Invalid default key '{}': {} -- using escape", def, e2.what());
            return SDL_SCANCODE_ESCAPE;
        }
    }
}

static std::vector<int> parseKeyArray(const toml::array& arr) {
    std::vector<int> result;
    for (const auto& elem : arr) {
        if (elem.is_string()) {
            try {
                result.push_back(getKeyCode(elem.as_string()->get()));
            } catch (const std::runtime_error& e) {
                spdlog::warn("Skipping invalid key binding: {}", e.what());
            }
        } else {
            spdlog::warn("Skipping non-string key binding entry");
        }
    }
    return result;
}

std::vector<int> parseIntArray(const toml::array& arr) {
    std::vector<int> result;
    for (const auto& elem : arr) {
        if (auto val = elem.as_integer()) {
            result.push_back(static_cast<int>(val->get()));
        } else {
            spdlog::warn("Skipping non-integer gamepad binding element");
        }
    }
    return result;
}

static std::vector<fs::path> parsePathArray(const toml::array& arr) {
    std::vector<fs::path> result;
    for (const auto& elem : arr) {
        if (elem.is_string()) {
            result.emplace_back(elem.as_string()->get());
        }
    }
    return result;
}

static fs::path config_file_path() {
    return fs::path("config.toml");
}

// Touch input is enabled by default on the mobile platforms (where the touch
// drums are the primary control) and disabled on desktop.
#if defined(PLATFORM_ANDROID) || defined(YATAIDON_PLATFORM_IOS)
static constexpr bool kDefaultTouchInput = true;
#else
static constexpr bool kDefaultTouchInput = false;
#endif

// iOS drives the display from UIKit's animation callback, so vsync must stay on.
#ifdef YATAIDON_PLATFORM_IOS
static constexpr bool kDefaultVsync = true;
#else
static constexpr bool kDefaultVsync = false;
#endif

static std::vector<int> parseKeyArrayOrDefault(const toml::table& t, const char* section, const char* key,
                                               std::vector<int> def) {
    if (auto arr = t[section][key].as_array()) {
        return parseKeyArray(*arr);
    }
    return def;
}

static std::vector<int> parseIntArrayOrDefault(const toml::table* section, const char* key, std::vector<int> def) {
    if (section) {
        if (auto arr = (*section)[key].as_array()) {
            return parseIntArray(*arr);
        }
    }
    return def;
}

static std::vector<fs::path> parsePathArrayOrDefault(const toml::table& t, const char* section, const char* key,
                                                     std::vector<fs::path> def) {
    if (auto arr = t[section][key].as_array()) {
        return parsePathArray(*arr);
    }
    return def;
}

Config get_config() {
    fs::path config_path = config_file_path();

    toml::table config_file;
    try {
        config_file = toml::parse_file(config_path.string());
    } catch (const toml::parse_error& err) {
        std::error_code ec;
        if (!fs::exists(config_path, ec)) {
            spdlog::info("{} not found -- using defaults", config_path.string());
        } else {
            spdlog::error("Failed to parse {}: {} -- using defaults", config_path.string(), err.what());
            // Back up the unparsable file before defaults get written over it by
            // the next save_config(), so the user's real settings aren't lost.
            fs::path backup_path = config_path;
            backup_path += ".bak";
            fs::copy_file(config_path, backup_path, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                spdlog::error("Failed to back up {}: {}", config_path.string(), ec.message());
                fs::path aside_path = config_path;
                aside_path += ".unparsable";
                fs::rename(config_path, aside_path, ec);
                if (ec) {
                    spdlog::error("Failed to preserve unparsable {}: {}", config_path.string(), ec.message());
                }
            } else {
                spdlog::warn("Backed up unparsable config to {}", backup_path.string());
            }
        }
    }

    Config config{};

    config.general.fps_counter = config_file["general"]["fps_counter"].value_or(false);
    config.general.audio_offset = config_file["general"]["audio_offset"].value_or(0);
    config.general.visual_offset = config_file["general"]["visual_offset"].value_or(0);
    config.general.language = config_file["general"]["language"].value_or("en");
    config.general.song_timer = config_file["general"]["song_timer"].value_or(false);
    config.general.judge_counter = config_file["general"]["judge_counter"].value_or(false);
    config.general.show_timing_offset = config_file["general"]["show_timing_offset"].value_or(false);
    config.general.log_level = config_file["general"]["log_level"].value_or("info");
    config.general.practice_mode_bar_delay = config_file["general"]["practice_mode_bar_delay"].value_or(1);
    config.general.score_method = config_file["general"]["score_method"].value_or("shinuchi");
    config.general.display_bpm = config_file["general"]["display_bpm"].value_or(false);
    config.general.webcam_number = config_file["general"]["webcam_number"].value_or(-1);
    config.general.touch_input = config_file["general"]["touch_input"].value_or(kDefaultTouchInput);
    config.general.touch_drum_size = config_file["general"]["touch_drum_size"].value_or(1.0f);
    config.general.skin_updater = config_file["general"]["skin_updater"].value_or(true);
    config.general.fast_transitions = config_file["general"]["fast_transitions"].value_or(false);

    // limits (timer_frozen / song_limit used to live under [general])
    config.limits.timer_frozen =
        config_file["limits"]["timer_frozen"].value_or(config_file["general"]["timer_frozen"].value_or(true));
    config.limits.song_limit =
        config_file["limits"]["song_limit"].value_or(config_file["general"]["song_limit"].value_or(0));
    config.limits.practice_time_limit = config_file["limits"]["practice_time_limit"].value_or(120);

    // Migrate old config: access_code -> access_code_1, player_1_id -> access_code_1, player_2_id -> access_code_2
    std::string old_access_code =
        config_file["network"]["access_code"].value_or(config_file["general"]["access_code"].value_or(""));
    int old_player_1_id = config_file["general"]["player_1_id"].value_or(-1);
    int old_player_2_id = config_file["general"]["player_2_id"].value_or(-1);

    // access_code_1: prefer new field, then old access_code, then convert old player_1_id, else default "0"
    if (auto ac1 = config_file["network"]["access_code_1"].value<std::string>()) {
        config.network.access_code_1 = *ac1;
    } else if (!old_access_code.empty()) {
        config.network.access_code_1 = old_access_code;
    } else if (old_player_1_id > 0) {
        // Map old integer IDs to new string access codes (matching DB migration: 1->"0", 2->"1")
        if (old_player_1_id == 1) {
            config.network.access_code_1 = "0";
        } else if (old_player_1_id == 2) {
            config.network.access_code_1 = "1";
        } else {
            config.network.access_code_1 = std::to_string(old_player_1_id);
        }
    } else {
        // Completely new config: use "0" as default (matches default player "0" in DB)
        config.network.access_code_1 = "0";
    }

    // access_code_2: prefer new field, then convert old player_2_id, else default "1"
    if (auto ac2 = config_file["network"]["access_code_2"].value<std::string>()) {
        config.network.access_code_2 = *ac2;
    } else if (old_player_2_id > 0) {
        // Map old player_2_id to new string access code (old default was 2, maps to "1")
        if (old_player_2_id == 2) {
            config.network.access_code_2 = "1";
        } else {
            config.network.access_code_2 = std::to_string(old_player_2_id);
        }
    } else {
        // Completely new config: use "1" as default (matches default player "1" in DB)
        config.network.access_code_2 = "1";
    }

    // Access codes were 24 digits; the server now uses the last 20.
    for (std::string* code : {&config.network.access_code_1, &config.network.access_code_2}) {
        if (code->size() == 24 && std::ranges::all_of(*code, ::isdigit)) {
            code->erase(0, 4);
        }
    }

    config.network.online_play =
        config_file["network"]["online_play"].value_or(config_file["general"]["online_play"].value_or(false));
    config.network.sync_scores =
        config_file["network"]["sync_scores"].value_or(config_file["general"]["sync_scores_on_launch"].value_or(false));
    config.network.auto_login = config_file["network"]["auto_login"].value_or(true);

    config.card_reader.enabled = config_file["card_reader"]["enabled"].value_or(false);

    // Parse paths
    config.paths.tja_path = parsePathArrayOrDefault(config_file, "paths", "tja_path", {fs::path("Songs")});
    config.paths.skin = fs::path(config_file["paths"]["skin"].value_or(""));

    // Parse keys (converting from strings to key codes)
    config.keys.exit_key = getKeyCodeOrDefault(config_file["keys"]["exit_key"].value_or("Q"), "Q");
    config.keys.fullscreen_key = getKeyCodeOrDefault(config_file["keys"]["fullscreen_key"].value_or("f11"), "f11");
    config.keys.borderless_key = getKeyCodeOrDefault(config_file["keys"]["borderless_key"].value_or("f10"), "f10");
    config.keys.pause_key = getKeyCodeOrDefault(config_file["keys"]["pause_key"].value_or("space"), "space");
    config.keys.back_key = getKeyCodeOrDefault(config_file["keys"]["back_key"].value_or("escape"), "escape");
    config.keys.restart_key = getKeyCodeOrDefault(config_file["keys"]["restart_key"].value_or("f1"), "f1");
    config.keys.gamepad_menu_layout = config_file["keys"]["gamepad_menu_layout"].value_or(false);

    // Parse keys_1p
    config.keys_1p.left_kat = parseKeyArrayOrDefault(config_file, "keys_1p", "left_kat", {SDL_SCANCODE_D});
    config.keys_1p.left_don = parseKeyArrayOrDefault(config_file, "keys_1p", "left_don", {SDL_SCANCODE_F});
    config.keys_1p.right_don = parseKeyArrayOrDefault(config_file, "keys_1p", "right_don", {SDL_SCANCODE_J});
    config.keys_1p.right_kat = parseKeyArrayOrDefault(config_file, "keys_1p", "right_kat", {SDL_SCANCODE_K});

    // Parse keys_2p
    config.keys_2p.left_kat = parseKeyArrayOrDefault(config_file, "keys_2p", "left_kat", {SDL_SCANCODE_Z});
    config.keys_2p.left_don = parseKeyArrayOrDefault(config_file, "keys_2p", "left_don", {SDL_SCANCODE_X});
    config.keys_2p.right_don = parseKeyArrayOrDefault(config_file, "keys_2p", "right_don", {SDL_SCANCODE_C});
    config.keys_2p.right_kat = parseKeyArrayOrDefault(config_file, "keys_2p", "right_kat", {SDL_SCANCODE_V});

    // Parse gamepad_1p (fallback to legacy [gamepad] if missing)
    const toml::table* gamepad_1p_node = config_file["gamepad_1p"].as_table();
    if (!gamepad_1p_node) {
        gamepad_1p_node = config_file["gamepad"].as_table();
    }
    config.gamepad_1p.left_kat = parseIntArrayOrDefault(gamepad_1p_node, "left_kat", {10});
    config.gamepad_1p.left_don = parseIntArrayOrDefault(gamepad_1p_node, "left_don", {16});
    config.gamepad_1p.right_don = parseIntArrayOrDefault(gamepad_1p_node, "right_don", {17});
    config.gamepad_1p.right_kat = parseIntArrayOrDefault(gamepad_1p_node, "right_kat", {12});

    // Parse gamepad_2p
    const toml::table* gamepad_2p_node = config_file["gamepad_2p"].as_table();
    config.gamepad_2p.left_kat = parseIntArrayOrDefault(gamepad_2p_node, "left_kat", {});
    config.gamepad_2p.left_don = parseIntArrayOrDefault(gamepad_2p_node, "left_don", {});
    config.gamepad_2p.right_don = parseIntArrayOrDefault(gamepad_2p_node, "right_don", {});
    config.gamepad_2p.right_kat = parseIntArrayOrDefault(gamepad_2p_node, "right_kat", {});

    const toml::table* midi_node = config_file["midi"].as_table();
    config.midi.device = midi_node ? (*midi_node)["device"].value_or("") : "";
    config.midi.channel = midi_node ? (*midi_node)["channel"].value_or(0) : 0;
    config.midi.notes = parseIntArrayOrDefault(midi_node, "notes", {});
    config.midi.buttons = parseIntArrayOrDefault(midi_node, "buttons", {});
    if (config.midi.channel < 0 || config.midi.channel > 16) {
        spdlog::warn("MIDI channel must be 0 (any) or 1-16; using any channel");
        config.midi.channel = 0;
    }

    // Parse audio
    config.audio.device_type = config_file["audio"]["device_type"].value_or(0);
    config.audio.device = config_file["audio"]["device"].value_or("");
    config.audio.sample_rate = config_file["audio"]["sample_rate"].value_or(44100);
    config.audio.buffer_size = config_file["audio"]["buffer_size"].value_or(128);
    if (auto asio_channel = config_file["audio"]["asio_channel"].as_array()) {
        config.audio.asio_channel = parseIntArray(*asio_channel);
    }
    if (config.audio.asio_channel.empty()) {
        config.audio.asio_channel.push_back(0);
    }

    // Parse volume
    config.volume.sound = config_file["volume"]["sound"].value_or(1.0);
    config.volume.music = config_file["volume"]["music"].value_or(0.8);
    config.volume.voice = config_file["volume"]["voice"].value_or(1.0);
    config.volume.hitsound = config_file["volume"]["hitsound"].value_or(1.0);
    config.volume.attract_mode = config_file["volume"]["attract_mode"].value_or(1.0);

    // Parse video
    config.video.fullscreen = config_file["video"]["fullscreen"].value_or(false);
    config.video.borderless = config_file["video"]["borderless"].value_or(false);
    config.video.target_fps = config_file["video"]["target_fps"].value_or(-1);
    config.video.vsync = config_file["video"]["vsync"].value_or(kDefaultVsync);
    platform_load_config(config, config_file);

    return config;
}

static std::string getKeyStringSafe(int key_code) {
    try {
        return getKeyString(key_code);
    } catch (const std::runtime_error& e) {
        spdlog::warn("{} -- saving escaped key code {}", e.what(), key_code);
        return "code:" + std::to_string(key_code);
    }
}

void save_config(const Config& config) {
    fs::path config_path = config_file_path();

    toml::table config_table;

    // General
    config_table.insert("general", toml::table{{"fps_counter", config.general.fps_counter},
                                               {"audio_offset", config.general.audio_offset},
                                               {"visual_offset", config.general.visual_offset},
                                               {"language", config.general.language},
                                               {"song_timer", config.general.song_timer},
                                               {"judge_counter", config.general.judge_counter},
                                               {"show_timing_offset", config.general.show_timing_offset},
                                               {"log_level", config.general.log_level},
                                               {"practice_mode_bar_delay", config.general.practice_mode_bar_delay},
                                               {"score_method", config.general.score_method},
                                               {"display_bpm", config.general.display_bpm},
                                               {"webcam_number", config.general.webcam_number},
                                               {"touch_input", config.general.touch_input},
                                               {"touch_drum_size", config.general.touch_drum_size},
                                               {"skin_updater", config.general.skin_updater},
                                               {"fast_transitions", config.general.fast_transitions}});

    config_table.insert("limits", toml::table{{"timer_frozen", config.limits.timer_frozen},
                                              {"song_limit", config.limits.song_limit},
                                              {"practice_time_limit", config.limits.practice_time_limit}});

    // Network
    config_table.insert("network",
                        toml::table{{"access_code_1", global_data.card_override[0] ? global_data.card_prev_code[0]
                                                                                   : config.network.access_code_1},
                                    {"access_code_2", global_data.card_override[1] ? global_data.card_prev_code[1]
                                                                                   : config.network.access_code_2},
                                    {"online_play", config.network.online_play},
                                    {"sync_scores", config.network.sync_scores},
                                    {"auto_login", config.network.auto_login}});

    config_table.insert("card_reader", toml::table{{"enabled", config.card_reader.enabled}});

    // Paths
    toml::array tja_path_array;
    for (const auto& path : config.paths.tja_path) {
        tja_path_array.push_back(path.string());
    }
    config_table.insert("paths", toml::table{{"tja_path", tja_path_array}, {"skin", config.paths.skin.string()}});

    // Keys
    config_table.insert("keys", toml::table{{"exit_key", getKeyStringSafe(config.keys.exit_key)},
                                            {"fullscreen_key", getKeyStringSafe(config.keys.fullscreen_key)},
                                            {"borderless_key", getKeyStringSafe(config.keys.borderless_key)},
                                            {"pause_key", getKeyStringSafe(config.keys.pause_key)},
                                            {"back_key", getKeyStringSafe(config.keys.back_key)},
                                            {"restart_key", getKeyStringSafe(config.keys.restart_key)},
                                            {"gamepad_menu_layout", config.keys.gamepad_menu_layout}});

    // Keys 1P
    toml::array left_kat_1p, left_don_1p, right_don_1p, right_kat_1p;
    for (int key : config.keys_1p.left_kat) {
        left_kat_1p.push_back(getKeyStringSafe(key));
    }
    for (int key : config.keys_1p.left_don) {
        left_don_1p.push_back(getKeyStringSafe(key));
    }
    for (int key : config.keys_1p.right_don) {
        right_don_1p.push_back(getKeyStringSafe(key));
    }
    for (int key : config.keys_1p.right_kat) {
        right_kat_1p.push_back(getKeyStringSafe(key));
    }

    config_table.insert("keys_1p", toml::table{{"left_kat", left_kat_1p},
                                               {"left_don", left_don_1p},
                                               {"right_don", right_don_1p},
                                               {"right_kat", right_kat_1p}});

    // Keys 2P
    toml::array left_kat_2p, left_don_2p, right_don_2p, right_kat_2p;
    for (int key : config.keys_2p.left_kat) {
        left_kat_2p.push_back(getKeyStringSafe(key));
    }
    for (int key : config.keys_2p.left_don) {
        left_don_2p.push_back(getKeyStringSafe(key));
    }
    for (int key : config.keys_2p.right_don) {
        right_don_2p.push_back(getKeyStringSafe(key));
    }
    for (int key : config.keys_2p.right_kat) {
        right_kat_2p.push_back(getKeyStringSafe(key));
    }

    config_table.insert("keys_2p", toml::table{{"left_kat", left_kat_2p},
                                               {"left_don", left_don_2p},
                                               {"right_don", right_don_2p},
                                               {"right_kat", right_kat_2p}});

    toml::array gp1_left_kat, gp1_left_don, gp1_right_don, gp1_right_kat;
    for (int btn : config.gamepad_1p.left_kat) {
        gp1_left_kat.push_back(btn);
    }
    for (int btn : config.gamepad_1p.left_don) {
        gp1_left_don.push_back(btn);
    }
    for (int btn : config.gamepad_1p.right_don) {
        gp1_right_don.push_back(btn);
    }
    for (int btn : config.gamepad_1p.right_kat) {
        gp1_right_kat.push_back(btn);
    }

    config_table.insert("gamepad_1p", toml::table{{"left_kat", gp1_left_kat},
                                                  {"left_don", gp1_left_don},
                                                  {"right_don", gp1_right_don},
                                                  {"right_kat", gp1_right_kat}});

    toml::array gp2_left_kat, gp2_left_don, gp2_right_don, gp2_right_kat;
    for (int btn : config.gamepad_2p.left_kat) {
        gp2_left_kat.push_back(btn);
    }
    for (int btn : config.gamepad_2p.left_don) {
        gp2_left_don.push_back(btn);
    }
    for (int btn : config.gamepad_2p.right_don) {
        gp2_right_don.push_back(btn);
    }
    for (int btn : config.gamepad_2p.right_kat) {
        gp2_right_kat.push_back(btn);
    }

    config_table.insert("gamepad_2p", toml::table{{"left_kat", gp2_left_kat},
                                                  {"left_don", gp2_left_don},
                                                  {"right_don", gp2_right_don},
                                                  {"right_kat", gp2_right_kat}});

    toml::array midi_notes, midi_buttons;
    for (int note : config.midi.notes) {
        midi_notes.push_back(note);
    }
    for (int button : config.midi.buttons) {
        midi_buttons.push_back(button);
    }
    config_table.insert("midi", toml::table{{"device", config.midi.device},
                                            {"channel", config.midi.channel},
                                            {"notes", midi_notes},
                                            {"buttons", midi_buttons}});

    // Audio
    toml::array asio_channel;
    for (int ch : config.audio.asio_channel) {
        asio_channel.push_back(ch);
    }

    config_table.insert("audio", toml::table{{"device_type", config.audio.device_type},
                                             {"device", config.audio.device},
                                             {"sample_rate", config.audio.sample_rate},
                                             {"buffer_size", config.audio.buffer_size},
                                             {"asio_channel", asio_channel}});

    // Volume
    config_table.insert("volume", toml::table{{"sound", config.volume.sound},
                                              {"music", config.volume.music},
                                              {"voice", config.volume.voice},
                                              {"hitsound", config.volume.hitsound},
                                              {"attract_mode", config.volume.attract_mode}});

    // Video
    config_table.insert("video", toml::table{{"fullscreen", config.video.fullscreen},
                                             {"borderless", config.video.borderless},
                                             {"target_fps", config.video.target_fps},
                                             {"vsync", config.video.vsync}});

    platform_save_config(config, config_table);
    fs::path tmp_path = config_path;
    tmp_path += ".tmp";
    {
        std::error_code rm_ec;
        std::ofstream ofs(tmp_path, std::ios::trunc);
        if (!ofs.is_open()) {
            spdlog::error("Failed to open {} for writing", tmp_path.string());
            return;
        }
        ofs << config_table;
        ofs.close();
        if (!ofs) {
            spdlog::error("Failed to write {}", tmp_path.string());
            fs::remove(tmp_path, rm_ec);
            return;
        }
    }

    std::error_code ec;
    // std::filesystem::rename replacing an existing destination is only
    // guaranteed on POSIX; on Windows it commonly fails when config_path
    // already exists, so remove it first (best-effort).
    std::error_code pre_rm_ec;
    fs::remove(config_path, pre_rm_ec);
    fs::rename(tmp_path, config_path, ec);
    if (ec) {
        spdlog::error("Failed to save config.toml: {}", ec.message());
        std::error_code rm_ec;
        fs::remove(tmp_path, rm_ec);
    }
}

void ensure_config_file(const Config& config) {
    std::error_code ec;
    if (fs::exists("config.toml", ec)) {
        return;
    }
    spdlog::info("No config file found -- writing defaults to {}", config_file_path().string());
    save_config(config);
}
