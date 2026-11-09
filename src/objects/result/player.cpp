#include "player.h"

#include <algorithm>
#include <iterator>

#include "../../libs/audio/audio.h"
#include "../../libs/file/scores.h"
#include "../../libs/network/network.h"

ResultPlayer::ResultPlayer(PlayerNum player_num, bool has_2p, bool is_2p)
    : player_num(player_num), has_2p(has_2p), is_2p(is_2p) {
    PlayerSoundScope audio_owner(player_num);
    std::string player_id = get_player_id(player_num);
    auto pd = scores_manager.get_player_data(player_id);

    chara = make_chara_from_player_data(pd ? &*pd : nullptr); // 2P stands below 1P, facing the same way
    if (pd) {
        chara->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
        chara->apply_face(pd->chara_face_index);
    } else {
        chara->set_don_colors(chara_default_color_1(player_num), chara_default_color_2(player_num),
                              {249, 240, 225, 255});
    }
    chara->set_anim(AnimIndex::DON_NORMAL);

    SessionData& sd = global_data.session_data[static_cast<int>(player_num)];
    if (!sd.result_data.cleared) {
        chara->set_anim(AnimIndex::DON_MISS_NORMAL);
    }
    ranking_name = pd ? pd->username : "";
    ranking_pending = sd.result_data.submitted && !ranking_name.empty();
    nameplate = Nameplate(pd ? pd->username : "", pd ? pd->title : "", player_num, pd ? pd->dan : -1,
                          pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    update_list = {{"good", sd.result_data.good},
                   {"ok", sd.result_data.ok},
                   {"bad", sd.result_data.bad},
                   {"total_drumroll", sd.result_data.total_drumroll},
                   {"max_combo", sd.result_data.max_combo},
                   {"score", sd.result_data.score}};
    score_animator = ScoreAnimator(std::get<1>(update_list[0]));

    CrownType crown_type;
    if (sd.result_data.ok == 0 && sd.result_data.bad == 0) {
        crown_type = CrownType::CROWN_DFC;
    } else if (sd.result_data.bad == 0) {
        crown_type = CrownType::CROWN_FC;
    } else {
        crown_type = CrownType::CROWN_CLEAR;
    }

    int score_diff = std::max(0, sd.result_data.score - sd.result_data.prev_score);

    Modifiers mods = player_data_to_modifiers(pd.value_or(PlayerData{}));
    bool is_shinuchi = global_data.config->general.score_method == ScoreMethod::SHINUCHI;

    if (script_manager.lua) {
        sol::state& lua = *script_manager.lua;
        auto preload = [&](const char* cls, const char* script) {
            if (lua[cls].valid()) {
                return;
            }
            if (!script_manager.has_lua_script(script)) {
                return;
            }
            auto result = lua.script_file(script_manager.get_lua_script_path(script));
            if (!result.valid()) {
                sol::error err = result;
                spdlog::error("Error loading {}.lua: {}", script, err.what());
            }
        };
        preload("Gauge", "gauge");
        preload("BottomCharacters", "bottom_characters");
        preload("ResultCrown", "result_crown");
        preload("ResultCrownMessage", "result_crown_message");
        preload("ResultBasin", "result_basin");
        preload("ResultRanking", "result_ranking");
        preload("HighScoreIndicator", "high_score_indicator");
    }

    if (!load("ResultPlayer", "result_player", static_cast<int>(player_num), is_2p, has_2p, sd.result_data.gauge_length,
              sd.selected_difficulty, sd.selected_difficulty, static_cast<int>(crown_type), score_diff, is_shinuchi,
              mods.display, mods.inverse, mods.random, mods.speed)) {
        return;
    }

    sol::optional<bool> ci = lua_object["count_up_instant"];
    sol::optional<double> crm = lua_object["count_up_row_ms"];
    sol::optional<double> csm = lua_object["count_up_score_ms"];
    count_up_instant = ci.value_or(false);
    count_up_row_ms = crm.value_or(count_up_row_ms);
    count_up_score_ms = csm.value_or(count_up_score_ms);
    sol::optional<std::string> crs = lua_object["count_up_row_sound"];
    sol::optional<std::string> css = lua_object["count_up_score_sound"];
    sol::optional<bool> c1p = lua_object["count_up_sound_1p_only"];
    count_up_row_sound = crs.value_or(count_up_row_sound);
    count_up_score_sound = css.value_or(count_up_score_sound);
    count_up_sound_1p_only = c1p.value_or(count_up_sound_1p_only);

    fn_update = lua_object["update"];
    fn_draw = lua_object["draw"];
    fn_draw_gauge = lua_object["draw_gauge"];
    fn_draw_front = lua_object["draw_front"];
    fn_chara_pos = lua_object["chara_pos"];
    fn_chara_mirror = lua_object["chara_mirror"];
    fn_nameplate_pos = lua_object["nameplate_pos"];
    fn_draw_song_info = lua_object["draw_song_info"];
}

void ResultPlayer::assign_field(const std::string& field_name, const std::string& value) {
    if (field_name == "score") {
        score = value;
    } else if (field_name == "good") {
        good = value;
    } else if (field_name == "ok") {
        ok = value;
    } else if (field_name == "bad") {
        bad = value;
    } else if (field_name == "max_combo") {
        max_combo = value;
    } else if (field_name == "total_drumroll") {
        total_drumroll = value;
    }
}

void ResultPlayer::update_score_animation(double current_ms, bool is_skipped) {
    if (is_skipped) {
        while (update_index < std::ssize(update_list)) {
            auto& [field_name, value] = update_list[update_index];
            assign_field(field_name, std::to_string(value));
            update_index++;
        }
    } else if (count_up_instant) {
        if (score_delay.has_value() && update_index < std::ssize(update_list) && current_ms > score_delay.value()) {
            auto& [field_name, value] = update_list[update_index];
            assign_field(field_name, std::to_string(value));
            bool is_last = (update_index == std::ssize(update_list) - 1);
            if (!count_up_sound_1p_only || player_num == PlayerNum::P1) {
                audio.play_player_sound(is_last ? count_up_score_sound : count_up_row_sound,
                                        count_up_sound_1p_only ? PlayerNum::ALL : player_num, VolumePreset::SOUND);
            }
            update_index++;
            score_delay.value() += (update_index == std::ssize(update_list) - 1) ? count_up_score_ms : count_up_row_ms;
        }
    } else if (score_delay.has_value() && update_index < std::ssize(update_list) && current_ms > score_delay.value() &&
               score_animator.has_value() && !score_animator->is_finished) {
        auto& [field_name, curr_num] = update_list[update_index];
        std::string next_score_str = score_animator->next_score();
        int new_num = std::stoi(next_score_str);
        assign_field(field_name, next_score_str);
        if (new_num != curr_num) {
            audio.play_player_sound("num_up", player_num, VolumePreset::SOUND);
        }
        if (score_animator->is_finished) {
            audio.play_player_sound("don", player_num, VolumePreset::SOUND);
            score_delay.value() += 750;
            if (update_index == std::ssize(update_list) - 1) {
                rows_done_ms = current_ms;
                return;
            }
            update_index++;
            if (update_index < std::ssize(update_list)) {
                score_animator = ScoreAnimator(std::get<1>(update_list[update_index]));
            }
        }
        score_delay.value() += 16.67 * 3;
    }

    if (rows_done_ms == 0 && update_index >= std::ssize(update_list)) {
        rows_done_ms = current_ms;
    }
    if (!score.empty() && !high_score_sound_played) {
        SessionData& sd = global_data.session_data[static_cast<int>(player_num)];
        if (sd.result_data.score > sd.result_data.prev_score) {
            audio.play_player_sound("high_score_voice_" + std::to_string(static_cast<int>(player_num)) + "p",
                                    player_num, VolumePreset::VOICE);
            high_score_sound_played = true;
        }
    }
}

void ResultPlayer::update(double current_ms, bool fade_in_finished, bool is_skipped) {
    PlayerSoundScope audio_owner(player_num);
    if (!score_delay.has_value() && lua_object.valid()) {
        sol::optional<bool> lua_started = lua_object["fade_in_started"];
        if (lua_started && lua_started.value()) {
            score_delay = current_ms;
        }
    }
    update_score_animation(current_ms, is_skipped);
    update_ranking();
    if (!miss_loop_started && lua_object.valid()) {
        sol::optional<bool> miss_loop = lua_object["chara_miss_loop"];
        if (miss_loop && miss_loop.value()) {
            chara->set_anim(AnimIndex::DON_MISS6);
            miss_loop_started = true;
        }
    }
    call(fn_update, "ResultPlayer:update", current_ms, fade_in_finished, score, good, ok, bad, max_combo,
         total_drumroll, is_skipped);
    nameplate.update(current_ms);
    chara->update(current_ms);
}

void ResultPlayer::update_ranking() {
    if (!ranking_pending || !lua_object.valid()) {
        return;
    }
    const SessionData& sd = global_data.session_data[static_cast<int>(player_num)];
    auto board = network.song_leaderboard(sd.song_hash);
    if (!board) {
        return;
    }
    ranking_pending = false;
    const int own = sd.result_data.score;
    auto it = std::ranges::find(*board, ranking_name, &RankingEntry::username);
    if (it == board->end()) {
        board->push_back({ranking_name, own});
    } else {
        it->score = std::max(it->score, own);
    }
    std::ranges::stable_sort(*board, std::greater{}, &RankingEntry::score);

    sol::state& lua = *script_manager.lua;
    sol::table entries = lua.create_table();
    int rank_in = -1;
    for (int i = 0; i < std::min<int>(3, std::ssize(*board)); i++) {
        const RankingEntry& r = (*board)[i];
        entries[i + 1] = lua.create_table_with("name", r.username, "score", r.score);
        if (r.username == ranking_name && r.score == own) {
            rank_in = i;
        }
    }
    lua_object["ranking"] = lua.create_table_with("entries", entries, "rank_in", rank_in);
}

double ResultPlayer::reveal_end_ms() {
    if (lua_object.valid()) {
        sol::optional<double> from_lua = lua_object["reveal_end_ms"];
        if (from_lua && from_lua.value() > 0) {
            return from_lua.value();
        }
    }
    return rows_done_ms;
}

void ResultPlayer::draw_song_info(const std::string& title, const std::string& subtitle, int song_number) {
    if (!fn_draw_song_info.valid()) {
        return;
    }
    sol::table info = script_manager.lua->create_table();
    info["title"] = title;
    info["subtitle"] = subtitle;
    info["number"] = song_number;
    call(fn_draw_song_info, "ResultPlayer:draw_song_info", info);
}

void ResultPlayer::hide_stage() {
    if (lua_object.valid()) {
        lua_object["hide_stage"] = true;
    }
}

void ResultPlayer::draw() {
    PlayerSoundScope audio_owner(player_num);
    call(fn_draw, "ResultPlayer:draw");
    auto [cx, cy] = tex.chara_xy("result");
    cy += static_cast<int>(is_2p) * tex.screen_height / 2;
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
    if (fn_chara_mirror.valid()) {
        auto mirror_opt = call_r<bool>(fn_chara_mirror, "ResultPlayer:chara_mirror");
        if (mirror_opt) {
            chara->set_mirror(mirror_opt.value());
        }
    }
    chara->draw(cx, cy, cs);
    if (fn_draw_front.valid()) {
        call(fn_draw_front, "ResultPlayer:draw_front");
    }
    call(fn_draw_gauge, "ResultPlayer:draw_gauge");
    auto [nx, ny] = tex.nameplate_xy(is_2p ? "result_2p" : "result", "result");
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
