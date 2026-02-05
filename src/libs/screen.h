#pragma once

#include "texture.h"
#include "audio.h"

class Screen {

public:
    bool screen_init;
    std::string screen_name;
    Screen(const std::string& name)
        : screen_init(false), screen_name(name) {}

    virtual ~Screen() = default;

    virtual void on_screen_start() {
        try {
            tex.load_screen_textures(screen_name);
            spdlog::info("Loaded textures for screen: {}", screen_name);
        } catch (const std::exception& e) {
            ray::TraceLog(ray::LOG_WARNING, "Failed to load textures for screen %s: %s",
                     screen_name.c_str(), e.what());
        }

        audio->load_screen_sounds(screen_name);
        spdlog::info("Loaded sounds for screen: {}", screen_name);
    }

    virtual std::string on_screen_end(const std::string& next_screen) {
        screen_init = false;
        spdlog::info("{} ended, transitioning to {} screen", screen_name, next_screen);

        audio->unload_all_sounds();
        audio->unload_all_music();
        spdlog::info("Unloaded sounds for screen: {}", screen_name);

        tex.unload_textures();
        spdlog::info("Unloaded textures for screen: {}", screen_name);

        return next_screen;
    }

    virtual std::optional<std::string> update() {
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
    std::optional<std::string> _do_screen_start() {
        if (!screen_init) {
            screen_init = true;
            on_screen_start();
            spdlog::info("{} initialized", screen_name);
        }
        return std::nullopt;
    }
};
