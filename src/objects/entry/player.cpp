#include "player.h"

EntryPlayer::EntryPlayer(PlayerNum player_num, int side, BoxManager* box_manager)
    : player_num(player_num), side(side), box_manager(box_manager) {
    NameplateConfig plate_info = global_data.config->nameplate_1p;
    nameplate = new Nameplate(
        plate_info.name,
        plate_info.title,
        player_num,
        plate_info.dan,
        plate_info.gold,
        plate_info.rainbow,
        plate_info.title_bg
    );
    indicator = new Indicator(Indicator::State::SELECT);

    int chara_id = (side == 0) ? 0 : 1;
    //chara = new Chara2D(chara_id);

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
    //chara->update(current_time);
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
    //chara->draw(chara_x, chara_y, chara_mirror);

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
    if (box_manager->is_box_selected()) return;

    if (is_l_don_pressed(player_num) || is_r_don_pressed(player_num)) {
        audio->play_sound("don", "sound");
        box_manager->select_box();
    }
    if (is_l_kat_pressed(player_num)) {
        audio->play_sound("kat", "sound");
        box_manager->move_left();
    }
    if (is_r_kat_pressed(player_num)) {
        audio->play_sound("kat", "sound");
        box_manager->move_right();
    }
}
