#include "player.h"

#include <algorithm>
#include <spdlog/spdlog.h>

#include "../../libs/file/scores.h"
#include "../../libs/global_data.h"
#include "../../libs/input/input.h"
#include "../../libs/network/network.h"

static void apply_pd_look(Chara3D& chara, PlayerData* pd, PlayerNum player_num) {
    if (pd) {
        chara.set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
        chara.apply_face(pd->chara_face_index);
        chara.set_puchi(pd->chara_acce_index);
    } else {
        chara.set_don_colors(chara_default_color_1(player_num), chara_default_color_2(player_num),
                             {249, 240, 225, 255});
    }
    chara.set_anim(AnimIndex::DON_ENTRY_LOOP);
}

EntryPlayer::EntryPlayer(PlayerNum player_num, int side, BoxManager* box_manager)
    : player_num(player_num), side(side), box_manager(box_manager) {
    PlayerSoundScope audio_owner(player_num);
    indicator = std::make_unique<Indicator>(Indicator::State::SELECT);

    std::string player_id = get_player_id(player_num);
    auto pd = scores_manager.get_player_data(player_id);
    nameplate = std::make_unique<Nameplate>(pd ? pd->username : "", pd ? pd->title : "", player_num, pd ? pd->dan : -1,
                                            pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    PlayerData* pd_ptr = pd ? &*pd : nullptr;
    chara = make_chara_from_player_data(pd_ptr, player_num == PlayerNum::P2);
    apply_pd_look(*chara, pd_ptr, player_num);
    chara->set_anim(AnimIndex::DON_ENTRY_IN);

    if (!load("EntryPlayer", "player", side)) {
        return;
    }
    fn_start_animations = lua_object["start_animations"];
    fn_update = lua_object["update"];
    fn_draw_drum_back = lua_object["draw_drum_back"];
    fn_draw_drum_front = lua_object["draw_drum_front"];
    fn_is_cloud_finished = lua_object["is_cloud_finished"];
    fn_get_nameplate_fade = lua_object["get_nameplate_fade"];
    fn_finish_cloud = lua_object["finish_cloud"];
    fn_get_overlay_pos = lua_object["nameplate_indicator_pos"];
    if (auto pos = call_r<sol::table>(fn_get_overlay_pos, "EntryPlayer:nameplate_indicator_pos")) {
        nameplate_x = (*pos)[1].get_or(0.0f);
        nameplate_y = (*pos)[2].get_or(0.0f);
        indicator_x = (*pos)[3].get_or(0.0f);
        indicator_y = (*pos)[4].get_or(0.0f);
    }
}

void EntryPlayer::finish_loading() {
    if (!loading) {
        return;
    }
    loading = false;
    call(fn_finish_cloud, "EntryPlayer:finish_cloud");
}

// Login data arrived after the join animation started: re-equip the real costume/colors/nameplate.
void EntryPlayer::refresh_from_player_data() {
    auto pd = scores_manager.get_player_data(get_player_id(player_num));
    PlayerData* pd_ptr = pd ? &*pd : nullptr;
    nameplate = std::make_unique<Nameplate>(pd ? pd->username : "", pd ? pd->title : "", player_num, pd ? pd->dan : -1,
                                            pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    chara = make_chara_from_player_data(pd_ptr, player_num == PlayerNum::P2);
    apply_pd_look(*chara, pd_ptr, player_num);
}

void save_player_costume(const PlayerData& pd) {
    scores_manager.save_player_data(pd);
    spdlog::info("costume_save: player_id={} is_costume={} head={} body={} cos={}", pd.player_id, pd.chara_is_costume,
                 pd.chara_head_index, pd.chara_body_index, pd.chara_cos_index);
    if (pd.player_id == scores_manager.player_1 && !global_data.config->network.access_code_1.empty()) {
        network.update_costume(global_data.config->network.access_code_1, pd.chara_head_index, pd.chara_body_index,
                               pd.chara_cos_index, pd.chara_is_costume);
    } else if (pd.player_id == scores_manager.player_2 && !global_data.config->network.access_code_2.empty()) {
        network.update_costume(global_data.config->network.access_code_2, pd.chara_head_index, pd.chara_body_index,
                               pd.chara_cos_index, pd.chara_is_costume);
    }
}

void EntryPlayer::start_animations(bool hold_cloud) {
    loading = hold_cloud;
    call(fn_start_animations, "EntryPlayer:start_animations", hold_cloud);
}

void EntryPlayer::update(double current_time) {
    PlayerSoundScope audio_owner(player_num);
    call(fn_update, "EntryPlayer:update", current_time);
    nameplate->update(current_time);
    indicator->update(current_time);
    chara->update(current_time);
    if (costume_menu) {
        costume_menu->update(current_time);
    }
}

void EntryPlayer::open_costume_menu() {
    const auto campaigns = network.active_campaigns();
    const bool bonus = !collabo_bonus_seen && std::ranges::find(campaigns, "mario") != campaigns.end();
    collabo_bonus_seen = collabo_bonus_seen || bonus;
    costume_menu.emplace(player_num, bonus);
    audio.play_player_sound("mode_costume", player_num, VolumePreset::VOICE);
    preset_seq_applied = 0;
}

void EntryPlayer::draw_drum() {
    auto pos_opt = call_r<sol::table>(fn_draw_drum_back, "EntryPlayer:draw_drum_back");
    if (pos_opt) {
        sol::table& pos = pos_opt.value();
        sol::optional<float> x = pos[1];
        sol::optional<float> y = pos[2];
        sol::optional<float> s = pos[3];
        chara->draw(x.value_or(0.0f), y.value_or(0.0f), s.value_or(1.0f));
    }
    call(fn_draw_drum_front, "EntryPlayer:draw_drum_front");
}

void EntryPlayer::draw_costume_menu() {
    PlayerSoundScope audio_owner(player_num);
    if (!costume_menu) {
        return;
    }
    costume_menu->draw();
}

void EntryPlayer::draw_nameplate_and_indicator(float fade) {
    nameplate->draw(nameplate_x, nameplate_y, fade);
    indicator->draw(indicator_x, indicator_y, fade);
}

bool EntryPlayer::is_cloud_animation_finished() {
    return call_r<bool>(fn_is_cloud_finished, "EntryPlayer:is_cloud_finished").value_or(false);
}

float EntryPlayer::get_nameplate_fadein() {
    return call_r<float>(fn_get_nameplate_fade, "EntryPlayer:get_nameplate_fade").value_or(1.0f);
}

void EntryPlayer::handle_input() {
    PlayerSoundScope audio_owner(player_num);
    if (costume_menu) {
        costume_menu->handle_input();
        if (int seq = costume_menu->get_preset_seq(); seq != preset_seq_applied) {
            preset_seq_applied = seq;
            bool mirror = player_num == PlayerNum::P2;
            std::string player_id = get_player_id(player_num);
            auto pd = scores_manager.get_player_data(player_id);
            PlayerData* pd_ptr = pd ? &*pd : nullptr;
            if (auto cos = costume_menu->get_preset_cos_id()) {
                std::string cos_name = std::to_string(*cos);
                chara = std::make_unique<Chara3D>(cos_name, mirror);
            } else {
                chara = make_chara_from_player_data(pd_ptr, mirror); // preview dropped
            }
            apply_pd_look(*chara, pd_ptr, player_num);
        }
        if (auto selection = costume_menu->get_index()) {
            int selected_index = *selection;
            CostumePickStage stage = costume_menu->get_pick_stage();
            if (selected_index != chara_index || stage != chara_pick_stage) {
                chara_index = selected_index;
                chara_pick_stage = stage;
                bool mirror = player_num == PlayerNum::P2;
                std::string player_id = get_player_id(player_num);
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
            std::string player_id = get_player_id(player_num);
            bool parsed = false;
            if (auto pd = scores_manager.get_player_data(player_id)) {
                parsed = true;
                try {
                    if (costume_menu->get_pick_stage() == CostumePickStage::BODY) {
                        pd->chara_head_index = costume_menu->get_picked_head_id();
                        pd->chara_body_index = std::stoi(costume_menu->get_costume_name());
                        pd->chara_is_costume = false;
                    } else {
                        pd->chara_cos_index = std::stoi(costume_menu->get_costume_name());
                        pd->chara_is_costume = true;
                    }
                } catch (const std::exception& e) {
                    spdlog::error("costume_save: invalid costume name '{}': {}", costume_menu->get_costume_name(),
                                  e.what());
                    parsed = false;
                }
                if (parsed) {
                    save_player_costume(*pd);
                }
            }
            costume_menu.reset();
            chara_index = -1;
            chara_pick_stage = CostumePickStage::NONE;
            if (parsed) {
                audio.play_player_sound("costume_select_" + std::to_string(static_cast<int>(player_num)) + "p",
                                        player_num, VolumePreset::VOICE);
                chara->set_anim(AnimIndex::DON_BALLOON_SUCCESS);
            }
        }
        return;
    }
    if (box_manager->is_box_selected()) {
        return;
    }

    if (menu_confirm_pressed(player_num)) {
        audio.play_player_sound("don", player_num, VolumePreset::HITSOUND);
        if (box_manager->is_costume_box()) {
            box_manager->open_costume_menu(player_num);
        } else {
            box_manager->select_box();
        }
    }
    if (menu_left_pressed(player_num)) {
        audio.play_player_sound("kat", player_num, VolumePreset::HITSOUND);
        box_manager->move_left();
    }
    if (menu_right_pressed(player_num)) {
        audio.play_player_sound("kat", player_num, VolumePreset::HITSOUND);
        box_manager->move_right();
    }
}
