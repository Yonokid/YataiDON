#include "settings.h"
#include "../libs/audio.h"
#include "../libs/input.h"
#include "../libs/filesystem.h"
#include "../libs/scores.h"

void save_config(const Config& config);

void SettingsScreen::on_screen_start() {
    Screen::on_screen_start();

    fs::path skin_dir = fs::path("Skins") / global_data.config->paths.skin / "Graphics";
    fs::path tmpl_path = skin_dir / "settings_template.json";

    try {
        box_manager = std::make_unique<SettingsBoxManager>(read_json_file(tmpl_path));
    } catch (const std::exception& e) {
        spdlog::error("Failed to load settings template: {}", e.what());
        screen_init = false;
        return;
    }
    indicator   = Indicator(Indicator::State::SELECT);
    coin_overlay   = CoinOverlay();
    allnet_indicator = AllNetIcon();

    audio.play_sound("bgm", VolumePreset::MUSIC);
    screen_init = true;
}

Screens SettingsScreen::on_screen_end(Screens next_screen) {
    save_config(*global_data.config);
    scores_manager.save_player_data(scores_manager.player_1_data);
    scores_manager.save_player_data(scores_manager.player_2_data);
    spdlog::info("Settings saved");

    audio.close_audio_device();
    fs::path sounds_path = fs::path("Skins") / global_data.config->paths.skin / "Sounds";
    audio.init_audio_device(sounds_path, global_data.config->audio, global_data.config->volume);

    box_manager.reset();

    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> SettingsScreen::handle_input() {
    if (ray::IsKeyPressed(ray::KEY_F1)) {
        return on_screen_end(Screens::INPUT_CALI);
    }
    if (ray::IsKeyPressed(ray::KEY_F2)) {
        return on_screen_end(Screens::SKIN_VIEWER);
    }
    if (is_l_kat_pressed()) {
        audio.play_sound("kat", VolumePreset::SOUND);
        box_manager->move_left();
    } else if (is_r_kat_pressed()) {
        audio.play_sound("kat", VolumePreset::SOUND);
        box_manager->move_right();
    } else if (is_l_don_pressed() || is_r_don_pressed()) {
        audio.play_sound("don", VolumePreset::SOUND);
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
    allnet_indicator.update(current_time);
    indicator.update(current_time);
    box_manager->update(current_time);
    if (auto screen = box_manager->pending_screen_change())
        return on_screen_end(*screen);
    return handle_input();
}

void SettingsScreen::draw() {
    tex.draw_texture(BACKGROUND::BACKGROUND);
    box_manager->draw();
    tex.draw_texture(BACKGROUND::FOOTER);
    indicator.draw(tex.skin_config[SC::SONG_SELECT_INDICATOR].x,
                   tex.skin_config[SC::SONG_SELECT_INDICATOR].y);
    coin_overlay.draw();
    allnet_indicator.draw();
}
