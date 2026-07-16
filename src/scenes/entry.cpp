#include "entry.h"
#include "../libs/input.h"
#include "../libs/scores.h"

void EntryScreen::on_screen_start() {
    Screen::on_screen_start();
    side = 1;
    is_2p = false;
    box_manager = std::make_unique<BoxManager>();
    state = EntryState::SELECT_SIDE;

    {
        auto pd = scores_manager.get_player_data(global_data.config->general.player_1_id);
        nameplate = Nameplate(
            pd ? pd->username : "", pd ? pd->title : "",
            PlayerNum::ALL,
            pd ? pd->dan : -1, pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    }

    timer = std::make_unique<Timer>(60, get_current_ms(), [this]() {
        if (box_manager->is_costume_box()) {
            box_manager->open_costume_menu(global_data.player_num);
        } else {
            box_manager->select_box();
        }
    });

    lua_entry = std::make_unique<EntryScript>();
    lua_entry->start_side_select();

    reload_preview_chara(global_data.config->general.player_1_id);
    announce_played = false;
    players.clear();
    players.resize(2);

    audio.play_sound("bgm", VolumePreset::MUSIC);
}

void EntryScreen::reload_preview_chara(int player_id) {
    auto pd = scores_manager.get_player_data(player_id);
    std::string costume_name = pd ? std::to_string(pd->chara_cos_index) : "0";
    chara = std::make_unique<Chara3D>(costume_name);
    if (pd) {
        chara->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
        chara->apply_face(pd->chara_face_index);
    } else {
        chara->set_don_colors(chara_default_color_1(player_id), chara_default_color_2(player_id), {249, 240, 225, 255});
    }
}

Screens EntryScreen::on_screen_end(Screens next_screen) {
    audio.stop_sound("bgm");
    return Screen::on_screen_end(next_screen);
}

std::optional<Screens> EntryScreen::handle_input() {
    if (state == EntryState::SELECT_SIDE) {
        if (is_l_don_pressed() || is_r_don_pressed()) {
            if (side == 1) {
                return on_screen_end(Screens::TITLE);
            }
            global_data.player_num = (side == 0) ? PlayerNum::P1 : PlayerNum::P2;

            if (players[0]) {
                players[1] = std::make_unique<EntryPlayer>(global_data.player_num, side, box_manager.get());
                players[1]->start_animations();
                global_data.player_num = PlayerNum::P1;
                is_2p = true;
            } else {
                global_data.first_login_player = global_data.player_num;
                players[0] = std::make_unique<EntryPlayer>(global_data.player_num, side, box_manager.get());
                players[0]->start_animations();
                is_2p = false;
            }
            audio.play_sound("cloud", VolumePreset::SOUND);
            audio.play_sound("entry_start_" + std::to_string((int)global_data.player_num) + "p", VolumePreset::VOICE);
            state = EntryState::SELECT_MODE;
            audio.play_sound("don", VolumePreset::SOUND);
        }
        if (is_l_kat_pressed()) {
            audio.play_sound("kat", VolumePreset::SOUND);
            if (players[0] && players[0]->player_num == PlayerNum::P1)
                side = 1;
            else if (players[0] && players[0]->player_num == PlayerNum::P2)
                side = 0;
            else
                side = std::max(0, side - 1);
        }
        if (is_r_kat_pressed()) {
            audio.play_sound("kat", VolumePreset::SOUND);
            if (players[0] && players[0]->player_num == PlayerNum::P1)
                side = 2;
            else if (players[0] && players[0]->player_num == PlayerNum::P2)
                side = 1;
            else
                side = std::min(2, side + 1);
        }
    } else if (state == EntryState::SELECT_COSTUME) {
        for (auto& player : players) {
            if (player) player->handle_input();
        }
    } else if (state == EntryState::SELECT_MODE) {
        for (auto& player : players) {
            if (player) player->handle_input();
        }
        if (players[0] && players[0]->player_num == PlayerNum::P1 && (is_l_don_pressed(PlayerNum::P2) || is_r_don_pressed(PlayerNum::P2))) {
            audio.play_sound("don", VolumePreset::SOUND);
            state = EntryState::SELECT_SIDE;
            {
                auto pd = scores_manager.get_player_data(global_data.config->general.player_2_id);
                nameplate = Nameplate(
                    pd ? pd->username : "", pd ? pd->title : "",
                    PlayerNum::ALL,
                    pd ? pd->dan : -1, pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
            }
            lua_entry->restart_side_select();
            side = 1;
            reload_preview_chara(global_data.config->general.player_2_id);
        } else if (players[0] && players[0]->player_num == PlayerNum::P2 && (is_l_don_pressed(PlayerNum::P1) || is_r_don_pressed(PlayerNum::P1))) {
            audio.play_sound("don", VolumePreset::SOUND);
            state = EntryState::SELECT_SIDE;
            lua_entry->restart_side_select();
            side = 1;
            reload_preview_chara(global_data.config->general.player_2_id);
        }
    }
    return std::nullopt;
}

std::optional<Screens> EntryScreen::update() {
    Screen::update();
    double current_time = get_current_ms();
    allnet_indicator.update(current_time);
    entry_overlay.update(current_time);
    lua_entry->update(current_time);
    box_manager->update(current_time, is_2p);
    timer->update(current_time);
    nameplate.update(current_time);
    chara->update(current_time);
    for (auto& player : players) {
        if (player) player->update(current_time);
    }
    if (box_manager->costume_menu_open) {
        box_manager->costume_menu_open = false;
        state = EntryState::SELECT_COSTUME;
        for (auto& player : players) {
            if (player && player->player_num == box_manager->opening_player)
                player->open_costume_menu();
        }
    }
    if (state == EntryState::SELECT_COSTUME) {
        bool any_open = false;
        for (auto& player : players) {
            if (player && player->costume_menu.has_value()) { any_open = true; break; }
        }
        if (!any_open) state = EntryState::SELECT_MODE;
    }
    if (box_manager->is_finished()) {
        return on_screen_end(box_manager->selected_box());
    }
    for (auto& player : players) {
        if (player && player->is_cloud_animation_finished() &&
            !audio.is_sound_playing("entry_start_" + std::to_string((int)global_data.player_num) + "p") &&
            !announce_played) {
            audio.play_sound("select_mode", VolumePreset::VOICE);
            announce_played = true;
        }
    }
    return handle_input();
}

void EntryScreen::draw_background() {
    lua_entry->draw_background();
}

void EntryScreen::draw_side_select(float fade) {
    auto& skin = tex.skin_config;
    lua_entry->draw_side_select();

    chara->draw(tex.skin_config[SC::CHARA_ENTRY].x, tex.skin_config[SC::CHARA_ENTRY].y);
    lua_entry->draw_side_select_buttons(side);
    nameplate.draw(skin[SC::NAMEPLATE_ENTRY].x, skin[SC::NAMEPLATE_ENTRY].y, fade);
}

void EntryScreen::draw_player_drum() {
    for (auto& player : players) {
        if (player) player->draw_drum();
    }
}

void EntryScreen::draw_mode_select() {
    for (auto& player : players) {
        if (player && !player->is_cloud_animation_finished()) return;
    }
    box_manager->draw();
}

void EntryScreen::draw() {
    draw_background();
    draw_player_drum();

    if (state == EntryState::SELECT_SIDE) {
        draw_side_select(lua_entry->get_side_select_fade());
    } else if (state == EntryState::SELECT_MODE) {
        draw_mode_select();
    } else if (state == EntryState::SELECT_COSTUME) {
        for (auto& player : players) {
            if (player) player->draw_costume_menu();
        }
    }

    bool p1_joined = (players[0] && players[0]->player_num == PlayerNum::P1) ||
                     (players[1] && players[1]->player_num == PlayerNum::P1);
    bool p2_joined = (players[0] && players[0]->player_num == PlayerNum::P2) ||
                     (players[1] && players[1]->player_num == PlayerNum::P2);
    lua_entry->draw_footer(p1_joined, p2_joined);

    for (auto& player : players) {
        if (player) {
            player->draw_nameplate_and_indicator(player->get_nameplate_fadein());
        }
    }

    lua_entry->draw_player_entry();

    if (box_manager->is_finished()) {
        ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::BLACK);
    }

    timer->draw();
    entry_overlay.draw(0, tex.skin_config[SC::ENTRY_OVERLAY_ENTRY].y);
    coin_overlay.draw();
    allnet_indicator.draw();
}
