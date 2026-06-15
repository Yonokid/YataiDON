#include "player.h"
#include "../../libs/input.h"
#include "../../libs/scores.h"

EntryPlayer::EntryPlayer(PlayerNum player_num, int side, BoxManager* box_manager)
    : player_num(player_num), side(side), box_manager(box_manager) {
    indicator = std::make_unique<Indicator>(Indicator::State::SELECT);

    int player_id = get_player_id(player_num);
    auto pd = scores_manager.get_player_data(player_id);
    nameplate = std::make_unique<Nameplate>(
        pd ? pd->username : "", pd ? pd->title : "",
        player_num,
        pd ? pd->dan : -1, pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    std::string costume_name = pd ? std::to_string(pd->chara_cos_index) : "0";
    chara = std::make_unique<Chara3D>(costume_name, player_num == PlayerNum::P2);
    if (pd) {
        chara->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
        chara->apply_face(pd->chara_face_index);
    } else {
        chara->set_don_colors(chara_default_color_1(player_id), chara_default_color_2(player_id), {249, 240, 225, 255});
    }

    if (!load("EntryPlayer", "player", side)) return;
    fn_start_animations   = lua_object["start_animations"];
    fn_update             = lua_object["update"];
    fn_draw_drum_back     = lua_object["draw_drum_back"];
    fn_draw_drum_front    = lua_object["draw_drum_front"];
    fn_is_cloud_finished  = lua_object["is_cloud_finished"];
    fn_get_nameplate_fade = lua_object["get_nameplate_fade"];
}

void EntryPlayer::start_animations() { call(fn_start_animations, "EntryPlayer:start_animations"); }

void EntryPlayer::update(double current_time) {
    call(fn_update, "EntryPlayer:update", current_time);
    nameplate->update(current_time);
    indicator->update(current_time);
    chara->update(current_time);
    if (costume_menu) costume_menu->update(current_time);
}

void EntryPlayer::open_costume_menu() {
    costume_menu.emplace(player_num);
}

void EntryPlayer::draw_drum() {
    auto pos_opt = call_r<sol::table>(fn_draw_drum_back, "EntryPlayer:draw_drum_back");
    if (pos_opt) {
        sol::table& pos = pos_opt.value();
        chara->draw(pos.get<float>(1), pos.get<float>(2));
    }
    call(fn_draw_drum_front, "EntryPlayer:draw_drum_front");
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
    return call_r<bool>(fn_is_cloud_finished, "EntryPlayer:is_cloud_finished").value_or(false);
}

float EntryPlayer::get_nameplate_fadein() {
    return call_r<float>(fn_get_nameplate_fade, "EntryPlayer:get_nameplate_fade").value_or(1.0f);
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
