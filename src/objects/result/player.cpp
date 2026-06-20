#include "player.h"
#include "../../libs/audio.h"
#include "../../libs/scores.h"

ResultPlayer::ResultPlayer(PlayerNum player_num, bool has_2p, bool is_2p)
    : player_num(player_num), has_2p(has_2p), is_2p(is_2p)
{
    int player_id = get_player_id(player_num);
    auto pd = scores_manager.get_player_data(player_id);

    std::string costume_name = pd ? std::to_string(pd->chara_cos_index) : "0";
    chara = std::make_unique<Chara3D>(costume_name);
    if (pd) {
        chara->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
        chara->apply_face(pd->chara_face_index);
    } else {
        chara->set_don_colors(chara_default_color_1(player_id), chara_default_color_2(player_id), {249, 240, 225, 255});
    }
    chara->set_anim(AnimIndex::DON_NORMAL);

    SessionData& sd = global_data.session_data[(int)player_num];
    score_animator = ScoreAnimator(sd.result_data.score);
    nameplate = Nameplate(
        pd ? pd->username : "", pd ? pd->title : "",
        player_num,
        pd ? pd->dan : -1, pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    update_list = {
        {"score",          sd.result_data.score},
        {"good",           sd.result_data.good},
        {"ok",             sd.result_data.ok},
        {"bad",            sd.result_data.bad},
        {"max_combo",      sd.result_data.max_combo},
        {"total_drumroll", sd.result_data.total_drumroll}
    };

    CrownType crown_type;
    if (sd.result_data.ok == 0 && sd.result_data.bad == 0)
        crown_type = CrownType::CROWN_DFC;
    else if (sd.result_data.bad == 0)
        crown_type = CrownType::CROWN_FC;
    else
        crown_type = CrownType::CROWN_CLEAR;

    int score_diff = std::max(0, sd.result_data.score - sd.result_data.prev_score);

    Modifiers mods = player_data_to_modifiers(pd.value_or(PlayerData{}));
    bool is_shinuchi = global_data.config->general.score_method == ScoreMethod::SHINUCHI;

    if (script_manager.lua) {
        sol::state& lua = *script_manager.lua;
        auto preload = [&](const char* cls, const char* script) {
            if (lua[cls].valid()) return;
            auto result = lua.script_file(script_manager.get_lua_script_path(script));
            if (!result.valid()) {
                sol::error err = result;
                spdlog::error("Error loading {}.lua: {}", script, err.what());
            }
        };
        preload("BottomCharacters",   "bottom_characters");
        preload("ResultCrown",        "result_crown");
        preload("ResultCrownMessage", "result_crown_message");
        preload("HighScoreIndicator", "high_score_indicator");
    }

    if (!load("ResultPlayer", "result_player",
              (int)player_num, is_2p, has_2p,
              sd.result_data.gauge_length, sd.selected_difficulty, sd.selected_difficulty,
              (int)crown_type, score_diff, is_shinuchi,
              mods.display, mods.inverse, mods.random, mods.speed))
        return;

    fn_update     = lua_object["update"];
    fn_draw       = lua_object["draw"];
    fn_draw_gauge = lua_object["draw_gauge"];
}

void ResultPlayer::update_score_animation(double current_ms, bool is_skipped) {
    if (is_skipped) {
        while (update_index < (int)update_list.size()) {
            auto& [field_name, value] = update_list[update_index];
            std::string value_str = std::to_string(value);
            if      (field_name == "score")          score          = value_str;
            else if (field_name == "good")           good           = value_str;
            else if (field_name == "ok")             ok             = value_str;
            else if (field_name == "bad")            bad            = value_str;
            else if (field_name == "max_combo")      max_combo      = value_str;
            else if (field_name == "total_drumroll") total_drumroll = value_str;
            update_index++;
        }
    } else if (score_delay.has_value() && update_index < (int)update_list.size()) {
        if (current_ms > score_delay.value()) {
            if (score_animator.has_value() && !score_animator->is_finished) {
                auto& [field_name, curr_num] = update_list[update_index];
                std::string next_score_str = score_animator->next_score();
                int new_num = std::stoi(next_score_str);
                if      (field_name == "score")          score          = next_score_str;
                else if (field_name == "good")           good           = next_score_str;
                else if (field_name == "ok")             ok             = next_score_str;
                else if (field_name == "bad")            bad            = next_score_str;
                else if (field_name == "max_combo")      max_combo      = next_score_str;
                else if (field_name == "total_drumroll") total_drumroll = next_score_str;
                if (new_num != curr_num) audio.play_sound("num_up", VolumePreset::SOUND);
                if (score_animator->is_finished) {
                    audio.play_sound("don", VolumePreset::SOUND);
                    score_delay.value() += 750;
                    if (update_index == (int)update_list.size() - 1) return;
                    update_index++;
                    if (update_index < (int)update_list.size()) {
                        score_animator = ScoreAnimator(std::get<1>(update_list[update_index]));
                    }
                }
                score_delay.value() += 16.67 * 3;
            }
        }
    }
    if (update_index > 0 && !high_score_sound_played) {
        SessionData& sd = global_data.session_data[(int)player_num];
        if (sd.result_data.score > sd.result_data.prev_score) {
            audio.play_sound("high_score_voice_" + std::to_string((int)player_num) + "p", VolumePreset::VOICE);
            high_score_sound_played = true;
        }
    }
}

void ResultPlayer::update(double current_ms, bool fade_in_finished, bool is_skipped) {
    if (!score_delay.has_value()) {
        sol::optional<bool> lua_started = lua_object["fade_in_started"];
        if (lua_started && lua_started.value()) {
            score_delay = current_ms;
        }
    }
    update_score_animation(current_ms, is_skipped);
    call(fn_update, "ResultPlayer:update",
         current_ms, fade_in_finished,
         score, good, ok, bad, max_combo, total_drumroll);
    nameplate.update(current_ms);
    chara->update(current_ms);
}

void ResultPlayer::draw() {
    call(fn_draw, "ResultPlayer:draw");
    chara->draw(tex.skin_config[SC::RESULT_CHARA].x,
                tex.skin_config[SC::RESULT_CHARA].y + ((int)is_2p * tex.screen_height / 2));
    call(fn_draw_gauge, "ResultPlayer:draw_gauge");
    nameplate.draw(tex.skin_config[SC::RESULT_NAMEPLATE].x,
                   tex.skin_config[SC::RESULT_NAMEPLATE].y + (is_2p * tex.skin_config[SC::RESULT_NAMEPLATE].height));
}
