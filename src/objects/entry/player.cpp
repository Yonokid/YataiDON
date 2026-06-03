#include "player.h"
#include "../../libs/input.h"
#include "../../libs/scores.h"

EntryPlayer::EntryPlayer(PlayerNum player_num, int side, BoxManager* box_manager)
    : player_num(player_num), side(side), box_manager(box_manager) {
    NameplateConfig plate_info;
    if (player_num == PlayerNum::P2) {
        plate_info = global_data.config->nameplate_2p;
    } else {
        plate_info = global_data.config->nameplate_1p;
    }
    nameplate = std::make_unique<Nameplate>(
        plate_info.name,
        plate_info.title,
        player_num,
        plate_info.dan,
        plate_info.gold,
        plate_info.rainbow,
        plate_info.title_bg
    );
    indicator = std::make_unique<Indicator>(Indicator::State::SELECT);

    {
        int player_id = get_player_id(player_num);
        auto pd = scores_manager.get_player_data(player_id);
        std::string costume_name = pd ? std::to_string(pd->chara_cos_index) : "0";
        chara = std::make_unique<Chara3D>(costume_name, player_num == PlayerNum::P2);
        if (pd) {
            chara->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
            chara->apply_face(pd->chara_face_index);
        } else {
            chara->set_don_colors(chara_default_color_1(player_id), chara_default_color_2(player_id), {249, 240, 225, 255});
        }
    }

    drum_move_1 = (MoveAnimation*)tex.get_animation(2);
    drum_move_2 = (MoveAnimation*)tex.get_animation(3);
    drum_move_3 = (MoveAnimation*)tex.get_animation(4);
    cloud_resize = (TextureResizeAnimation*)tex.get_animation(5);
    cloud_resize_loop = (TextureResizeAnimation*)tex.get_animation(6);
    cloud_texture_change = (TextureChangeAnimation*)tex.get_animation(7);
    cloud_fade = (FadeAnimation*)tex.get_animation(8);
    nameplate_fadein = (FadeAnimation*)tex.get_animation(12);
}

void EntryPlayer::start_animations() {
    drum_move_1->start();
    drum_move_2->start();
    drum_move_3->start();
    cloud_resize->start();
    cloud_resize_loop->start();
    cloud_texture_change->start();
    cloud_fade->start();
    nameplate_fadein->start();
}

void EntryPlayer::update(double current_time) {
    drum_move_1->update(current_time);
    drum_move_2->update(current_time);
    drum_move_3->update(current_time);
    cloud_resize->update(current_time);
    cloud_texture_change->update(current_time);
    cloud_fade->update(current_time);
    cloud_resize_loop->update(current_time);
    nameplate_fadein->update(current_time);
    nameplate->update(current_time);
    indicator->update(current_time);
    chara->update(current_time);
    if (costume_menu) costume_menu->update(current_time);
}

void EntryPlayer::open_costume_menu() {
    costume_menu.emplace(player_num);
}

void EntryPlayer::draw_drum() {
    float move_x = drum_move_3->attribute;
    float move_y = drum_move_1->attribute + drum_move_2->attribute;

    float offset;
    float chara_x;
    bool chara_mirror;

    if (side == 0) {
        offset = tex.skin_config[SC::ENTRY_DRUM_OFFSET].x;
        tex.draw_texture(SIDE_SELECT::RED_DRUM, {.x=move_x, .y=move_y});
        chara_x = move_x + offset + tex.skin_config[SC::ENTRY_CHARA_OFFSET_L].x;
        chara_mirror = false;
    } else {
        move_x *= -1;
        offset = tex.skin_config[SC::ENTRY_DRUM_OFFSET].y;
        tex.draw_texture(SIDE_SELECT::BLUE_DRUM, {.x=move_x, .y=move_y});
        chara_x = move_x + offset + tex.skin_config[SC::ENTRY_CHARA_OFFSET_R].x;
        chara_mirror = true;
    }

    float chara_y = tex.skin_config[SC::ENTRY_CHARA_OFFSET_R].y + move_y;
    chara->draw(chara_x, chara_y);

    float scale = cloud_resize->attribute;
    if (cloud_resize->is_finished) {
        scale = std::max(1.0f, (float)cloud_resize_loop->attribute);
    }
    tex.draw_texture(SIDE_SELECT::CLOUD, {
        .frame=(int)cloud_texture_change->attribute,
        .scale=scale,
        .center=true,
        .x=move_x + offset,
        .y=move_y,
        .fade=cloud_fade->attribute,
    });
}

void EntryPlayer::draw_costume_menu() {
    if (!costume_menu) return;
    auto sc = (player_num == PlayerNum::P2) ? SC::ENTRY_COSTUME_MENU_2P : SC::ENTRY_COSTUME_MENU_1P;
    auto& info = tex.skin_config[sc];
    costume_menu->draw(info.x, info.y);
}

void EntryPlayer::draw_nameplate_and_indicator(float fade) {
    if (side == 0) {
        nameplate->draw(tex.skin_config[SC::NAMEPLATE_ENTRY_LEFT].x, tex.skin_config[SC::NAMEPLATE_ENTRY_LEFT].y, fade);
        indicator->draw(tex.skin_config[SC::INDICATOR_ENTRY_LEFT].x, tex.skin_config[SC::INDICATOR_ENTRY_LEFT].y, fade);
    } else {
        nameplate->draw(tex.skin_config[SC::NAMEPLATE_ENTRY_RIGHT].x, tex.skin_config[SC::NAMEPLATE_ENTRY_RIGHT].y, fade);
        indicator->draw(tex.skin_config[SC::INDICATOR_ENTRY_RIGHT].x, tex.skin_config[SC::INDICATOR_ENTRY_RIGHT].y, fade);
    }
}

bool EntryPlayer::is_cloud_animation_finished() {
    return cloud_texture_change->is_finished;
}

void EntryPlayer::handle_input() {
    if (costume_menu) {
        costume_menu->handle_input();
        if (costume_menu->get_index().has_value()) {
            int selected_index = costume_menu->get_index().value();
            if (selected_index != chara_index) {
                chara_index = selected_index;
                std::string model_name = costume_menu->get_costume_name();
                chara = std::make_unique<Chara3D>(model_name, player_num == PlayerNum::P2);
                {
                    int player_id = get_player_id(player_num);
                    if (auto pd = scores_manager.get_player_data(player_id)) {
                        chara->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
                        chara->apply_face(pd->chara_face_index);
                    } else {
                        chara->set_don_colors(chara_default_color_1(player_id), chara_default_color_2(player_id), {249, 240, 225, 255});
                    }
                }
            }
        }
        if (costume_menu->confirmed) {
            int player_id = get_player_id(player_num);
            if (auto pd = scores_manager.get_player_data(player_id)) {
                pd->chara_cos_index = std::stoi(costume_menu->get_costume_name());
                scores_manager.save_player_data(*pd);
            }
            costume_menu.reset();
            audio.play_sound("costume_select_" + std::to_string((int)player_num) + "p", VolumePreset::SOUND);
            chara->set_anim(AnimIndex::DON_BALLOON_SUCCESS);
        }
        return;
    }
    if (box_manager->is_box_selected()) return;

    if (is_l_don_pressed(player_num) || is_r_don_pressed(player_num)) {
        audio.play_sound("don", VolumePreset::SOUND);
        if (box_manager->is_costume_box()) {
            box_manager->open_costume_menu(player_num);
        } else {
            box_manager->select_box();
        }
    }
    if (is_l_kat_pressed(player_num)) {
        audio.play_sound("kat", VolumePreset::SOUND);
        box_manager->move_left();
    }
    if (is_r_kat_pressed(player_num)) {
        audio.play_sound("kat", VolumePreset::SOUND);
        box_manager->move_right();
    }
}
