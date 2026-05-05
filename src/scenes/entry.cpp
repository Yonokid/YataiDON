#include "entry.h"
#include "../libs/input.h"

void EntryScreen::on_screen_start() {
    Screen::on_screen_start();
    side = 1;
    is_2p = false;
    box_manager = std::make_unique<BoxManager>();
    state = EntryState::SELECT_SIDE;

    NameplateConfig plate_info = global_data.config->nameplate_1p;
    nameplate = Nameplate(plate_info.name, plate_info.title, PlayerNum::ALL, -1, false, false, 0);

    timer = std::make_unique<Timer>(60, get_current_ms(), [this]() { box_manager->select_box(); });

    lua_entry.start_side_select();

    //chara = new Chara2D(0);
    announce_played = false;
    players.clear();
    players.resize(2);

    std::string lang = global_data.config->general.language;
    auto& skin = tex.skin_config;
    text_cancel = std::make_unique<OutlinedText>(skin[SC::ENTRY_CANCEL].text[lang], skin[SC::ENTRY_CANCEL].font_size, ray::WHITE, ray::BLACK, false, 4, -4);
    text_question = std::make_unique<OutlinedText>(skin[SC::ENTRY_QUESTION].text[lang], skin[SC::ENTRY_QUESTION].font_size, ray::WHITE, ray::BLACK, false, 4, -1);

    audio->play_sound("bgm", "music");
}

Screens EntryScreen::on_screen_end(Screens next_screen) {
    audio->stop_sound("bgm");
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
                players[0] = std::make_unique<EntryPlayer>(global_data.player_num, side, box_manager.get());
                players[0]->start_animations();
                is_2p = false;
            }
            audio->play_sound("cloud", "sound");
            audio->play_sound("entry_start_" + std::to_string((int)global_data.player_num) + "p", "voice");
            state = EntryState::SELECT_MODE;
            audio->play_sound("don", "sound");
        }
        if (is_l_kat_pressed()) {
            audio->play_sound("kat", "sound");
            if (players[0] && players[0]->player_num == PlayerNum::P1)
                side = 1;
            else if (players[0] && players[0]->player_num == PlayerNum::P2)
                side = 0;
            else
                side = std::max(0, side - 1);
        }
        if (is_r_kat_pressed()) {
            audio->play_sound("kat", "sound");
            if (players[0] && players[0]->player_num == PlayerNum::P1)
                side = 2;
            else if (players[0] && players[0]->player_num == PlayerNum::P2)
                side = 1;
            else
                side = std::min(2, side + 1);
        }
    } else if (state == EntryState::SELECT_MODE) {
        for (auto& player : players) {
            if (player) player->handle_input();
        }
        if (players[0] && players[0]->player_num == PlayerNum::P1 && (is_l_don_pressed(PlayerNum::P2) || is_r_don_pressed(PlayerNum::P2))) {
            audio->play_sound("don", "sound");
            state = EntryState::SELECT_SIDE;
            NameplateConfig plate_info = global_data.config->nameplate_2p;
            nameplate = Nameplate(plate_info.name, plate_info.title, PlayerNum::ALL, -1, false, false, 1);
            //chara = new Chara2D(1);
            lua_entry.restart_side_select();
            side = 1;
        } else if (players[0] && players[0]->player_num == PlayerNum::P2 && (is_l_don_pressed(PlayerNum::P1) || is_r_don_pressed(PlayerNum::P1))) {
            audio->play_sound("don", "sound");
            state = EntryState::SELECT_SIDE;
            lua_entry.restart_side_select();
            side = 1;
        }
    }
    return std::nullopt;
}

std::optional<Screens> EntryScreen::update() {
    Screen::update();
    double current_time = get_current_ms();
    lua_entry.update(current_time);
    box_manager->update(current_time, is_2p);
    timer->update(current_time);
    nameplate.update(current_time);
    //chara->update(current_time, 100, false, false);
    for (auto& player : players) {
        if (player) player->update(current_time);
    }
    if (box_manager->is_finished()) {
        return on_screen_end(box_manager->selected_box());
    }
    for (auto& player : players) {
        if (player && player->is_cloud_animation_finished() &&
            !audio->is_sound_playing("entry_start_" + std::to_string((int)global_data.player_num) + "p") &&
            !announce_played) {
            audio->play_sound("select_mode", "voice");
            announce_played = true;
        }
    }
    return handle_input();
}

void EntryScreen::draw_background() {
    lua_entry.draw_background();
}

void EntryScreen::draw_side_select(float fade) {
    auto& skin = tex.skin_config;
    lua_entry.draw_side_select(side);

    text_question->draw({
        .x=skin[SC::ENTRY_QUESTION].x - text_question->width / 2,
        .y=skin[SC::ENTRY_QUESTION].y,
        .fade=fade,
    });

    //chara->draw(skin["chara_entry"].x, skin["chara_entry"].y);

    auto& tex_obj = tex.textures[SIDE_SELECT::CANCEL];
    float text_x = tex_obj->x[0] + ((float)tex_obj->width / 2) - (text_cancel->width / 2);
    float text_y = tex_obj->y[0] + ((float)tex_obj->height / 2) - (text_cancel->height / 2);
    text_cancel->draw({.x=text_x, .y=text_y, .fade=fade});
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
        draw_side_select(lua_entry.get_side_select_fade());
    } else if (state == EntryState::SELECT_MODE) {
        draw_mode_select();
    }

    bool p1_joined = (players[0] && players[0]->player_num == PlayerNum::P1) ||
                     (players[1] && players[1]->player_num == PlayerNum::P1);
    bool p2_joined = (players[0] && players[0]->player_num == PlayerNum::P2) ||
                     (players[1] && players[1]->player_num == PlayerNum::P2);
    lua_entry.draw_footer(p1_joined, p2_joined);

    for (auto& player : players) {
        if (player) {
            player->draw_nameplate_and_indicator(player->nameplate_fadein->attribute);
        }
    }

    lua_entry.draw_player_entry();

    if (box_manager->is_finished()) {
        ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::BLACK);
    }

    timer->draw();
    entry_overlay.draw(0, tex.skin_config[SC::ENTRY_OVERLAY_ENTRY].y);
    coin_overlay.draw();
    allnet_indicator.draw();
}
