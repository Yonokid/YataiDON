#include "player.h"
#include "../../libs/audio.h"
#include "../../libs/scores.h"

ResultPlayer::ResultPlayer(PlayerNum player_num, bool has_2p, bool is_2p)
    : player_num(player_num), has_2p(has_2p), is_2p(is_2p)
{
    int player_id = get_player_id(player_num);
    auto pd = scores_manager.get_player_data(player_id);

    chara = make_chara_from_player_data(pd ? &*pd : nullptr, is_2p);
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
        {"good",           sd.result_data.good},
        {"ok",             sd.result_data.ok},
        {"bad",            sd.result_data.bad},
        {"total_drumroll", sd.result_data.total_drumroll},
        {"max_combo",      sd.result_data.max_combo},
        {"score",          sd.result_data.score}
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
            if (!script_manager.has_lua_script(script)) return;
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

    sol::optional<bool>   ci  = lua_object["count_up_instant"];
    sol::optional<double> crm = lua_object["count_up_row_ms"];
    sol::optional<double> csm = lua_object["count_up_score_ms"];
    count_up_instant  = ci.value_or(false);
    count_up_row_ms   = crm.value_or(count_up_row_ms);
    count_up_score_ms = csm.value_or(count_up_score_ms);
    sol::optional<std::string> crs = lua_object["count_up_row_sound"];
    sol::optional<std::string> css = lua_object["count_up_score_sound"];
    sol::optional<bool>        c1p = lua_object["count_up_sound_1p_only"];
    count_up_row_sound    = crs.value_or(count_up_row_sound);
    count_up_score_sound  = css.value_or(count_up_score_sound);
    count_up_sound_1p_only = c1p.value_or(count_up_sound_1p_only);

    fn_update     = lua_object["update"];
    fn_draw       = lua_object["draw"];
    fn_draw_gauge = lua_object["draw_gauge"];
    fn_chara_pos  = lua_object["chara_pos"];
    fn_nameplate_pos = lua_object["nameplate_pos"];
}

void ResultPlayer::assign_field(const std::string& field_name, const std::string& value) {
    if      (field_name == "score")          score          = value;
    else if (field_name == "good")           good           = value;
    else if (field_name == "ok")             ok             = value;
    else if (field_name == "bad")            bad            = value;
    else if (field_name == "max_combo")      max_combo      = value;
    else if (field_name == "total_drumroll") total_drumroll = value;
}

void ResultPlayer::update_score_animation(double current_ms, bool is_skipped) {
    if (is_skipped) {
        while (update_index < (int)update_list.size()) {
            auto& [field_name, value] = update_list[update_index];
            assign_field(field_name, std::to_string(value));
            update_index++;
        }
    } else if (count_up_instant) {
        if (score_delay.has_value() && update_index < (int)update_list.size()
            && current_ms > score_delay.value()) {
            auto& [field_name, value] = update_list[update_index];
            assign_field(field_name, std::to_string(value));
            bool is_last = (update_index == (int)update_list.size() - 1);
            if (!count_up_sound_1p_only || player_num == PlayerNum::P1)
                audio.play_sound(is_last ? count_up_score_sound : count_up_row_sound,
                                 VolumePreset::SOUND);
            update_index++;
            score_delay.value() +=
                (update_index == (int)update_list.size() - 1) ? count_up_score_ms : count_up_row_ms;
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
    if (rows_done_ms == 0 && update_index >= (int)update_list.size()) rows_done_ms = current_ms;
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
         score, good, ok, bad, max_combo, total_drumroll, is_skipped);
    nameplate.update(current_ms);
    chara->update(current_ms);
}

double ResultPlayer::reveal_end_ms() {
    if (lua_object.valid()) {
        sol::optional<double> from_lua = lua_object["reveal_end_ms"];
        if (from_lua && from_lua.value() > 0) return from_lua.value();
    }
    return rows_done_ms;
}

void ResultPlayer::draw() {
    call(fn_draw, "ResultPlayer:draw");
    float cx = tex.skin_config[SC::RESULT_CHARA].x;
    float cy = tex.skin_config[SC::RESULT_CHARA].y + ((int)is_2p * tex.screen_height / 2);
    float cs = 1.0f;
    if (fn_chara_pos.valid()) {
        auto pos_opt = call_r<sol::table>(fn_chara_pos, "ResultPlayer:chara_pos");
        if (pos_opt) {
            sol::table& pos = pos_opt.value();
            sol::optional<float> px = pos[1], py = pos[2], ps = pos[3];
            cx = px.value_or(cx);
            cy = py.value_or(cy);
            cs = ps.value_or(cs);
        }
    }
    chara->draw(cx, cy, cs);
    call(fn_draw_gauge, "ResultPlayer:draw_gauge");
    float nx = tex.skin_config[SC::RESULT_NAMEPLATE].x;
    float ny = tex.skin_config[SC::RESULT_NAMEPLATE].y
             + (is_2p * tex.skin_config[SC::RESULT_NAMEPLATE].height);
    float nf = 1.0f;
    if (fn_nameplate_pos.valid()) {
        auto pos_opt = call_r<sol::table>(fn_nameplate_pos, "ResultPlayer:nameplate_pos");
        if (pos_opt) {
            sol::table& pos = pos_opt.value();
            sol::optional<float> px = pos[1], py = pos[2], pf = pos[3];
            nx = px.value_or(nx);
            ny = py.value_or(ny);
            nf = pf.value_or(nf);
        }
    }
    nameplate.draw(nx, ny, nf);
}
