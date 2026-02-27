#include "entry.h"
#include "raylib.h"

void EntryScreen::on_screen_start() {
    Screen::on_screen_start();
    side = 1;
    is_2p = false;
    box_manager = new BoxManager();
    state = EntryState::SELECT_SIDE;

    NameplateConfig plate_info = global_data.config->nameplate_1p;
    nameplate = Nameplate(plate_info.name, plate_info.title, PlayerNum::ALL, -1, false, false, 0);

    timer = new Timer(60, get_current_ms(), [this]() { box_manager->select_box(); });
    screen_init = true;

    side_select_fade = (FadeAnimation*)tex.get_animation(0);
    bg_flicker = (TextureChangeAnimation*)tex.get_animation(1);
    side_select_fade->start();

    //chara = new Chara2D(0);
    announce_played = false;
    players = {nullptr, nullptr};

    std::string lang = global_data.config->general.language;
    auto& skin = tex.skin_config;
    text_cancel = new OutlinedText(skin["entry_cancel"].text[lang], skin["entry_cancel"].font_size, ray::WHITE, ray::BLACK, false, 4, -4);
    text_question = new OutlinedText(skin["entry_question"].text[lang], skin["entry_question"].font_size, ray::WHITE, ray::BLACK, false, 4, -1);

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
                players[1] = new EntryPlayer(global_data.player_num, side, box_manager);
                players[1]->start_animations();
                global_data.player_num = PlayerNum::P1;
                is_2p = true;
            } else {
                players[0] = new EntryPlayer(global_data.player_num, side, box_manager);
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
        for (auto* player : players) {
            if (player) player->handle_input();
        }
        if (players[0] && players[0]->player_num == PlayerNum::P1 && (is_l_don_pressed(PlayerNum::P2) || is_r_don_pressed(PlayerNum::P2))) {
            audio->play_sound("don", "sound");
            state = EntryState::SELECT_SIDE;
            NameplateConfig plate_info = global_data.config->nameplate_2p;
            nameplate = Nameplate(plate_info.name, plate_info.title, PlayerNum::ALL, -1, false, false, 1);
            //chara = new Chara2D(1);
            side_select_fade->restart();
            side = 1;
        } else if (players[0] && players[0]->player_num == PlayerNum::P2 && (is_l_don_pressed(PlayerNum::P1) || is_r_don_pressed(PlayerNum::P1))) {
            audio->play_sound("don", "sound");
            state = EntryState::SELECT_SIDE;
            side_select_fade->restart();
            side = 1;
        }
    }
    return std::nullopt;
}

std::optional<Screens> EntryScreen::update() {
    Screen::update();
    double current_time = get_current_ms();
    side_select_fade->update(current_time);
    bg_flicker->update(current_time);
    box_manager->update(current_time, is_2p);
    timer->update(current_time);
    nameplate.update(current_time);
    //chara->update(current_time, 100, false, false);
    for (auto* player : players) {
        if (player) player->update(current_time);
    }
    if (box_manager->is_finished()) {
        on_screen_end(box_manager->selected_box());
        return std::nullopt;
    }
    for (auto* player : players) {
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
    tex.draw_texture("background", "bg");
    tex.draw_texture("background", "tower");
    tex.draw_texture("background", "shops_center");
    tex.draw_texture("background", "people");
    tex.draw_texture("background", "shops_left");
    tex.draw_texture("background", "shops_right");
    tex.draw_texture("background", "lights", {.scale=2.0f, .fade=bg_flicker->attribute});
}

void EntryScreen::draw_side_select(float fade) {
    auto& skin = tex.skin_config;
    tex.draw_texture("side_select", "box_top_left",    {.fade=fade});
    tex.draw_texture("side_select", "box_top_right",   {.fade=fade});
    tex.draw_texture("side_select", "box_bottom_left", {.fade=fade});
    tex.draw_texture("side_select", "box_bottom_right",{.fade=fade});
    tex.draw_texture("side_select", "box_top",         {.fade=fade});
    tex.draw_texture("side_select", "box_bottom",      {.fade=fade});
    tex.draw_texture("side_select", "box_left",        {.fade=fade});
    tex.draw_texture("side_select", "box_right",       {.fade=fade});
    tex.draw_texture("side_select", "box_center",      {.fade=fade});

    text_question->draw({
        .x=skin["entry_question"].x - text_question->width / 2,
        .y=skin["entry_question"].y,
        .fade=fade,
    });

    //chara->draw(skin["chara_entry"].x, skin["chara_entry"].y);

    tex.draw_texture("side_select", "1P",     {.fade=fade});
    tex.draw_texture("side_select", "cancel", {.fade=fade});
    tex.draw_texture("side_select", "2P",     {.fade=fade});

    if (side == 0) {
        tex.draw_texture("side_select", "1P_highlight",  {.fade=fade});
        tex.draw_texture("side_select", "1P2P_outline",  {.mirror="horizontal", .fade=fade, .index=0});
    } else if (side == 1) {
        tex.draw_texture("side_select", "cancel_highlight", {.fade=fade});
        tex.draw_texture("side_select", "cancel_outline",   {.fade=fade});
    } else {
        tex.draw_texture("side_select", "2P_highlight", {.fade=fade});
        tex.draw_texture("side_select", "1P2P_outline", {.fade=fade, .index=1});
    }

    auto& tex_obj = tex.textures["side_select"]["cancel"];
    float text_x = tex_obj->x[0] + ((float)tex_obj->width / 2) - (text_cancel->width / 2);
    float text_y = tex_obj->y[0] + ((float)tex_obj->height / 2) - (text_cancel->height / 2);
    text_cancel->draw({.x=text_x, .y=text_y, .fade=fade});
    nameplate.draw(tex.skin_config["nameplate_entry"].x, tex.skin_config["nameplate_entry"].y, fade);
}

void EntryScreen::draw_player_drum() {
    for (auto* player : players) {
        if (player) player->draw_drum();
    }
}

void EntryScreen::draw_mode_select() {
    for (auto* player : players) {
        if (player && !player->is_cloud_animation_finished()) return;
    }
    box_manager->draw();
}

void EntryScreen::draw() {
    draw_background();
    draw_player_drum();

    if (state == EntryState::SELECT_SIDE) {
        draw_side_select(side_select_fade->attribute);
    } else if (state == EntryState::SELECT_MODE) {
        draw_mode_select();
    }

    tex.draw_texture("side_select", "footer");

    if (players[0] && players[1]) {
        // both players present, no footer sides needed
    } else if (!players[0]) {
        tex.draw_texture("side_select", "footer_left");
        tex.draw_texture("side_select", "footer_right");
    } else if (players[0] && players[0]->player_num == PlayerNum::P1) {
        tex.draw_texture("side_select", "footer_right");
    } else if (players[0] && players[0]->player_num == PlayerNum::P2) {
        tex.draw_texture("side_select", "footer_left");
    }

    for (auto* player : players) {
        if (player) {
            player->draw_nameplate_and_indicator(player->nameplate_fadein->attribute);
        }
    }

    tex.draw_texture("global", "player_entry");

    if (box_manager->is_finished()) {
        ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::BLACK);
    }

    timer->draw();
    entry_overlay.draw(0, tex.skin_config["entry_overlay_entry"].y);
    coin_overlay.draw();
    allnet_indicator.draw();
}
