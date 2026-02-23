#include "player.h"

ResultPlayer::ResultPlayer(PlayerNum player_num, bool has_2p, bool is_2p)
: player_num(player_num), has_2p(has_2p), is_2p(is_2p){
    fade_in_bottom = (FadeAnimation*)tex.get_animation(1);
    // self.chara = Chara2D(int(self.player_num) - 1)
    SessionData& session_data = global_data.session_data[(int)player_num];
    score_animator = ScoreAnimator(session_data.result_data.score);
    NameplateConfig plate_info;
    if (is_2p) {
        plate_info = global_data.config->nameplate_2p;
    } else {
        plate_info = global_data.config->nameplate_1p;
    }
    nameplate = Nameplate(plate_info.name, plate_info.title, player_num, plate_info.dan, plate_info.gold, plate_info.rainbow, plate_info.title_bg);
    update_list = {
        {"score", session_data.result_data.score},
        {"good", session_data.result_data.good},
        {"ok", session_data.result_data.ok},
        {"bad", session_data.result_data.bad},
        {"max_combo", session_data.result_data.max_combo},
        {"total_drumroll", session_data.result_data.total_drumroll}
    };
    if (session_data.result_data.ok == 0 && session_data.result_data.bad == 0) {
        crown_type = CrownType::CROWN_DFC;
    } else if (session_data.result_data.bad == 0) {
        crown_type = CrownType::CROWN_FC;
    } else {
        crown_type = CrownType::CROWN_CLEAR;
    }
}
void ResultPlayer::update_score_animation(double current_ms, bool is_skipped) {
    if (is_skipped) {
        if (update_index == update_list.size()) return;
        while (update_index < update_list.size()) {
            std::string field_name = std::get<0>(update_list[update_index]);
            int value = std::get<1>(update_list[update_index]);
            std::string value_str = std::to_string(value);

            if (field_name == "score") score = value_str;
            else if (field_name == "good") good = value_str;
            else if (field_name == "ok") ok = value_str;
            else if (field_name == "bad") bad = value_str;
            else if (field_name == "max_combo") max_combo = value_str;
            else if (field_name == "total_drumroll") total_drumroll = value_str;

            update_index += 1;
        }
    } else if (score_delay.has_value() && update_index < update_list.size()) {
        if (current_ms > score_delay.value()) {
            if (score_animator.has_value() && !score_animator->is_finished) {
                std::string field_name = std::get<0>(update_list[update_index]);
                int curr_num = std::get<1>(update_list[update_index]);
                std::string next_score_str = score_animator->next_score();
                int new_num = std::stoi(next_score_str);

                if (field_name == "score") score = next_score_str;
                else if (field_name == "good") good = next_score_str;
                else if (field_name == "ok") ok = next_score_str;
                else if (field_name == "bad") bad = next_score_str;
                else if (field_name == "max_combo") max_combo = next_score_str;
                else if (field_name == "total_drumroll") total_drumroll = next_score_str;

                if (new_num != curr_num) audio->play_sound("num_up", "sound");
                if (score_animator->is_finished) {
                    audio->play_sound("don", "sound");
                    score_delay.value() += 750;
                    if (update_index == update_list.size() - 1) {
                        return;
                    }
                    update_index += 1;
                    if (update_index < update_list.size()) {
                        score_animator = ScoreAnimator(std::get<1>(update_list[update_index]));
                    }
                }
                score_delay.value() += 16.67 * 3;
            }
        }
    }
    if (update_index > 0 && !high_score_indicator.has_value()) {
        SessionData& session_data = global_data.session_data[(int)player_num];
        if (session_data.result_data.score > session_data.result_data.prev_score) {
            high_score_indicator = HighScoreIndicator(session_data.result_data.prev_score, session_data.result_data.score, is_2p);
        }
    }
}

void ResultPlayer::update(double current_ms, bool fade_in_finished, bool is_skipped) {
    this->fade_in_finished = fade_in_finished;
    if (this->fade_in_finished && !gauge.has_value()) {
        gauge.emplace(ResultGauge(player_num, global_data.session_data[(int)player_num].result_data.gauge_length, is_2p));
        bottom_characters.start();
    }
    if (state.has_value()) {
        bottom_characters.update(state.value());
    }
    update_score_animation(current_ms, is_skipped);
    if (bottom_characters.is_finished() && !crown.has_value()) {
        if (gauge.has_value() && gauge->is_clear()) {
            crown.emplace(ResultCrown(is_2p));
        }
    }
    if (high_score_indicator.has_value()) {
        high_score_indicator->update(current_ms);
    }
    fade_in_bottom->update(current_ms);
    nameplate.update(current_ms);
    if (gauge.has_value()) {
        gauge->update(current_ms);
        if (gauge->is_finished() && !score_delay.has_value()) {
            score_delay = current_ms + 1883;
        }
    }
    if (score_delay.has_value()) {
        if (current_ms > score_delay.value() && !fade_in_bottom->is_started) {
            fade_in_bottom->start();
            if (gauge.has_value()) state = gauge->get_state();
        }
    }
    if (crown.has_value()) crown->update(current_ms);
    //chara.update(current_ms, 100, False, False);
}

void ResultPlayer::draw_score_info() {
    std::vector<std::string> scores = {good, ok, bad, max_combo, total_drumroll};
    for (int j = 0; j < scores.size(); j++) {
        if (scores[j] == "") continue;
        std::string score_str = std::to_string(std::stoi(scores[j]));
        std::string reversed_score = score_str;
        std::reverse(reversed_score.begin(), reversed_score.end());
        for (int i = 0; i < reversed_score.size(); i++) {
            int digit = reversed_score[i] - '0';
            tex.draw_texture("score", "judge_num", {.frame=digit, .x=-(i * tex.skin_config["score_info_counter_margin"].x), .index=j + (is_2p * 5)});
        }
    }
}

void ResultPlayer::draw_total_score() {
    if (!fade_in_finished) return;
    if (global_data.config->general.score_method == ScoreMethod::SHINUCHI) {
        tex.draw_texture("score", "score_shinuchi", {.index=is_2p});
    } else {
        tex.draw_texture("score", "score", {.index=is_2p});
    }
    if (score != "") {
        std::string reversed_score = score;
        std::reverse(reversed_score.begin(), reversed_score.end());
        for (int i = 0; i < reversed_score.size(); i++) {
            int digit = reversed_score[i] - '0';
            tex.draw_texture("score", "score_num", {.frame=digit, .x=-(i*tex.skin_config["result_score_margin"].x), .index=is_2p});
        }
    }
}

void ResultPlayer::draw_modifiers() {
    if (global_data.modifiers[(int)player_num].display) {
        tex.draw_texture("score", "mod_doron", {.index=is_2p});
    }
    if (global_data.modifiers[(int)player_num].inverse) {
        tex.draw_texture("score", "mod_abekobe", {.index=is_2p});
    }
    if (global_data.modifiers[(int)player_num].random == 1) {
        tex.draw_texture("score", "mod_kimagure", {.index=is_2p});
    } else if (global_data.modifiers[(int)player_num].random == 2) {
        tex.draw_texture("score", "mod_detarame", {.index=is_2p});
    }
    if (global_data.modifiers[(int)player_num].speed >= 4) {
        tex.draw_texture("score", "mod_yonbai", {.index=is_2p});
    } else if (global_data.modifiers[(int)player_num].speed >= 3) {
        tex.draw_texture("score", "mod_sanbai", {.index=is_2p});
    } else if (global_data.modifiers[(int)player_num].speed > 1) {
        tex.draw_texture("score", "mod_baisaku", {.index=is_2p});
    }
}

void ResultPlayer::draw() {
    if (is_2p) {
        if (state.has_value() && state.value() == ResultState::FAIL) {
            tex.draw_texture("background", "gradient_fail", {.fade=std::min(0.4, fade_in_bottom->attribute)});
        } else if (state.has_value() && state.value() == ResultState::CLEAR) {
            tex.draw_texture("background", "gradient_clear", {.fade=std::min(0.4, fade_in_bottom->attribute)});
        }
    } else {
        float y = tex.skin_config["result_2p_offset"].y ? has_2p : 0;
        if (state.has_value() && state.value() == ResultState::FAIL) {
            tex.draw_texture("background", "gradient_fail", {.y=y, .fade=std::min(0.4, fade_in_bottom->attribute)});
        } else if (state.has_value() && state.value() >= ResultState::CLEAR) {
            tex.draw_texture("background", "gradient_clear", {.y=y, .fade=std::min(0.4, fade_in_bottom->attribute)});
        }
    }
    tex.draw_texture("score", "overlay", {.color=ray::Fade(ray::WHITE, 0.75), .index=is_2p});
    tex.draw_texture("score", "difficulty", {.frame=global_data.session_data[(int)player_num].selected_difficulty, .index=is_2p});
    if (!has_2p) bottom_characters.draw();

    tex.draw_texture("score", "judge_good", {.index=is_2p});
    tex.draw_texture("score", "judge_ok", {.index=is_2p});
    tex.draw_texture("score", "judge_bad", {.index=is_2p});
    tex.draw_texture("score", "max_combo", {.index=is_2p});
    tex.draw_texture("score", "drumroll", {.index=is_2p});

    draw_score_info();
    draw_total_score();

    if (crown.has_value()) crown->draw(crown_type);

    draw_modifiers();

    if (high_score_indicator.has_value()) high_score_indicator->draw();

    //self.chara.draw(y=tex.skin_config["result_chara"].y+(self.is_2p*tex.screen_height//2))
    if (gauge.has_value()) gauge->draw();
    nameplate.draw(tex.skin_config["result_nameplate"].x, tex.skin_config["result_nameplate"].y+(is_2p*tex.skin_config["result_nameplate"].height));
}
