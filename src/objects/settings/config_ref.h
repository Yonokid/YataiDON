#pragma once
#include "../../libs/config.h"
#include "../../libs/global_data.h"
#include <variant>
#include <stdexcept>

// Holds a typed pointer into a Config field, enabling get/set by path string.
struct ConfigRef {
    std::variant<bool*, int*, float*, std::string*, std::vector<int>*> ptr;

    ConfigRef(bool* p)              : ptr(p) {}
    ConfigRef(int* p)               : ptr(p) {}
    ConfigRef(float* p)             : ptr(p) {}
    ConfigRef(std::string* p)       : ptr(p) {}
    ConfigRef(std::vector<int>* p)  : ptr(p) {}

    bool             get_bool()  const { return *std::get<bool*>(ptr); }
    int              get_int()   const { return *std::get<int*>(ptr); }
    float            get_float() const { return *std::get<float*>(ptr); }
    std::string      get_str()   const { return *std::get<std::string*>(ptr); }
    std::vector<int> get_vec()   const { return *std::get<std::vector<int>*>(ptr); }

    void set_bool(bool v)                   { *std::get<bool*>(ptr) = v; }
    void set_int(int v)                     { *std::get<int*>(ptr) = v; }
    void set_float(float v)                 { *std::get<float*>(ptr) = v; }
    void set_str(const std::string& v)      { *std::get<std::string*>(ptr) = v; }
    void set_vec(const std::vector<int>& v) { *std::get<std::vector<int>*>(ptr) = v; }

    bool is_bool()  const { return std::holds_alternative<bool*>(ptr); }
    bool is_int()   const { return std::holds_alternative<int*>(ptr); }
    bool is_float() const { return std::holds_alternative<float*>(ptr); }
    bool is_str()   const { return std::holds_alternative<std::string*>(ptr); }
    bool is_vec()   const { return std::holds_alternative<std::vector<int>*>(ptr); }
};

inline ConfigRef get_config_ref(const std::string& path) {
    Config* c = global_data.config;
    // general
    if (path == "general/fps_counter")              return &c->general.fps_counter;
    if (path == "general/audio_offset")             return &c->general.audio_offset;
    if (path == "general/visual_offset")            return &c->general.visual_offset;
    if (path == "general/language")                 return &c->general.language;
    if (path == "general/timer_frozen")             return &c->general.timer_frozen;
    if (path == "general/judge_counter")            return &c->general.judge_counter;
    if (path == "general/nijiiro_notes")            return &c->general.nijiiro_notes;
    if (path == "general/fake_online")              return &c->general.fake_online;
    if (path == "general/practice_mode_bar_delay")  return &c->general.practice_mode_bar_delay;
    if (path == "general/log_level")                return &c->general.log_level;
    if (path == "general/score_method")             return &c->general.score_method;
    if (path == "general/display_bpm")              return &c->general.display_bpm;
    // nameplate_1p
    if (path == "nameplate_1p/name")     return &c->nameplate_1p.name;
    if (path == "nameplate_1p/title")    return &c->nameplate_1p.title;
    if (path == "nameplate_1p/title_bg") return &c->nameplate_1p.title_bg;
    if (path == "nameplate_1p/dan")      return &c->nameplate_1p.dan;
    if (path == "nameplate_1p/gold")     return &c->nameplate_1p.gold;
    if (path == "nameplate_1p/rainbow")  return &c->nameplate_1p.rainbow;
    // nameplate_2p
    if (path == "nameplate_2p/name")     return &c->nameplate_2p.name;
    if (path == "nameplate_2p/title")    return &c->nameplate_2p.title;
    if (path == "nameplate_2p/title_bg") return &c->nameplate_2p.title_bg;
    if (path == "nameplate_2p/dan")      return &c->nameplate_2p.dan;
    if (path == "nameplate_2p/gold")     return &c->nameplate_2p.gold;
    if (path == "nameplate_2p/rainbow")  return &c->nameplate_2p.rainbow;
    // keys (single ints stored in config)
    if (path == "keys/exit_key")         return &c->keys.exit_key;
    if (path == "keys/fullscreen_key")   return &c->keys.fullscreen_key;
    if (path == "keys/borderless_key")   return &c->keys.borderless_key;
    if (path == "keys/pause_key")        return &c->keys.pause_key;
    if (path == "keys/back_key")         return &c->keys.back_key;
    if (path == "keys/restart_key")      return &c->keys.restart_key;
    // keys_1p/2p / gamepad (vector<int>)
    if (path == "keys_1p/left_kat")      return &c->keys_1p.left_kat;
    if (path == "keys_1p/left_don")      return &c->keys_1p.left_don;
    if (path == "keys_1p/right_don")     return &c->keys_1p.right_don;
    if (path == "keys_1p/right_kat")     return &c->keys_1p.right_kat;
    if (path == "keys_2p/left_kat")      return &c->keys_2p.left_kat;
    if (path == "keys_2p/left_don")      return &c->keys_2p.left_don;
    if (path == "keys_2p/right_don")     return &c->keys_2p.right_don;
    if (path == "keys_2p/right_kat")     return &c->keys_2p.right_kat;
    if (path == "gamepad/left_kat")      return &c->gamepad.left_kat;
    if (path == "gamepad/left_don")      return &c->gamepad.left_don;
    if (path == "gamepad/right_don")     return &c->gamepad.right_don;
    if (path == "gamepad/right_kat")     return &c->gamepad.right_kat;
    // audio
    if (path == "audio/device_type")     return &c->audio.device_type;
    if (path == "audio/sample_rate")     return &c->audio.sample_rate;
    if (path == "audio/buffer_size")     return &c->audio.buffer_size;
    // volume
    if (path == "volume/sound")          return &c->volume.sound;
    if (path == "volume/music")          return &c->volume.music;
    if (path == "volume/voice")          return &c->volume.voice;
    if (path == "volume/hitsound")       return &c->volume.hitsound;
    if (path == "volume/attract_mode")   return &c->volume.attract_mode;
    // video
    if (path == "video/fullscreen")      return &c->video.fullscreen;
    if (path == "video/borderless")      return &c->video.borderless;
    if (path == "video/target_fps")      return &c->video.target_fps;
    if (path == "video/vsync")           return &c->video.vsync;

    throw std::runtime_error("Unknown config path: " + path);
}
