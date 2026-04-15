#include "audio_engine.h"

std::unique_ptr<IAudioEngine> audio;

// Shared implementation: walks the skin sound directory and calls the virtual
// load_sound(), which dispatches to whichever backend is active.
void IAudioEngine::load_screen_sounds(const std::string& screen_name) {
    fs::path path = sounds_path / screen_name;
    if (!fs::exists(path)) {
        spdlog::warn("Sounds for screen {} not found", screen_name);
        return;
    }

    load_sound(sounds_path / "don.wav", "don");
    load_sound(sounds_path / "ka.wav",  "kat");

    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.is_directory()) {
            for (const auto& file : fs::directory_iterator(entry.path())) {
                load_sound(file.path(),
                           entry.path().stem().string() + "_" + file.path().stem().string());
            }
        } else if (entry.is_regular_file()) {
            load_sound(entry.path(), entry.path().stem().string());
        }
    }

    fs::path global_path = sounds_path / "global";
    if (fs::exists(global_path)) {
        for (const auto& entry : fs::directory_iterator(global_path)) {
            if (entry.is_directory()) {
                for (const auto& file : fs::directory_iterator(entry.path())) {
                    load_sound(file.path(),
                               entry.path().stem().string() + "_" + file.path().stem().string());
                }
            } else if (entry.is_regular_file()) {
                load_sound(entry.path(), entry.path().stem().string());
            }
        }
    }
}
