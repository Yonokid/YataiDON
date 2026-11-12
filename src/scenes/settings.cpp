#include "settings.h"

#include "../libs/audio/audio.h"
#include "../libs/file/filesystem.h"
#include "../libs/file/scores.h"
#include "../libs/gfx/camera_utils.h"
#include "../libs/global_data.h"
#include "../libs/input/input.h"
#include "../libs/network/network.h"
#include "../objects/song_select/file_navigator/navigator.h"

void SettingsScreen::on_screen_start() {
    Screen::on_screen_start();

    if (!global_data.config) {
        spdlog::error("SettingsScreen entered without initialized config");
        screen_init = false;
        throw std::runtime_error("SettingsScreen: global_data.config is null");
    }

    fs::path skin_dir = fs::path("Skins") / global_data.config->paths.skin / "Graphics";
    fs::path tmpl_path = skin_dir / "settings_template.json";
    if (!fs::exists(tmpl_path)) {
        for (const fs::path& root : tex.all_graphics_roots()) {
            if (fs::exists(root / "settings_template.json")) {
                tmpl_path = root / "settings_template.json";
                break;
            }
        }
    }

    try {
        box_manager = std::make_unique<SettingsBoxManager>(read_json_file(tmpl_path));
    } catch (const std::exception& e) {
        spdlog::error("Failed to load settings template: {}", e.what());
        screen_init = false;
        throw;
    }
    indicator = Indicator(Indicator::State::SELECT);
    coin_overlay = CoinOverlay();
    allnet_indicator = AllNetIcon();
    accounts_on_entry = {
        AccountOnEntry{global_data.config->network.access_code_1, scores_manager.player_1_data.username},
        AccountOnEntry{global_data.config->network.access_code_2, scores_manager.player_2_data.username}};
    set_touch_drum_enabled(false);
    take_touch_tap();
    show_cursor(); // the access code boxes reveal on mouse hover
    assets_on_entry.emplace(*global_data.config);

    t_background = tex.get_texture("background/background");
    t_footer = tex.get_texture("background/footer");

    audio.play_sound("bgm", VolumePreset::MUSIC);
    screen_init = true;
}

Screens SettingsScreen::on_screen_end(Screens next_screen) {
    set_touch_drum_enabled(true);
    save_config(*global_data.config);
    scores_manager.save_player_data(scores_manager.player_1_data);
    scores_manager.save_player_data(scores_manager.player_2_data);
    spdlog::info("Settings saved");

    hide_cursor();

    // A switched account's name is not an edit; pushing it could overwrite the server's newer one.
    const std::array<std::pair<const std::string*, const PlayerData*>, 2> accounts{
        std::pair{&global_data.config->network.access_code_1, &scores_manager.player_1_data},
        std::pair{&global_data.config->network.access_code_2, &scores_manager.player_2_data}};
    for (size_t i = 0; i < accounts.size(); i++) {
        const auto& [code, data] = accounts[i];
        if (!code->empty() && *code == accounts_on_entry[i].code && data->username != accounts_on_entry[i].username) {
            network.update_username(*code, data->username);
        }
    }

    box_manager.reset();

    indicator.reset();
    coin_overlay.reset();
    allnet_indicator.reset();
    t_background = nullptr;
    t_footer = nullptr;
    if (assets_on_entry && !assets_on_entry->needs_reload(*global_data.config)) {
        return Screen::on_screen_end(next_screen);
    }
    drop_other_screens_for_skin_reload();
    // navigator is a global, not a Screen -- the rebuild above never
    // touches it, so it needs its own reset.
    navigator.reset_for_skin_reload();

    unload_skin();
    load_skin();

    reload_skin_screens();

    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> SettingsScreen::handle_input() {
    if (!box_manager->editing() && check_key_pressed(global_data.config->keys.back_key)) {
        return on_screen_end(Screens::ENTRY);
    }
    if (key_pressed(SDL_SCANCODE_F1)) {
        return on_screen_end(Screens::INPUT_CALI);
    }
    if (key_pressed(SDL_SCANCODE_F3)) {
        return on_screen_end(Screens::INPUT_TEST);
    }
    const bool typing = box_manager->editing_text();
    if (const auto tap = take_touch_tap(); tap && !typing) {
        const Vec2 pixel{tap->x * get_screen_width(), tap->y * get_screen_height()};
        const Vec2 world = get_screen_to_world_2d(pixel, compute_camera2d(tex.screen_width, tex.screen_height));
        audio.play_sound("don", VolumePreset::HITSOUND);
        if (box_manager->tap(world)) {
            return on_screen_end(Screens::ENTRY);
        }
        return std::nullopt;
    }
    if (is_l_kat_pressed() || (!typing && menu_left_pressed())) {
        audio.play_sound("kat", VolumePreset::HITSOUND);
        box_manager->move_left();
    } else if (is_r_kat_pressed() || (!typing && menu_right_pressed())) {
        audio.play_sound("kat", VolumePreset::HITSOUND);
        box_manager->move_right();
    } else if (is_l_don_pressed() || is_r_don_pressed() || (!typing && menu_confirm_pressed())) {
        audio.play_sound("don", VolumePreset::HITSOUND);
        bool result = box_manager->select_box();
        if (result) {
            return on_screen_end(Screens::ENTRY);
        }
    }
    return std::nullopt;
}

std::optional<Screens> SettingsScreen::update() {
    if (auto result = Screen::update()) {
        return result;
    }
    if (!screen_init) {
        return std::nullopt;
    }
    double current_time = get_current_ms();
    allnet_indicator->update(current_time);
    indicator->update(current_time);
    const bool was_editing_text = box_manager->editing_text();
    box_manager->update(current_time);
    if (auto screen = box_manager->pending_screen_change()) {
        return on_screen_end(*screen);
    }
    // The field consumes Return/Escape during update(). Keep that entire frame
    // in text-edit mode so the same key cannot reopen it or leave settings.
    if (was_editing_text || box_manager->editing_text()) {
        clear_input_buffers();
        return std::nullopt;
    }
    return handle_input();
}

void SettingsScreen::draw() {
    tex.draw_texture(t_background);
    box_manager->draw();
    tex.draw_texture(t_footer);
    indicator->draw_on("song_select");
    coin_overlay->draw();
    allnet_indicator->draw();
}
