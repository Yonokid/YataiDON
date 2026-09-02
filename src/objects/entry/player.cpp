#include "player.h"
#include "../../libs/input.h"
#include "../../libs/scores.h"
#include "../../libs/network.h"
#include "../../libs/global_data.h"
#include <spdlog/spdlog.h>

static void apply_pd_look(Chara3D& chara, PlayerData* pd, PlayerNum player_num) {
    int player_id = get_player_id(player_num);
    if (pd) {
        chara.set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
        chara.apply_face(pd->chara_face_index);
    } else {
        chara.set_don_colors(chara_default_color_1(player_id), chara_default_color_2(player_id), {249, 240, 225, 255});
    }
}

EntryPlayer::EntryPlayer(PlayerNum player_num, int side, BoxManager* box_manager)
    : player_num(player_num), side(side), box_manager(box_manager) {
    indicator = std::make_unique<Indicator>(Indicator::State::SELECT);

    int player_id = get_player_id(player_num);
    auto pd = scores_manager.get_player_data(player_id);
    nameplate = std::make_unique<Nameplate>(
        pd ? pd->username : "", pd ? pd->title : "",
        player_num,
        pd ? pd->dan : -1, pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    PlayerData* pd_ptr = pd ? &*pd : nullptr;
    chara = make_chara_from_player_data(pd_ptr, player_num == PlayerNum::P2);
    apply_pd_look(*chara, pd_ptr, player_num);

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
    preset_seq_applied = 0;
}

void EntryPlayer::draw_drum() {
    auto pos_opt = call_r<sol::table>(fn_draw_drum_back, "EntryPlayer:draw_drum_back");
    if (pos_opt) {
        sol::table& pos = pos_opt.value();
        sol::optional<float> s = pos[3];
        chara->draw(pos.get<float>(1), pos.get<float>(2), s.value_or(1.0f));
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
        if (int seq = costume_menu->get_preset_seq(); seq != preset_seq_applied) {
            preset_seq_applied = seq;
            bool mirror = player_num == PlayerNum::P2;
            int player_id = get_player_id(player_num);
            auto pd = scores_manager.get_player_data(player_id);
            PlayerData* pd_ptr = pd ? &*pd : nullptr;
            if (auto cos = costume_menu->get_preset_cos_id()) {
                std::string cos_name = std::to_string(*cos);
                chara = std::make_unique<Chara3D>(cos_name, mirror);
            } else {
                chara = make_chara_from_player_data(pd_ptr, mirror);  // preview dropped
            }
            apply_pd_look(*chara, pd_ptr, player_num);
        }
        if (costume_menu->get_index().has_value()) {
            int selected_index = costume_menu->get_index().value();
            CostumePickStage stage = costume_menu->get_pick_stage();
            if (selected_index != chara_index || stage != chara_pick_stage) {
                chara_index = selected_index;
                chara_pick_stage = stage;
                bool mirror = player_num == PlayerNum::P2;
                int player_id = get_player_id(player_num);
                auto pd = scores_manager.get_player_data(player_id);

                if (stage == CostumePickStage::NONE) {
                    std::string model_name = costume_menu->get_costume_name();
                    chara = std::make_unique<Chara3D>(model_name, mirror);
                } else if (stage == CostumePickStage::HEAD) {
                    std::string head_name = costume_menu->get_costume_name();
                    std::string body_name = pd ? std::to_string(pd->chara_body_index) : "0";
                    chara = std::make_unique<Chara3D>(head_name, body_name, mirror);
                } else {
                    std::string head_name = std::to_string(costume_menu->get_picked_head_id());
                    std::string body_name = costume_menu->get_costume_name();
                    chara = std::make_unique<Chara3D>(head_name, body_name, mirror);
                }
                apply_pd_look(*chara, pd ? &*pd : nullptr, player_num);
            }
        }
        if (costume_menu->confirmed) {
            int player_id = get_player_id(player_num);
            if (auto pd = scores_manager.get_player_data(player_id)) {
                if (costume_menu->get_pick_stage() == CostumePickStage::BODY) {
                    pd->chara_head_index = costume_menu->get_picked_head_id();
                    pd->chara_body_index = std::stoi(costume_menu->get_costume_name());
                    pd->chara_is_costume = false;
                } else {
                    pd->chara_cos_index = std::stoi(costume_menu->get_costume_name());
                    pd->chara_is_costume = true;
                }
                scores_manager.save_player_data(*pd);
                spdlog::info("costume_save: player_id={} is_costume={} head={} body={} cos={}",
                    pd->player_id, pd->chara_is_costume, pd->chara_head_index, pd->chara_body_index, pd->chara_cos_index);

                const std::string& access_code = global_data.config->network.access_code;
                if (pd->player_id == scores_manager.player_1 && !access_code.empty()) {
                    network.update_costume(access_code, pd->chara_head_index, pd->chara_body_index,
                                            pd->chara_cos_index, pd->chara_is_costume);
                }
            }
            costume_menu.reset();
            chara_pick_stage = CostumePickStage::NONE;
            audio.play_sound("costume_select_" + std::to_string((int)player_num) + "p", VolumePreset::VOICE);
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
