#pragma once

#include "texture.h"
#include "audio.h"
#include "global_data.h"
#include <spdlog/spdlog.h>

inline int virtual_to_screen_x(float virtual_x) {
    int win_w = ray::GetScreenWidth();
    int win_h = ray::GetScreenHeight();
    float scale = std::min((float)win_w / tex.screen_width, (float)win_h / tex.screen_height);
    float effective_zoom = scale * global_data.camera.zoom;
    float zoom_off    = (tex.screen_width * scale * (global_data.camera.zoom    - 1.0f)) * 0.5f;
    float h_scale_off = (tex.screen_width * scale * (global_data.camera.h_scale - 1.0f)) * 0.5f;
    float offset_x = (win_w - tex.screen_width * scale) * 0.5f - zoom_off - h_scale_off
                     + global_data.camera.offset.x * scale;
    return static_cast<int>(virtual_x * effective_zoom + offset_x);
}

enum class Screens {
    TITLE,
    ENTRY,
    SONG_SELECT,
    GAME,
    GAME_2P,
    RESULT,
    RESULT_2P,
    SONG_SELECT_2P,
    DAN_SELECT,
    GAME_DAN,
    DAN_RESULT,
    PRACTICE_SELECT,
    GAME_PRACTICE,
    AI_SELECT,
    AI_GAME,
    SETTINGS,
    LOADING,
    INPUT_CALI,
    SKIN_VIEWER,
    SANDBOX,
    GAME_OVER,
    INPUT_TEST
};

inline std::string screens_to_string(Screens screen) {
    static const std::array<std::string, 24> names = {
        "TITLE",
        "ENTRY",
        "SONG_SELECT",
        "GAME",
        "GAME_2P",
        "RESULT",
        "RESULT_2P",
        "SONG_SELECT_2P",
        "DAN_SELECT",
        "GAME_DAN",
        "DAN_RESULT",
        "PRACTICE_SELECT",
        "GAME_PRACTICE",
        "AI_SELECT",
        "AI_GAME",
        "SETTINGS",
        "LOADING",
        "INPUT_CALI",
        "SKIN_VIEWER",
        "SANDBOX",
        "GAME_OVER",
        "INPUT_TEST"
    };
    return names[static_cast<int>(screen)];
}

template <>
struct fmt::formatter<Screens> : fmt::formatter<std::string> {
    auto format(Screens screen, fmt::format_context& ctx) const {
        return fmt::formatter<std::string>::format(screens_to_string(screen), ctx);
    }
};

class Screen {

public:
    bool screen_init;
    std::string screen_name;
    Screen(const std::string& name)
        : screen_init(false), screen_name(name) {}

    virtual ~Screen() = default;

    virtual void on_screen_start() {
        tex.load_screen_textures(screen_name);
        audio.load_screen_sounds(screen_name);
    }

    virtual Screens on_screen_end(Screens next_screen) {
        screen_init = false;
        spdlog::info("{} ended, transitioning to {} screen", screen_name, next_screen);

        audio.unload_all_sounds();
        audio.unload_all_music();

        tex.unload_textures();

        return next_screen;
    }

    virtual std::optional<Screens> update() {
        auto ret_val = _do_screen_start();
        if (ret_val.has_value()) {
            return ret_val;
        }
        return std::nullopt;
    }

    virtual void draw() {
        // Base implementation does nothing, override in derived classes
    }

    void _do_draw() {
        if (screen_init) {
            draw();
        }
    }

protected:
    std::optional<Screens> _do_screen_start() {
        if (!screen_init) {
            screen_init = true;
            try {
                on_screen_start();
                spdlog::info("{} initialized", screen_name);
            } catch (const std::exception& e) {
                spdlog::critical("{} failed to initialize: {}", screen_name, e.what());
                screen_init = false;
                return Screens::SONG_SELECT;
            } catch (...) {
                spdlog::critical("{} failed to initialize: unknown exception", screen_name);
                screen_init = false;
                return Screens::SONG_SELECT;
            }
        }
        return std::nullopt;
    }
};
