#pragma once

#include "texture.h"
#include "audio_engine.h"

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
            // logger.info(f"Loaded textures for screen: {screen_name}")
        } catch (const std::exception& e) {
            ray::TraceLog(ray::LOG_WARNING, "Failed to load textures for screen %s: %s",
                     screen_name.c_str(), e.what());
        }

        audio->loadScreenSounds(screen_name);
        // logger.info(f"Loaded sounds for screen: {screen_name}")
    }

    virtual std::string on_screen_end(const std::string& next_screen) {
        screen_init = false;
        // logger.info(f"{class_name} ended, transitioning to {next_screen} screen")

        audio->unloadAllSounds();
        audio->unloadAllMusic();
        // logger.info(f"Unloaded sounds for screen: {next_screen}")

        tex.unload_textures();
        // logger.info(f"Unloaded textures for screen: {next_screen}")

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
            // logger.info(f"{class_name} initialized")
        }
        return std::nullopt;
    }
};
