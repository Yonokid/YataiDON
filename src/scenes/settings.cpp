#include "settings.h"
#include <fstream>
#include <sstream>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

// Exposed from config.cpp (non-static).
void save_config(const Config& config);

void SettingsScreen::on_screen_start() {
    Screen::on_screen_start();

    fs::path skin_dir = fs::path("Skins") / global_data.config->paths.skin / "Graphics";
    fs::path tmpl_path = skin_dir / "settings_template.json";

    rapidjson::Document doc;
    {
        std::ifstream ifs(tmpl_path);
        if (!ifs.is_open()) {
            spdlog::error("settings_template.json not found at {}", tmpl_path.string());
            doc.Parse(R"({"exit":{"name":{"en":"Exit"},"options":{}}})");
        } else {
            rapidjson::IStreamWrapper isw(ifs);
            doc.ParseStream(isw);
        }
    }
    if (doc.HasParseError()) {
        spdlog::error("Failed to parse settings_template.json");
        doc.Parse(R"({"exit":{"name":{"en":"Exit"},"options":{}}})");
    }

    box_manager = new SettingsBoxManager(doc);
    indicator   = Indicator(Indicator::State::SELECT);
    coin_overlay   = CoinOverlay();
    allnet_indicator = AllNetIcon();

    audio->play_sound("bgm", "music");
    screen_init = true;
}

Screens SettingsScreen::on_screen_end(Screens next_screen) {
    save_config(*global_data.config);
    spdlog::info("Settings saved");

#ifndef AUDIO_BACKEND_RAYLIB
    audio->close_audio_device();
    audio = std::make_unique<AudioEngine>(
        global_data.config->audio.device_type,
        global_data.config->audio.sample_rate,
        global_data.config->audio.buffer_size,
        global_data.config->volume
    );
    audio->init_audio_device();
#endif

    delete box_manager;
    box_manager = nullptr;

    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> SettingsScreen::handle_input() {
    if (ray::IsKeyPressed(ray::KEY_F1)) {
        return on_screen_end(Screens::INPUT_CALI);
    }
    if (is_l_kat_pressed()) {
        audio->play_sound("kat", "sound");
        box_manager->move_left();
    } else if (is_r_kat_pressed()) {
        audio->play_sound("kat", "sound");
        box_manager->move_right();
    } else if (is_l_don_pressed() || is_r_don_pressed()) {
        audio->play_sound("don", "sound");
        bool result = box_manager->select_box();
        if (result) {
            return on_screen_end(Screens::ENTRY);
        }
    }
    return std::nullopt;
}

std::optional<Screens> SettingsScreen::update() {
    Screen::update();
    double current_time = get_current_ms();
    indicator.update(current_time);
    box_manager->update(current_time);
    return handle_input();
}

void SettingsScreen::draw() {
    tex.draw_texture("background", "background");
    box_manager->draw();
    tex.draw_texture("background", "footer");
    indicator.draw(tex.skin_config["song_select_indicator"].x,
                   tex.skin_config["song_select_indicator"].y);
    coin_overlay.draw();
    allnet_indicator.draw();
}
