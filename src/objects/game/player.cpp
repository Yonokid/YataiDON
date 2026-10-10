#include "player.h"
#include "../../libs/audio.h"
#include "../../libs/input.h"
#include "../../libs/scores.h"
#include "../../libs/text.h"
#include <algorithm>
#include <optional>
#include <vector>
#include <cmath>

Player::Player(std::optional<SongParser>& parser_ref, PlayerNum player_num_param, int difficulty_param,
       bool is_2p_param, const Modifiers& modifiers_param)
    : is_2p(is_2p_param)
    , is_dan(false)
    , player_num(player_num_param)
    , difficulty(difficulty_param)
    , visual_offset(global_data.config->general.visual_offset)
    , score_method(global_data.config->general.score_method)
    , modifiers(modifiers_param)
    , parser(parser_ref)
    , good_count(0)
    , ok_count(0)
    , bad_count(0)
    , combo(0)
    , score(0)
    , max_combo(0)
    , total_drumroll(0)
    , arc_points(25)
    , judge_x(0)
    , judge_y(0)
    , is_gogo_time(false)
    , autoplay_hit_side(Side::LEFT)
    , last_subdivision(-1)
    , combo_display(combo, 0)
    , score_counter(0, is_2p)
{
    init_player_textures();
    reset_chart();
    don_hitsound = "hitsound_don_" + std::to_string((int)player_num) + "p";
    kat_hitsound = "hitsound_kat_" + std::to_string((int)player_num) + "p";

    std::string pnum = std::to_string((int)player_num);
    lane_cover_tex_id = tex.get_texture("lane/" + pnum + "p_lane_cover");
    lane_icon_tex_id  = tex.get_texture("lane/" + pnum + "p_icon");
    for (int t = 1; t <= 9; ++t) {
        std::string name = "notes/" + std::to_string(t);
        note_tex_ids[t] = tex.has_texture(name) ? tex.get_texture(name) : nullptr;
    }

    if (parser.has_value() && !parser->metadata.course_data.empty()) {
        if (parser->metadata.course_data[difficulty].is_branching) {
        branch_indicator = BranchIndicator();
        }
    }
    std::string player_id = get_player_id(player_num);
    auto pd = scores_manager.get_player_data(player_id);
    nameplate = Nameplate(
        pd ? pd->username : "", pd ? pd->title : "",
        global_data.player_num,
        pd ? pd->dan : -1, pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    chara = make_chara_from_player_data(pd ? &*pd : nullptr, false, true);
    if (pd) {
        chara->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
        chara->apply_face(pd->chara_face_index);
    } else {
        chara->set_don_colors(chara_default_color_1(player_num), chara_default_color_2(player_num), {249, 240, 225, 255});
    }
    chara->set_anim(AnimIndex::DON_NORMAL);
    if (global_data.config->general.judge_counter && !is_2p) {
        judge_counter = JudgeCounter();
    }
}

void Player::apply_replay_appearance(const PlayerData& pd) {
    nameplate = Nameplate(pd.username, pd.title, player_num, pd.dan, pd.gold, pd.rainbow, pd.title_bg);
    chara = make_chara_from_player_data(&pd, false, true);
    chara->set_don_colors(pd.chara_color_1, pd.chara_color_2, pd.chara_color_3);
    chara->apply_face(pd.chara_face_index);
    chara->set_anim(AnimIndex::DON_NORMAL);
}

void Player::init_player_textures() {
    t_lane_background = tex.get_texture("lane/lane_background");
    t_ai_lane_background = tex.get_texture("lane/ai_lane_background");
    t_lane_hit_circle = tex.get_texture("lane/lane_hit_circle");
    t_dan_lane_cover = tex.get_texture("lane/dan_lane_cover");
    t_drum = tex.get_texture("lane/drum");
    t_lane_difficulty = tex.get_texture("lane/lane_difficulty");
    t_timer = tex.get_texture("lane/timer");
    t_auto_icon = tex.get_texture("lane/auto_icon_" + global_data.config->general.language);
    t_lane_score_cover = tex.get_texture("lane/lane_score_cover");
    t_mod_shinuchi = tex.has_texture("lane/mod_shinuchi") ? tex.get_texture("lane/mod_shinuchi") : nullptr;

    t_notes_0 = tex.get_texture("notes/0");
    t_notes_8 = tex.get_texture("notes/8");
    t_notes_9 = tex.get_texture("notes/9");
    t_notes_10 = tex.get_texture("notes/10");
    t_moji = tex.get_texture("notes/moji");
    t_moji_drumroll_mid = tex.get_texture("notes/moji_drumroll_mid");
    t_drumroll_big_tail = tex.get_texture("notes/drumroll_big_tail");
    t_drumroll_tail = tex.get_texture("notes/drumroll_tail");

    // Badge for the current speed: the cabinet has one per value (x1.1 .. x4);
    // fall back to the three coarse tiers when the skin does not ship them.
    // modifiers/score_method are fixed for this Player's lifetime, so the whole
    // badge set is resolved once here instead of every frame from draw_modifiers().
    auto speed_badge = [&]() -> std::optional<TextureObject*> {
        if (modifiers.speed <= 10) return std::nullopt;
        static const std::pair<int, const char*> labels[] = {
            {11, "x1_1"}, {12, "x1_2"}, {13, "x1_3"}, {14, "x1_4"}, {15, "x1_5"}, {16, "x1_6"},
            {17, "x1_7"}, {18, "x1_8"}, {19, "x1_9"}, {20, "x2"},   {25, "x2_5"}, {30, "x3"},
            {35, "x3_5"}, {40, "x4"}};
        const char* label = labels[0].second;
        for (const auto& [v, l] : labels) if (modifiers.speed >= v) label = l;
        std::string name = std::string("lane/mod_speed_") + label;
        if (tex.has_texture(name)) return tex.get_texture(name);
        if (modifiers.speed >= 40 && tex.has_texture("lane/mod_yonbai")) return tex.get_texture("lane/mod_yonbai");
        if (modifiers.speed >= 30 && tex.has_texture("lane/mod_sanbai")) return tex.get_texture("lane/mod_sanbai");
        if (tex.has_texture("lane/mod_baisaku")) return tex.get_texture("lane/mod_baisaku");
        return std::nullopt;
    };

    // Cabinet order: speed, doron, abekobe, random.
    t_badges.clear();
    if (auto sb = speed_badge()) t_badges.push_back(*sb);
    if (modifiers.display && tex.has_texture("lane/mod_doron")) t_badges.push_back(tex.get_texture("lane/mod_doron"));
    if (modifiers.inverse && tex.has_texture("lane/mod_abekobe")) t_badges.push_back(tex.get_texture("lane/mod_abekobe"));
    if (modifiers.random == 2 && tex.has_texture("lane/mod_detarame")) t_badges.push_back(tex.get_texture("lane/mod_detarame"));
    else if (modifiers.random == 1 && tex.has_texture("lane/mod_kimagure")) t_badges.push_back(tex.get_texture("lane/mod_kimagure"));
}

ResultData Player::get_result_score() {
    ResultData result = ResultData();
    result.score = score;
    result.good = good_count;
    result.ok = ok_count;
    result.bad = bad_count;
    result.max_combo = max_combo;
    result.total_drumroll = total_drumroll;
    result.hit_offset_sum_ms = hit_offset_sum_ms;
    result.hit_offset_count = hit_offset_count;
    if (dan_gauge) result.gauge_length = dan_gauge->get_length() * 0.87f;
    else if (gauge.has_value()) result.gauge_length = gauge->get_length() * 0.87f;
    if (dan_gauge) result.cleared = dan_gauge->get_is_clear();
    else if (gauge.has_value()) result.cleared = gauge->get_is_clear();
    if (skipped_run) {
        result.gauge_length = 0.0f;
        result.cleared = false;
    }
    return result;
}

static constexpr int MISS_STREAK_TINT = 6;   // misses in a row that darken the Don and the background

AnimIndex Player::rest_anim() const {
    if (is_gogo_time) return AnimIndex::DON_SABI;
    return was_gauge_clear ? AnimIndex::DON_NORM_LOOP : AnimIndex::DON_NORMAL;
}

void Player::on_miss() {
    miss_streak++;
    if (!is_gogo_time && miss_streak <= MISS_STREAK_TINT)
        chara->set_anim(miss_streak == MISS_STREAK_TINT ? AnimIndex::DON_MISS6 : AnimIndex::DON_MISS);
}

void Player::spawn_ending_anim(Background* background) {
    ending_background = background;
    if (skipped_run) {
        ending_anim.reset();
        return;
    }
    if (!gauge.has_value() && !dan_gauge) return;
    bool is_clear = dan_gauge ? dan_gauge->get_is_clear() : gauge->get_is_clear();
    const char* kind;
    if (!is_clear) {
        ending_anim = FailAnimation(is_2p);
        kind = "fail";
    } else if (bad_count == 0) {
        ending_anim = FCAnimation(is_2p, ok_count == 0);
        kind = (ok_count == 0) ? "donderful" : "full_combo";
        chara->set_anim(AnimIndex::DON_FULL_COMBO);
    } else {
        ending_anim = ClearAnimation(is_2p);
        kind = "clear";
    }
    if (ending_background && ending_background->wants_handle_ending())
        ending_background->handle_ending(player_num, kind);
}

void Player::reload_for_dan(std::optional<SongParser>& new_parser, int new_difficulty) {
    parser = new_parser;
    difficulty = new_difficulty;

    don_notes.clear();
    kat_notes.clear();
    other_notes.clear();
    draw_note_list.clear();
    draw_note_buffer.clear();
    branch_m.clear();
    branch_e.clear();
    branch_n.clear();
    timeline.clear();
    timeline_buffer.clear();
    if (!is_2p) {
        camera_eases.clear();
        global_data.camera = CameraConfig();
    }
    draw_judge_list.clear();

    gauge.reset();
    reset_chart();
    gauge.reset();  // reset_chart recreates gauge; discard it for dan mode
    branch_history.clear();  // switching charts: old song's branch decisions don't apply here
    skipped_run = false;
}

void Player::handle_timeline(double ms_from_start) {
    update_camera(ms_from_start);
    if (timeline.empty()) return;
    // Drain everything due: one per frame lets same-time commands trail by frames
    while (!timeline.empty() && ms_from_start > timeline.front().start_time) {
        timeline_buffer.push_back(timeline.front());
        timeline.pop_front();
    }

    for (int i = (int)timeline_buffer.size() - 1; i >= 0; i--) {
        // NOTE: handlers may erase timeline_buffer[i]; take a copy and stop
        // dispatching as soon as the entry has been consumed.
        TimelineObject entry = timeline_buffer[i];
        const size_t before = timeline_buffer.size();
        handle_bpmchange(ms_from_start, entry, i);
        if (timeline_buffer.size() != before) continue;
        handle_judgeposition(ms_from_start, entry, i);
        if (timeline_buffer.size() != before) continue;
        handle_gogotime(ms_from_start, entry, i);
        if (timeline_buffer.size() != before) continue;
        handle_branch_param(ms_from_start, entry, i);
        if (timeline_buffer.size() != before) continue;
        handle_lyric(ms_from_start, entry, i);
        if (timeline_buffer.size() != before) continue;
        handle_camera(ms_from_start, entry, i);
        if (timeline_buffer.size() != before) continue;
        handle_section(ms_from_start, entry, i);
    }
}

void Player::autoplay_manager(double ms_from_start, double current_ms, std::optional<Background>& background) {
    if (!modifiers.auto_play) return;

    double subdivision_in_ms;
    DrumType hit_type;
    if (is_drumroll || is_balloon) {
        if (bpm == 0) {
            subdivision_in_ms = 0;
        } else {
            subdivision_in_ms = static_cast<int>(ms_from_start / ((240000.0 / bpm) / 24.0));
        }
        if (subdivision_in_ms > last_subdivision) {
            last_subdivision = subdivision_in_ms;
            hit_type = DrumType::DON;
            autoplay_hit_side = autoplay_hit_side == Side::LEFT ? Side::RIGHT : Side::LEFT;
            spawn_hit_effects(hit_type, autoplay_hit_side);
            audio.play_sound(don_hitsound, VolumePreset::HITSOUND);
            check_note(ms_from_start, hit_type, current_ms, background);
        }
    } else {
        auto autoplay_hit = [&](DrumType type, bool big) {
            if (big) {
                spawn_hit_effects(type, Side::LEFT);
                spawn_hit_effects(type, Side::RIGHT);
            } else {
                autoplay_hit_side = autoplay_hit_side == Side::LEFT ? Side::RIGHT : Side::LEFT;
                spawn_hit_effects(type, autoplay_hit_side);
            }
        };

        const double bad_window = (difficulty <= (int)Difficulty::NORMAL)
                                ? Timing::BAD_EASY : Timing::BAD;

        while (!don_notes.empty() && ms_from_start >= don_notes.front().hit_ms) {
            if (ms_from_start > don_notes.front().hit_ms + bad_window) break;
            const size_t remaining = don_notes.size();
            hit_type = DrumType::DON;
            autoplay_hit(hit_type, don_notes.front().type == NoteType::DON_L);
            audio.play_sound(don_hitsound, VolumePreset::HITSOUND);
            check_note(ms_from_start, hit_type, current_ms, background);
            last_note_hit = current_ms;
            if (don_notes.size() == remaining) break;
        }

        while (!kat_notes.empty() && ms_from_start >= kat_notes.front().hit_ms) {
            if (ms_from_start > kat_notes.front().hit_ms + bad_window) break;
            const size_t remaining = kat_notes.size();
            hit_type = DrumType::KAT;
            autoplay_hit(hit_type, kat_notes.front().type == NoteType::KAT_L);
            audio.play_sound(kat_hitsound, VolumePreset::HITSOUND);
            check_note(ms_from_start, hit_type, current_ms, background);
            if (kat_notes.size() == remaining) break;
        }
    }
}

void Player::merge_branch_section(const NoteList& branch_section, double current_ms) {
    const double boundary_eps = 1.0;
    std::deque<Note> notes;
    for (const Note& note : branch_section.notes)
        if (note.hit_ms >= resume_filter_ms - boundary_eps) notes.push_back(note);

    draw_note_list.insert(draw_note_list.end(), notes.begin(), notes.end());

    std::sort(draw_note_list.begin(), draw_note_list.end(),
              [](const Note& a, const Note& b) { return a.load_ms < b.load_ms; });

    timeline.insert(timeline.begin(), branch_section.timeline.begin(), branch_section.timeline.end());

    std::sort(timeline.begin(), timeline.end(),
              [](const TimelineObject& a, const TimelineObject& b) { return a.start_time < b.start_time; });

    for (const auto& note : notes) {

        if (note.type == NoteType::DON || note.type == NoteType::DON_L) {
            auto pos = std::lower_bound(don_notes.begin(), don_notes.end(), note,
                [](const auto& a, const auto& b) { return a.hit_ms < b.hit_ms; });
            don_notes.insert(pos, note);
        } else if (note.type == NoteType::KAT || note.type == NoteType::KAT_L) {
            auto pos = std::lower_bound(kat_notes.begin(), kat_notes.end(), note,
                [](const auto& a, const auto& b) { return a.hit_ms < b.hit_ms; });
            kat_notes.insert(pos, note);
        } else if (note.type != NoteType::BARLINE) {
            auto pos = std::lower_bound(other_notes.begin(), other_notes.end(), note,
                [](const auto& a, const auto& b) { return a.hit_ms < b.hit_ms; });
            other_notes.insert(pos, note);
        }
    }
}

void Player::evaluate_branch(double current_ms) {
    float e_req = std::get<0>(curr_branch_reqs);
    float m_req = std::get<1>(curr_branch_reqs);
    double branch_end_ms = std::get<2>(curr_branch_reqs);
    if (current_ms >= branch_end_ms) {
        is_branch = false;
        if (branch_condition == "p") {
            branch_p_count = branch_note_count != 0 ? std::max(std::min((int)((double)branch_p_count / branch_note_count * 100), 100), 0) : 0;
        } else if (branch_condition == "r") {
            branch_r_count = std::max(curr_drumroll_count, branch_r_count);
        }
        float count = branch_condition == "p" ? branch_p_count : branch_r_count;
        if (branch_indicator.has_value()) {
            spdlog::info("Branch set to {} based on conditions {}, {}, {}", branch_diff_to_string(branch_indicator->difficulty), count, e_req, m_req);
        }
        BranchDifficulty chosen;
        if (branch_checkpoint_index < branch_history.size()) {
            chosen = branch_history[branch_checkpoint_index];
        } else {
            chosen = (count >= e_req && count < m_req && e_req >= 0) ? BranchDifficulty::EXPERT
                    : (count >= m_req)                                ? BranchDifficulty::MASTER
                                                                       : BranchDifficulty::NORMAL;
            branch_history.push_back(chosen);
        }
        branch_checkpoint_index++;
        if (chosen == BranchDifficulty::EXPERT) {
            if (!branch_e.empty()) {
                merge_branch_section(branch_e.front(), current_ms);
                branch_e.pop_front();
                if (branch_indicator.has_value() and branch_indicator->difficulty != BranchDifficulty::EXPERT) {
                    if (branch_indicator->difficulty == BranchDifficulty::MASTER) {
                        branch_indicator->level_down(BranchDifficulty::EXPERT);
                    } else {
                        branch_indicator->level_up(BranchDifficulty::EXPERT);
                    }
                }
            }
            if (!branch_m.empty()) {
                branch_m.pop_front();
            }
            if (!branch_n.empty()) {
                branch_n.pop_front();
            }
        } else if (chosen == BranchDifficulty::MASTER) {
            if (!branch_m.empty()) {
                merge_branch_section(branch_m.front(), current_ms);
                branch_m.pop_front();
                if (branch_indicator.has_value() and branch_indicator->difficulty != BranchDifficulty::MASTER) {
                    branch_indicator->level_up(BranchDifficulty::MASTER);
                }
            }
            if (!branch_n.empty()) {
                branch_n.pop_front();
            }
            if (!branch_e.empty()) {
                branch_e.pop_front();
            }
        } else {
            if (!branch_n.empty()) {
                merge_branch_section(branch_n.front(), current_ms);
                branch_n.erase(branch_n.begin());
                if (branch_indicator.has_value() and branch_indicator->difficulty != BranchDifficulty::NORMAL) {
                    branch_indicator->level_down(BranchDifficulty::NORMAL);
                }
            }
            if (!branch_m.empty()) {
                branch_m.pop_front();
            }
            if (!branch_e.empty()) {
                branch_e.pop_front();
            }
        }
        branch_p_count = 0;
        branch_r_count = 0;
        branch_note_count = 0;
    }
}

void Player::update(double ms_from_start, double current_ms, std::optional<Background>& background) {
    bg_hook = background.has_value() ? &background.value() : nullptr;

    note_manager(ms_from_start, background);
    combo_display.update(current_ms, combo);
    if (combo_announce.has_value()) {
        combo_announce->update(current_ms);
    }
    drumroll_counter_manager(current_ms);
    balloon_counter_manager(current_ms);
    kusudama_counter_manager(current_ms);
    for (auto it = draw_judge_list.begin(); it != draw_judge_list.end(); ) {
        it->update(current_ms);
        if (it->is_finished()) {
            it = draw_judge_list.erase(it);
        } else {
            ++it;
        }
    }
    if (gogo_time.has_value()) {
        gogo_time->update(current_ms);
    }
    if (fireworks.has_value()) {
        fireworks->update(current_ms);
        if (fireworks->is_finished()) {
            fireworks.reset();
        }
    }
    if (lane_hit_effect.has_value()) {
        lane_hit_effect->update(current_ms);
        if (lane_hit_effect->is_finished()) {
            lane_hit_effect.reset();
        }
    }
    for (auto it = draw_drum_hit_list.begin(); it != draw_drum_hit_list.end(); ) {
        (*it)->update(current_ms);
        if ((*it)->is_finished()) {
            it = draw_drum_hit_list.erase(it);
        } else {
            ++it;
        }
    }
    handle_timeline(ms_from_start);

    for (auto it = draw_arc_list.begin(); it != draw_arc_list.end(); ) {
        it->update(current_ms);
        if (it->consume_note_finished()) {
            NoteType note_type = it->note_type;
            bool is_big = it->is_big;
            if (is_big) {  // there should be a better way to do this
                if (note_type == NoteType::DON) {
                    note_type = NoteType::DON_L;
                } else if (note_type == NoteType::KAT) {
                    note_type = NoteType::KAT_L;
                }
            }
            if (note_type == NoteType::BALLOON_HEAD || is_balloon) {
                note_type = NoteType::DON_L;
            }
            gauge_hit_effect.push_back(GaugeHitEffect(note_type, is_big, arc_player() == PlayerNum::P2));
        } else if (it->is_finished()) {
            it = draw_arc_list.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = gauge_hit_effect.begin(); it != gauge_hit_effect.end(); ) {
        it->update(current_ms);
        if (it->is_finished()) {
            it = gauge_hit_effect.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = base_score_list.begin(); it != base_score_list.end(); ) {
        it->update(current_ms);
        if (it->is_finished()) {
            it = base_score_list.erase(it);
            if (tex.options[SCO::DELAY_SCORE_ADDITION])
                score_counter.update_count(score);
        } else {
            ++it;
        }
    }
    if (!tex.options[SCO::DELAY_SCORE_ADDITION]) {
        score_counter.update_count(score);
    }
    score_counter.update(current_ms);
    autoplay_manager(ms_from_start, current_ms, background);
    handle_input(ms_from_start, current_ms, background);
    nameplate.update(current_ms);
    if (dan_gauge) {
        dan_gauge->update(current_ms);
    } else if (gauge.has_value()) {
        gauge->update(current_ms);
        if (background.has_value()) {
            background->handle_gauge(player_num, gauge->get_length() / 100.0f, gauge->get_is_clear(), gauge->get_is_rainbow());
            if ((int)is_gogo_time != last_reported_gogo) {
                last_reported_gogo = (int)is_gogo_time;
                background->handle_gogo(player_num, is_gogo_time);
            }
            if (score != last_reported_score) {
                last_reported_score = score;
                background->handle_score(player_num, score);
            }
        }
        bool gauge_full_now = gauge->get_is_rainbow();
        if (gauge_full_now && !was_gauge_full) {
            chara->set_anim(AnimIndex::DON_FULL_GAGE);
        }
        was_gauge_full = gauge_full_now;
        const bool streak_now = miss_streak >= MISS_STREAK_TINT;
        if (streak_now != tinted_miss_streak || gauge_full_now != tinted_rainbow) {
            if (tinted_miss_streak && !streak_now) chara->set_anim(rest_anim());
            tinted_miss_streak = streak_now;
            tinted_rainbow = gauge_full_now;
            if (streak_now) chara->set_tint({0, 0, 0, 255}, 0.35f);
            else if (gauge_full_now) chara->set_tint({255, 220, 0, 255}, 0.5f);
            else chara->set_tint({0, 0, 0, 255}, 0.0f);
            if (background.has_value()) background->handle_miss_streak(player_num, streak_now);
        }
        const bool gauge_clear_now = gauge->get_is_clear();
        if (gauge_clear_now != was_gauge_clear) {
            was_gauge_clear = gauge_clear_now;
            if (!is_gogo_time) {
                chara->set_anim(rest_anim());
                chara->set_anim(gauge_clear_now ? AnimIndex::DON_NORM_UP : AnimIndex::DON_NORM_DOWN);
            }
        }
        // balloon being hit vs. waiting for a hit
        if (is_balloon && !balloon_idle && current_ms - last_balloon_hit_ms > 400.0) {
            balloon_idle = true;
            chara->set_anim(AnimIndex::DON_BALLOON_NOBEAT);
        }
    }
    if (judge_counter.has_value()) {
        judge_counter->update(good_count, ok_count, bad_count, total_drumroll);
    }
    if (branch_indicator.has_value()) {
        branch_indicator->update(current_ms);
    }
    if (ending_anim.has_value()) {
        std::visit([&current_ms](auto& anim) { anim.update(current_ms); }, ending_anim.value());
    }

    if (is_branch) {
        evaluate_branch(ms_from_start);
    }
    chara->update(current_ms);
}

void Player::draw(double ms_from_start, float x, float y, ray::Shader& mask_shader) {
    tex.draw_texture(t_lane_background, {.y=y});
    if (player_num == PlayerNum::AI) tex.draw_texture(t_ai_lane_background, {.y=y});
    if (branch_indicator.has_value()) {
        branch_indicator->draw(y);
    }
    if (gauge.has_value()) {
        if (arc_player() == PlayerNum::P2) {   // the bottom gauge only when two play
            gauge->draw(y + tex.skin_config[SC::GAUGE_2P_OFFSET].y);
        } else {
            gauge->draw(y);
        }
        if (bg_hook) bg_hook->draw_gauge(player_num);
    }
    tex.draw_texture(t_lane_hit_circle, {.x = judge_x, .y = y + judge_y});

    if (gogo_time.has_value()) {
        gogo_time->draw(judge_x, y + judge_y);
    }
    if (lane_hit_effect.has_value()) {
        lane_hit_effect->draw(y);
    }
    if (fireworks.has_value()) {
        fireworks->draw();
    }
    draw_lane_cover(y);

    for (Judgment& anim : draw_judge_list) {
        anim.draw_effect(judge_x, y + judge_y);
    }
    {
        int scissor_x = virtual_to_screen_x(lane_cover_tex_id->scissor_right());
        int win_w = ray::GetRenderWidth();
        ray::BeginScissorMode(scissor_x, 0, win_w - scissor_x, ray::GetRenderHeight());
        draw_notes(ms_from_start, y);
        ray::EndScissorMode();
    }

    for (Judgment& anim : draw_judge_list) {
        anim.draw_outer_effect(judge_x, y + judge_y);
        anim.draw_ray_effect(judge_x, y + judge_y);
    }
    for (Judgment& anim : draw_judge_list) {
        anim.draw_text(judge_x, y + judge_y);
    }

    draw_overlays(y, mask_shader);

    if (global_data.config->general.song_timer) {
        draw_song_timer(ms_from_start, y);
    }
}

void Player::draw_practice(double ms_from_start, float x, float y, ray::Shader& mask_shader, bool draw_notes_on) {
    practice_lyric = true;
    tex.draw_texture(t_lane_background, {.y=y});
    if (player_num == PlayerNum::AI) tex.draw_texture(t_ai_lane_background, {.y=y});
    if (branch_indicator.has_value()) {
        branch_indicator->draw(y);
    }
    if (lane_hit_effect.has_value()) {
        lane_hit_effect->draw(y);
    }
    tex.draw_texture(t_lane_hit_circle, {.x = judge_x, .y = y + judge_y});

    if (gogo_time.has_value()) {
        gogo_time->draw(judge_x, y + judge_y);
    }
    if (fireworks.has_value()) {
        fireworks->draw();
    }
    draw_lane_cover(y);

    for (Judgment& anim : draw_judge_list) {
        anim.draw_effect(judge_x, y + judge_y);
    }

    if (draw_notes_on) {
        int scissor_x = virtual_to_screen_x(lane_cover_tex_id->scissor_right());
        int win_w = ray::GetRenderWidth();
        ray::BeginScissorMode(scissor_x, 0, win_w - scissor_x, ray::GetRenderHeight());
        draw_notes(ms_from_start, y);
        ray::EndScissorMode();
    }

    for (Judgment& anim : draw_judge_list) {
        anim.draw_outer_effect(judge_x, y + judge_y);
        anim.draw_ray_effect(judge_x, y + judge_y);
    }
    for (Judgment& anim : draw_judge_list) {
        anim.draw_text(judge_x, y + judge_y);
    }

    draw_overlays(y, mask_shader);

}

void Player::get_load_time(Note& note) {
    int note_half_w = t_notes_9->width / 2;
    float travel_distance = tex.screen_width - JudgePos::X;
    // The faster axis decides when a note comes on screen: a polar #SCROLL pointing straight up
    // leaves a horizontal component of ~1e-17, not 0
    bool horizontal = abs(note.scroll_x) >= abs(note.scroll_y);
    float base_pixels_per_ms = (note.bpm / 240000 * (horizontal ? abs(note.scroll_x) : abs(note.scroll_y)) * travel_distance);
    if (base_pixels_per_ms == 0) {
        note.load_ms = note.hit_ms;
        note.unload_ms = note.hit_ms;
        return;
    }
    float normal_travel_ms = (travel_distance + note_half_w) / base_pixels_per_ms;

    if (!note.sudden_appear_ms.has_value() ||
        !note.sudden_moving_ms.has_value() ||
        note.sudden_appear_ms.value() == std::numeric_limits<float>::infinity()) {
        if (scroll_type != ScrollType::NMSCROLL) {
            // On screen while (note beat - current beat) * px_per_beat is between the left edge
            // and the right edge
            double px_per_beat = (horizontal ? note.scroll_x : abs(note.scroll_y)) * travel_distance / 4;
            double left = horizontal ? -(JudgePos::X + note_half_w) : -(travel_distance + note_half_w);
            double right = travel_distance + note_half_w;
            double beat = tempo_map.beat_at(note.hit_ms);
            double lo = beat - right / px_per_beat, hi = beat - left / px_per_beat;
            auto [first, last] = tempo_map.ms_within(std::min(lo, hi), std::max(lo, hi));
            note.load_ms = std::min(first, note.hit_ms);
            note.unload_ms = std::max(last, note.hit_ms);
            return;
        }
        note.load_ms = note.hit_ms - normal_travel_ms;
        note.unload_ms = note.hit_ms + normal_travel_ms;
        return;
    }
    note.load_ms = note.hit_ms - note.sudden_appear_ms.value();
    float movement_duration = note.sudden_moving_ms.value();
    if (movement_duration <= 0) {
        movement_duration = normal_travel_ms;
    }
    float sudden_pixels_per_ms = travel_distance / movement_duration;
    float unload_offset = travel_distance / sudden_pixels_per_ms;
    note.unload_ms = note.hit_ms + unload_offset;
}

// #BMSCROLL / #HBSCROLL charts can hold notes behind a #DELAY that outlasts the song (a wall of
// notes frozen on screen). They are drawn but never judged, counted or waited for.
bool Player::unplayable(const Note& note) const {
    return scroll_type != ScrollType::NMSCROLL && audio_end_ms.has_value()
        && note.type != NoteType::BARLINE && note.hit_ms > audio_end_ms.value();
}

void Player::set_audio_end(double chart_ms) {
    audio_end_ms = chart_ms;
    if (scroll_type != ScrollType::NMSCROLL && end_time > chart_ms) reset_chart();
}

void Player::reset_chart() {
    if (!parser.has_value()) return;

    don_notes.clear();
    kat_notes.clear();
    other_notes.clear();
    draw_note_list.clear();
    draw_note_buffer.clear();
    barlines.clear();

    auto [notes, branch_m_temp, branch_e_temp, branch_n_temp] = parser->notes_to_position(difficulty);
    apply_modifiers(notes, modifiers);

    Note* last_note = nullptr;
    end_time = 0;
    bpm = parser->metadata.bpm;
    scroll_type = notes.scroll_type;
    tempo_map = notes.tempo_map;

    for (Note& note: notes.notes) {
        get_load_time(note);
        if (note.type == NoteType::TAIL && last_note != nullptr) {
            note.load_ms = last_note->load_ms;
            last_note->unload_ms = note.unload_ms;
            auto it = std::find_if(draw_note_list.begin(), draw_note_list.end(),
                [&](const Note& n) { return n.index == last_note->index; });
            if (it != draw_note_list.end()) {
                it->unload_ms = note.unload_ms;
            }
        }
        bool playable = !unplayable(note);
        if (playable) {
            if (note.type == NoteType::DON || note.type == NoteType::DON_L) {
                don_notes.push_back(note);
            } else if (note.type == NoteType::KAT || note.type == NoteType::KAT_L) {
                kat_notes.push_back(note);
            } else if (note.type != NoteType::BARLINE) {
                other_notes.push_back(note);
            }
        }
        draw_note_list.push_back(note);
        if (note.type != NoteType::BARLINE) {
            last_note = &note;
        }

        if (playable && note.hit_ms > end_time) {
            end_time = note.hit_ms;
        }
    }

    std::sort(draw_note_list.begin(), draw_note_list.end(),
              [](const Note& a, const Note& b) { return a.load_ms < b.load_ms; });

    this->branch_m = branch_m_temp;
    this->branch_e = branch_e_temp;
    this->branch_n = branch_n_temp;
    std::vector<std::reference_wrapper<std::deque<NoteList>>> branches = {
        std::ref(this->branch_m),
        std::ref(this->branch_e),
        std::ref(this->branch_n)
    };

    for (auto& branch_ref : branches) {
        std::deque<NoteList>& branch = branch_ref.get();

        if (!branch.empty()) {
            for (NoteList& section : branch) {
                apply_modifiers(section, modifiers);
                std::erase_if(section.notes, [&](const Note& n) { return unplayable(n); });
                Note* last_note = nullptr;
                for (Note& note: section.notes) {
                    get_load_time(note);
                    if (note.type == NoteType::TAIL && last_note != nullptr) {
                        note.load_ms = last_note->load_ms;
                        last_note->unload_ms = note.unload_ms;
                    }
                    last_note = &note;

                    if (note.hit_ms > end_time) {
                        end_time = note.hit_ms;
                    }
                }
            }
        }
    }

    this->timeline = notes.timeline;
    last_jpos_key = {-1e300, -1};

    // Rasterize every #LYRIC glyph now, in one go: otherwise each new line with an
    // unseen character rebuilt the lyric-size font atlas mid-song (a ~20 ms hitch
    // per line).
    {
        std::string all_lyrics;
        for (const TimelineObject& t : this->timeline)
            if (t.lyric.has_value()) all_lyrics += t.lyric.value();
        if (!all_lyrics.empty()) {
            const SkinInfo* lyric_cfg = tex.skin_entry("lyric");
            int lyric_font = (lyric_cfg && lyric_cfg->font_size > 0) ? lyric_cfg->font_size
                                                                     : static_cast<int>(40 * tex.screen_scale);
            font_manager.register_text(all_lyrics, lyric_font);
        }
    }

    std::sort(this->timeline.begin(), this->timeline.end(),
              [](const TimelineObject& a, const TimelineObject& b) { return a.start_time < b.start_time; });

    is_drumroll = false;
    curr_drumroll_count = 0;
    is_balloon = false;
    curr_balloon_count = 0;
    kusudama_shared_hits = 0;
    last_subdivision = -1;
    is_branch = false;
    branch_condition = "";
    branch_p_count = 0;
    branch_r_count = 0;
    branch_note_count = 0;
    branch_checkpoint_index = 0;

    NoteList total_notes; //all notes including master branch

    std::copy_if(notes.notes.begin(), notes.notes.end(), std::back_inserter(total_notes.notes),
                 [&](const Note& n) { return !unplayable(n); });
    for (NoteList section : branch_m) {
        total_notes.notes.insert(total_notes.notes.end(), section.notes.begin(), section.notes.end());
    }

    //setup gauge
    int stars = 0;
    if (parser->metadata.course_data.empty()) {
        stars = 10;
    } else {
        stars = parser->metadata.course_data[difficulty].level;
    }
    if (stars == 0) {
        difficulty = 3;
        stars = 10;
    }
    int gauge_total_notes = 0;
    for (Note& note : total_notes.notes) {
        if (note.type >= NoteType::DON && note.type <= NoteType::KAT_L) {
            gauge_total_notes++;
        }
    }
    judgeable_note_count = gauge_total_notes;
    gauge = Gauge(gauge_total_notes, difficulty, stars, player_num);

    //setup score
    base_score = 0;
    score_init = 0;
    score_diff = 0;
    if (score_method == ScoreMethod::SHINUCHI) {
        base_score = calculate_base_score(total_notes);
    } else if (score_method == ScoreMethod::GEN3) {
        if (difficulty < 0 || difficulty >= (int)parser->metadata.course_data.size()) {
            score_init = calculate_base_score(total_notes);
            score_diff = 0;
            return;
        }
        score_diff = parser->metadata.course_data[difficulty].scorediff;
        if (score_diff <= 0) {
            spdlog::warn("Error: No scorediff specified or scorediff less than 0 | Using shinuchi scoring method instead");
            score_diff = 0;
        }

        std::vector<int> score_init_list = parser->metadata.course_data[difficulty].scoreinit;
        if (score_init_list.empty()) {
            spdlog::warn("Error: No scoreinit specified or scoreinit less than 0 | Using shinuchi scoring method instead");
            score_init = calculate_base_score(total_notes);
            score_diff = 0;
        } else {
            score_init = score_init_list[0];
        }
    }
}

std::optional<Note> Player::get_first_note() {
    if (draw_note_list.empty()) return std::nullopt;
    // #BMSCROLL / #HBSCROLL: a note can be on screen long before it comes up (a slow #SCROLL, a
    // beat count that stops or runs back), so the earliest-loading note says nothing about how
    // long the lead-in has to be; take the first note to hit
    if (scroll_type != ScrollType::NMSCROLL) {
        auto first = std::min_element(draw_note_list.begin(), draw_note_list.end(),
            [](const Note& a, const Note& b) {
                if ((a.type == NoteType::BARLINE) != (b.type == NoteType::BARLINE)) return b.type == NoteType::BARLINE;
                return a.hit_ms < b.hit_ms;
            });
        return *first;
    }
    return draw_note_list.front();
}

// #BMSCROLL / #HBSCROLL: distance = beats to the note, so the field speeds up and slows down
// with the tempo and stops during a #DELAY. Otherwise: time to the note at the note's own BPM.
float Player::get_position_x(const Note& note, double current_ms) {
    if (scroll_type != ScrollType::NMSCROLL) {
        double beats = tempo_map.beat_at(note.hit_ms) - tempo_map.beat_at(current_ms);
        return JudgePos::X + beats / 4 * note.scroll_x * (tex.screen_width - JudgePos::X);
    }
    float speedx = note.bpm / 240000 * note.scroll_x * (tex.screen_width - JudgePos::X);
    return JudgePos::X + (note.hit_ms - current_ms) * speedx;
}

float Player::get_position_y(const Note& note, double current_ms) {
    if (scroll_type != ScrollType::NMSCROLL) {
        double beats = tempo_map.beat_at(note.hit_ms) - tempo_map.beat_at(current_ms);
        return beats / 4 * note.scroll_y * (tex.screen_width - JudgePos::X);
    }
    float speedy = note.bpm / 240000 * note.scroll_y * ((tex.screen_width - JudgePos::X)/tex.screen_width) * tex.screen_width;
    return (note.hit_ms - current_ms) * speedy;
}

void Player::handle_gogotime(double ms_from_start, const TimelineObject& timeline_object, int buffer_index) {
    if (timeline_object.start_time > ms_from_start) return;
    if (!timeline_object.gogo_time.has_value()) return;

    is_gogo_time = timeline_object.gogo_time.value();

    if (is_gogo_time) {
        gogo_time = GogoTime();
        fireworks = Fireworks();
        chara->set_anim(AnimIndex::DON_SABI);
        chara->set_anim(AnimIndex::DON_SABI_START);
    } else {
        gogo_time.reset();
        if (0 < miss_streak && miss_streak < MISS_STREAK_TINT) {
            chara->set_anim(AnimIndex::DON_MISS);
        } else if (0 < miss_streak) {
            chara->set_anim(AnimIndex::DON_MISS6);
        } else {
            chara->set_anim(rest_anim());
        }
    }

    if (buffer_index != (int)timeline_buffer.size() - 1)
        timeline_buffer[buffer_index] = std::move(timeline_buffer.back());
    timeline_buffer.pop_back();
}

void Player::handle_judgeposition(double ms_from_start, const TimelineObject& timeline_object, int buffer_index) {
    if (timeline_object.start_time > ms_from_start) return;
    if (!timeline_object.judge_pos_x.has_value()) return;
    if (!timeline_object.judge_pos_y.has_value()) return;
    if (!timeline_object.delta_x.has_value()) return;
    if (!timeline_object.delta_y.has_value()) return;

    const std::pair<double, int> key{timeline_object.start_time, timeline_object.seq};
    const bool newest = key >= last_jpos_key;
    if (newest) last_jpos_key = key;

    if (newest && timeline_object.start_time <= ms_from_start && ms_from_start <= timeline_object.end_time) {
        double duration = timeline_object.end_time - timeline_object.start_time;
        if (duration > 0) {
            double t = (ms_from_start - timeline_object.start_time) / duration;
            t = std::max(0.0, std::min(1.0, t));

            judge_x = (timeline_object.judge_pos_x.value() + (timeline_object.delta_x.value() * t)) * tex.screen_scale;
            judge_y = (timeline_object.judge_pos_y.value() + (timeline_object.delta_y.value() * t)) * tex.screen_scale;
        } else {
            judge_x = (timeline_object.judge_pos_x.value() + timeline_object.delta_x.value()) * tex.screen_scale;
            judge_y = (timeline_object.judge_pos_y.value() + timeline_object.delta_y.value()) * tex.screen_scale;
        }
    }

    if (ms_from_start > timeline_object.end_time) {
        if (newest) {
            judge_x = (timeline_object.judge_pos_x.value() + timeline_object.delta_x.value()) * tex.screen_scale;
            judge_y = (timeline_object.judge_pos_y.value() + timeline_object.delta_y.value()) * tex.screen_scale;
        }

        if (buffer_index != (int)timeline_buffer.size() - 1)
            timeline_buffer[buffer_index] = std::move(timeline_buffer.back());
        timeline_buffer.pop_back();
    }
}

void Player::handle_bpmchange(double ms_from_start, const TimelineObject& timeline_object, int buffer_index) {
    if (timeline_object.start_time > ms_from_start) return;
    if (!timeline_object.bpm.has_value()) return;

    // Negative in #HBSCROLL / #BMSCROLL charts that run the field backwards; animations and
    // autoplay only want the tempo
    bpm = std::abs(timeline_object.bpm.value());
    chara->set_bpm(bpm);

    if (buffer_index != (int)timeline_buffer.size() - 1)
        timeline_buffer[buffer_index] = std::move(timeline_buffer.back());
    timeline_buffer.pop_back();
}

void Player::handle_branch_param(double ms_from_start, const TimelineObject& timeline_object, int buffer_index) {
    if (timeline_object.start_time > ms_from_start) return;
    if (!timeline_object.branch_params.has_value()) return;

    std::string params = timeline_object.branch_params.value();

    std::vector<std::string> parts;
    std::stringstream ss(params);
    std::string part;
    while (std::getline(ss, part, ',')) {
        parts.push_back(part);
    }

    if (parts.size() >= 3) {
        std::string branch_cond = parts[0];
        float e_req = 0.0f, m_req = 0.0f;
        try {
            e_req = std::stof(parts[1]);
            m_req = std::stof(parts[2]);
        } catch (const std::exception& e) {
            spdlog::warn("Invalid #BRANCHSTART params '{}': {}", params, e.what());
            e_req = m_req = 0.0f;
        }

        if (!is_branch) {
            is_branch = true;
            branch_condition = branch_cond;

            double branch_condition_end_time;
            if (!branch_m.empty() && !branch_m.front().notes.empty()) {
                branch_condition_end_time = branch_m.front().notes.front().load_ms;
            } else if (!branch_e.empty() && !branch_e.front().notes.empty()) {
                branch_condition_end_time = branch_e.front().notes.front().load_ms;
            } else if (!branch_n.empty() && !branch_n.front().notes.empty()) {
                branch_condition_end_time = branch_n.front().notes.front().load_ms;
            } else if (!draw_note_list.empty()) {
                branch_condition_end_time = draw_note_list.front().load_ms;
            } else {
                branch_condition_end_time = ms_from_start;
            }

            if (branch_cond == "r") {
                curr_branch_reqs = std::make_tuple(e_req, m_req, branch_condition_end_time);
            } else if (branch_cond == "p") {
                curr_branch_reqs = std::make_tuple(e_req, m_req, branch_condition_end_time);
            }
            spdlog::info("branch condition measures started with conditions {}, {}, {}, starting at {} and ending at {}", branch_cond, e_req, m_req, timeline_object.start_time, branch_condition_end_time);
        }
    }
    if (buffer_index != (int)timeline_buffer.size() - 1)
        timeline_buffer[buffer_index] = std::move(timeline_buffer.back());
    timeline_buffer.pop_back();
}

void Player::handle_section(double ms_from_start, const TimelineObject& timeline_object, int buffer_index) {
    if (timeline_object.start_time > ms_from_start) return;
    if (!timeline_object.section_reset.has_value()) return;

    branch_p_count = 0;
    branch_r_count = 0;
    branch_note_count = 0;
    if (buffer_index != (int)timeline_buffer.size() - 1)
        timeline_buffer[buffer_index] = std::move(timeline_buffer.back());
    timeline_buffer.pop_back();
}

void Player::handle_lyric(double ms_from_start, const TimelineObject& timeline_object, int buffer_index) {
    if (timeline_object.start_time > ms_from_start) return;
    if (!timeline_object.lyric.has_value()) return;

    if (current_lyric.has_value()) {
        current_lyric.reset();
    }

    const SkinInfo* lyric_cfg = tex.skin_entry("lyric");
    int font_size = (lyric_cfg && lyric_cfg->font_size > 0) ? lyric_cfg->font_size
                                                             : static_cast<int>(40 * tex.screen_scale);
    float outline = (lyric_cfg && lyric_cfg->outline >= 0) ? lyric_cfg->outline : 4.0f * tex.screen_scale;
    current_lyric.emplace(timeline_object.lyric.value(), font_size, ray::WHITE, ray::BLUE, false, outline);
    if (buffer_index != (int)timeline_buffer.size() - 1)
        timeline_buffer[buffer_index] = std::move(timeline_buffer.back());
    timeline_buffer.pop_back();
}

static double ease_progress(double t, EaseDir dir, EaseCalc calc) {
    t = std::clamp(t, 0.0, 1.0);
    auto in = [calc](double x) {
        switch (calc) {
            case EaseCalc::CUBIC: return x * x * x;
            case EaseCalc::QUARTIC: return x * x * x * x;
            case EaseCalc::QUINTIC: return x * x * x * x * x;
            case EaseCalc::SINUSOIDAL: return 1.0 - std::cos(x * M_PI / 2.0);
            case EaseCalc::EXPONENTIAL: return x == 0.0 ? 0.0 : std::pow(2.0, 10.0 * (x - 1.0));
            case EaseCalc::CIRCULAR: return 1.0 - std::sqrt(1.0 - x * x);
            default: return x;
        }
    };
    if (dir == EaseDir::IN_) return in(t);
    if (dir == EaseDir::OUT_) return 1.0 - in(1.0 - t);
    return t < 0.5 ? in(t * 2.0) / 2.0 : 1.0 - in((1.0 - t) * 2.0) / 2.0;
}

static void apply_camera_prop(CamProp prop, float v) {
    CameraConfig& c = global_data.camera;
    switch (prop) {
        case CamProp::H_OFFSET: c.offset.x = v; break;
        case CamProp::V_OFFSET: c.offset.y = v; break;
        case CamProp::ZOOM: c.zoom = v; break;
        case CamProp::ROTATION: c.rotation = v; break;
        case CamProp::H_SCALE: c.h_scale = v; break;
        case CamProp::V_SCALE: c.v_scale = v; break;
        default: break;
    }
}

// TJAPlayer3-Extended camera commands drive the global camera; only P1 owns it.
void Player::handle_camera(double ms_from_start, const TimelineObject& timeline_object, int buffer_index) {
    if (timeline_object.start_time > ms_from_start) return;
    if (!timeline_object.cam.has_value()) return;

    if (!is_2p) {
        const CameraEvent& ev = timeline_object.cam.value();
        if (ev.prop == CamProp::RESET) {
            camera_eases.clear();
            ray::Color border = global_data.camera.border_color;
            global_data.camera = CameraConfig();
            global_data.camera.border_color = border;
        } else if (ev.prop == CamProp::BORDER_COLOR) {
            global_data.camera.border_color = {(unsigned char)std::clamp(ev.from, 0.0, 255.0),
                                               (unsigned char)std::clamp(ev.to, 0.0, 255.0),
                                               (unsigned char)std::clamp(ev.extra, 0.0, 255.0), 255};
        } else if (ev.ease) {
            std::erase_if(camera_eases, [&](const TimelineObject& t) { return t.cam->prop == ev.prop; });
            camera_eases.push_back(timeline_object);
        } else {
            std::erase_if(camera_eases, [&](const TimelineObject& t) { return t.cam->prop == ev.prop; });
            apply_camera_prop(ev.prop, (float)ev.to);
        }
    }
    if (buffer_index != (int)timeline_buffer.size() - 1)
        timeline_buffer[buffer_index] = std::move(timeline_buffer.back());
    timeline_buffer.pop_back();
}

void Player::update_camera(double ms_from_start) {
    for (const TimelineObject& t : camera_eases) {
        const CameraEvent& ev = t.cam.value();
        double len = t.end_time - t.start_time;
        double p = len > 0 ? (ms_from_start - t.start_time) / len : 1.0;
        double e = ease_progress(p, ev.dir, ev.calc);
        apply_camera_prop(ev.prop, (float)(ev.from + (ev.to - ev.from) * e));
    }
    std::erase_if(camera_eases, [&](const TimelineObject& t) { return ms_from_start >= t.end_time; });
}

void Player::play_note_manager(double current_ms, std::optional<Background>& background) {
    const double miss_window = (difficulty <= (int)Difficulty::NORMAL)
                             ? Timing::BAD_EASY : Timing::BAD;
    while (!don_notes.empty() && don_notes.front().hit_ms + miss_window < current_ms) {
        combo = 0;
        if (background.has_value()) background->handle_bad(PlayerNum(1 + is_2p));
        bad_count++;
        on_miss();
        note_judgments[don_notes.front().index] = Judgments::BAD;
        if (dan_gauge) dan_gauge->add_bad();
        else if (gauge.has_value()) gauge->add_bad();

        don_notes.pop_front();
        branch_note_count++;
    }

    while (!kat_notes.empty() && kat_notes.front().hit_ms + miss_window < current_ms) {
        combo = 0;
        if (background.has_value()) background->handle_bad(PlayerNum(1 + is_2p));
        bad_count++;
        on_miss();
        note_judgments[kat_notes.front().index] = Judgments::BAD;
        if (dan_gauge) dan_gauge->add_bad();
        else if (gauge.has_value()) gauge->add_bad();

        kat_notes.pop_front();
        branch_note_count++;
    }

    if (other_notes.empty()) return;

    Note& note = other_notes.front();
    if (note.hit_ms <= current_ms) {
        if (note.type == NoteType::ROLL_HEAD || note.type == NoteType::ROLL_HEAD_L) {
            is_drumroll = true;
        } else if (note.type == NoteType::BALLOON_HEAD || note.type == NoteType::KUSUDAMA) {
            if (!is_balloon) { // should not set animation DON_BALLOON_NOBEAT here
                balloon_idle = true;
            }
            is_balloon = true;
        } else if (note.type == NoteType::TAIL) {
            other_notes.pop_front();
            is_drumroll = false;
            is_balloon = false;
            curr_drumroll_count = 0;
            curr_balloon_count = 0;
            return;
        }
        if (other_notes.size() < 2) return;
        Note& tail = other_notes[1];
        if (tail.hit_ms <= current_ms) {
            const bool balloon_missed = note.type == NoteType::BALLOON_HEAD;   // a burst one is gone already
            other_notes.pop_front();
            other_notes.pop_front();
            if (balloon_missed && background.has_value()) background->handle_balloon_end(PlayerNum(is_2p + 1));
            is_drumroll = false;
            is_balloon = false;
            curr_drumroll_count = 0;
            curr_balloon_count = 0;
        }
    }
}

void Player::draw_note_manager(double current_ms) {
    current_ms += visual_offset;
    while (!draw_note_list.empty() && current_ms >= draw_note_list.front().load_ms) {
        Note current_note = draw_note_list.front();
        draw_note_list.pop_front();

        if (current_note.type >= NoteType::ROLL_HEAD && current_note.type <= NoteType::BALLOON_HEAD) {
            auto pos = std::lower_bound(draw_note_buffer.begin(), draw_note_buffer.end(),
                                        current_note,
                                        [](const auto& a, const auto& b) { return a.index < b.index; });
            draw_note_buffer.insert(pos, current_note);

            auto tail_it = std::find_if(draw_note_list.begin(), draw_note_list.end(),
                                        [&current_note](const auto& note) {
                                            return note.type == NoteType::TAIL && note.index > current_note.index;
                                        });

            if (tail_it != draw_note_list.end()) {
                auto tail_note = *tail_it;

                pos = std::lower_bound(draw_note_buffer.begin(), draw_note_buffer.end(),
                                      tail_note,
                                      [](const auto& a, const auto& b) { return a.index < b.index; });
                draw_note_buffer.insert(pos, tail_note);

                draw_note_list.erase(tail_it);
            }
        } else if (current_note.type == NoteType::BARLINE) {
            auto pos = std::lower_bound(barlines.begin(), barlines.end(),
                                        current_note,
                                        [](const auto& a, const auto& b) { return a.index < b.index; });
            barlines.insert(pos, current_note);
        } else {
            auto pos = std::lower_bound(draw_note_buffer.begin(), draw_note_buffer.end(),
                                        current_note,
                                        [](const auto& a, const auto& b) { return a.index < b.index; });
            draw_note_buffer.insert(pos, current_note);
        }
    }

    barlines.erase(
        std::remove_if(barlines.begin(), barlines.end(),
            [current_ms](const Note& n) { return current_ms >= n.unload_ms; }),
        barlines.end());

    if (draw_note_buffer.empty()) return;

    if (is_drumroll && !other_notes.empty()) {
        int active_drumroll_index = other_notes.front().index;
        auto drumroll_it = std::lower_bound(draw_note_buffer.begin(), draw_note_buffer.end(),
                                            active_drumroll_index,
                                            [](const Note& n, int idx) { return n.index < idx; });
        if (drumroll_it != draw_note_buffer.end() && drumroll_it->index == active_drumroll_index &&
            (drumroll_it->type == NoteType::ROLL_HEAD || drumroll_it->type == NoteType::ROLL_HEAD_L) &&
            drumroll_it->color.has_value() && last_drumroll_color_time + 16.67f < current_ms) {
            last_drumroll_color_time = current_ms;
            drumroll_it->color = std::min(255, drumroll_it->color.value() + 1);
        }
    }

    draw_note_buffer.erase(
        std::remove_if(draw_note_buffer.begin(), draw_note_buffer.end(),
            [current_ms](const Note& n) { return current_ms >= n.unload_ms; }),
        draw_note_buffer.end());
}

void Player::note_manager(double current_ms, std::optional<Background>& background) {
    play_note_manager(current_ms, background);
    draw_note_manager(current_ms);
}

void Player::note_correct(const Note& note, double current_ms) {
    miss_streak = 0;
    if (!don_notes.empty() && don_notes[0] == note) {
        don_notes.pop_front();
    } else if (!kat_notes.empty() && kat_notes[0] == note) {
        kat_notes.pop_front();
    } else if (!other_notes.empty() && other_notes[0] == note) {
        other_notes.pop_front();
    }

    int index = note.index;
    if (note.type == NoteType::BALLOON_HEAD || note.type == NoteType::KUSUDAMA) {
        if (!other_notes.empty()) {
            other_notes.pop_front();
        }
    }

    if (note.type < NoteType::BALLOON_HEAD) {
        combo++;
        if (combo % 10 == 0 && !is_gogo_time) {
            chara->set_anim(AnimIndex::DON_COMBO);
        }
        if (combo % 100 == 0) {
            combo_announce = ComboAnnounce(combo, current_ms, player_num);
        }
        if (combo > max_combo) {
            max_combo = combo;
        }
        if (combo % 100 == 0 && score_method == ScoreMethod::GEN3) {
            score += 10000;
            base_score_list.push_back(ScoreCounterAnimation(player_num, 10000, is_2p));
        }
    }

    if (note.type != NoteType::KUSUDAMA) {
        bool is_big = note.type == NoteType::DON_L || note.type == NoteType::KAT_L || note.type == NoteType::BALLOON_HEAD;
        draw_arc_list.push_back(NoteArc(note.type, current_ms, arc_player(), is_big, note.type == NoteType::BALLOON_HEAD, judge_x, judge_y));
    }
    auto it = std::lower_bound(draw_note_buffer.begin(), draw_note_buffer.end(),
                               note.index, [](const Note& n, int idx) { return n.index < idx; });
    if (it != draw_note_buffer.end() && *it == note) {
        draw_note_buffer.erase(it);
    }
}

void Player::check_drumroll(double current_ms, DrumType drum_type, std::optional<Background>& background) {
    const bool is_big = !other_notes.empty() && other_notes.front().type == NoteType::ROLL_HEAD_L;
    draw_arc_list.push_back(NoteArc(NoteType(drum_type), current_ms, arc_player(), is_big, false));
    curr_drumroll_count++;
    total_drumroll++;
    branch_r_count++;
    if (background.has_value()) background->handle_drumroll(PlayerNum(is_2p + 1));
    score += 100;
    if (base_score_list.size() < 5) {
        base_score_list.push_back(ScoreCounterAnimation(player_num, 100, is_2p));
    }
    if (draw_note_buffer.empty()) return;
    if (!other_notes.empty()) {
        int active_drumroll_index = other_notes[0].index;
        auto drumroll_it = std::lower_bound(draw_note_buffer.begin(), draw_note_buffer.end(),
                                            active_drumroll_index,
                                            [](const Note& n, int idx) { return n.index < idx; });
        if (drumroll_it != draw_note_buffer.end() && drumroll_it->index == active_drumroll_index &&
            (drumroll_it->type == NoteType::ROLL_HEAD || drumroll_it->type == NoteType::ROLL_HEAD_L) &&
            drumroll_it->color.has_value()) {
            drumroll_it->color.value() = std::max(0, 255 - (curr_drumroll_count * 10));
        }
    }
}

void Player::check_balloon(double current_ms, DrumType drum_type, const Note& balloon, std::optional<Background>& background) {
    if (drum_type != DrumType::DON) return;
    if (!balloon.count.has_value()) return;
    if (!balloon_counter.has_value()) {
        balloon_counter = BalloonCounter(balloon.count.value(), is_2p);
    }
    if (balloon_idle) {
        balloon_idle = false;
        chara->set_anim(AnimIndex::DON_BALLOON_LOOP);
    }
    last_balloon_hit_ms = current_ms;
    if (background.has_value())
        background->handle_balloon(PlayerNum(is_2p + 1), balloon.count.value() - curr_balloon_count - 1);
    curr_balloon_count++;
    total_drumroll++;
    score += 100;
    base_score_list.push_back(ScoreCounterAnimation(player_num, 100, is_2p));
    if (curr_balloon_count == balloon.count.value()) {
        is_balloon = false;
        balloon_counter->update(current_ms, curr_balloon_count);
        audio.play_sound("balloon_pop", VolumePreset::HITSOUND);
        note_correct(balloon, current_ms);
        curr_balloon_count = 0;
    }
}

void Player::check_kusudama(double current_ms, DrumType drum_type, const Note& balloon, std::optional<Background>& background) {
    if (drum_type != DrumType::DON) return;
    if (!balloon.count.has_value()) return;

    Player* owner = kusudama_owner();
    if (!owner->kusudama_counter.has_value()) {
        owner->kusudama_counter = KusudamaCounter(balloon.count.value());
        owner->kusudama_shared_hits = 0;
    }
    if (background.has_value()) background->handle_kusudama(PlayerNum(is_2p + 1));
    total_drumroll++;
    score += 100;
    base_score_list.push_back(ScoreCounterAnimation(player_num, 100, is_2p));
    owner->kusudama_shared_hits++;
    owner->kusudama_counter->update(current_ms, owner->kusudama_shared_hits);

    if (owner->kusudama_shared_hits == balloon.count.value()) {
        audio.play_sound("kusudama_pop", VolumePreset::HITSOUND);

        is_balloon = false;
        note_correct(balloon, current_ms);

        if (kusudama_partner && kusudama_partner != this && kusudama_partner->is_balloon &&
            !kusudama_partner->other_notes.empty() &&
            kusudama_partner->other_notes.front().type == NoteType::KUSUDAMA) {
            Note partner_note = kusudama_partner->other_notes.front();
            kusudama_partner->is_balloon = false;
            kusudama_partner->note_correct(partner_note, current_ms);
        }

        owner->kusudama_shared_hits = 0;
    }
}

void Player::check_note(double ms_from_start, DrumType drum_type, double current_ms, std::optional<Background>& background) {
    if (don_notes.empty() && kat_notes.empty() && other_notes.empty()) return;

    auto record_hit_offset = [this, ms_from_start](const Note& note) {
        if (modifiers.auto_play) return;

        const double offset_ms = ms_from_start - note.hit_ms;
        hit_offset_sum_ms += offset_ms;
        hit_offset_count++;
    };

    float good_window_ms;
    float ok_window_ms;
    float bad_window_ms;
    if (difficulty <= (int)Difficulty::NORMAL) {
        good_window_ms = Timing::GOOD_EASY;
        ok_window_ms = Timing::OK_EASY;
        bad_window_ms = Timing::BAD_EASY;
    } else {
        good_window_ms = Timing::GOOD;
        ok_window_ms = Timing::OK;
        bad_window_ms = Timing::BAD;
    }
    if (score_method == ScoreMethod::GEN3) {
        base_score = score_init;
        if (9 < combo && combo < 30) {
            base_score = std::floor(score_init + 1 * score_diff);
        } else if (29 < combo && combo < 50) {
            base_score = std::floor(score_init + 2 * score_diff);
        } else if (49 < combo && combo < 100) {
            base_score = std::floor(score_init + 4 * score_diff);
        } else if (99 < combo) {
            base_score = std::floor(score_init + 8 * score_diff);
        }
    }

    Note curr_note;
    if (is_drumroll && !other_notes.empty()) {
        check_drumroll(current_ms, drum_type, background);
        return;
    } else if (is_balloon && !other_notes.empty()) {
        curr_note = other_notes.front();
        if (curr_note.type == NoteType::BALLOON_HEAD) {
            check_balloon(current_ms, drum_type, curr_note, background);
        }
        if (curr_note.type == NoteType::KUSUDAMA) {
            check_kusudama(current_ms, drum_type, curr_note, background);
        }
        return;
    }

    auto& lane = (drum_type == DrumType::DON) ? don_notes : kat_notes;
    if (lane.empty()) return;
    curr_note = lane.front();
    size_t lane_pos = 0;
    if (!modifiers.auto_play && lane.size() > 1 && ms_from_start > curr_note.hit_ms + ok_window_ms) {
        const Note& next = lane[1];
        auto blocked_by = [&](const std::deque<Note>& notes) {
            auto it = std::find_if(notes.begin(), notes.end(), [&](const Note& n) { return n.index > curr_note.index; });
            return it != notes.end() && it->index < next.index;
        };
        const auto& other_lane = (drum_type == DrumType::DON) ? kat_notes : don_notes;
        if (!blocked_by(other_lane) && !blocked_by(other_notes) && ms_from_start > next.hit_ms - ok_window_ms) {
            combo = 0;
            bad_count++;
            on_miss();
            note_judgments[curr_note.index] = Judgments::BAD;
            if (dan_gauge) dan_gauge->add_bad();
            else if (gauge.has_value()) gauge->add_bad();
            if (background.has_value()) background->handle_bad(PlayerNum(1 + is_2p));
            branch_note_count++;
            lane.pop_front();
            curr_note = lane.front();
        }
    }

    {
        if (ms_from_start > (curr_note.hit_ms + bad_window_ms)) return;

        bool big = curr_note.type == NoteType::DON_L || curr_note.type == NoteType::KAT_L;
        if ((curr_note.hit_ms - good_window_ms <= ms_from_start) && (ms_from_start <= curr_note.hit_ms + good_window_ms)) {
            record_hit_offset(curr_note);
            if (draw_judge_list.size() < 7) {
                draw_judge_list.push_back(Judgment(Judgments::GOOD, big));
            }
            lane_hit_effect = LaneHitEffect(drum_type, Judgments::GOOD);
            note_judgments[curr_note.index] = Judgments::GOOD;
            good_count++;
            score += base_score;
            if (base_score_list.size() < 5) {
                base_score_list.push_back(ScoreCounterAnimation(player_num, base_score, is_2p));
            }
            if (lane_pos != 0) lane.erase(lane.begin() + lane_pos);
            note_correct(curr_note, current_ms);
            if (dan_gauge) dan_gauge->add_good();
            else if (gauge.has_value()) gauge->add_good();
            branch_p_count++;
            branch_note_count++;
            if (background.has_value()) background->handle_good(PlayerNum(1 + is_2p));

        } else if ((curr_note.hit_ms - ok_window_ms) <= ms_from_start && ms_from_start <= (curr_note.hit_ms + ok_window_ms)) {
            record_hit_offset(curr_note);
            draw_judge_list.push_back(Judgment(Judgments::OK, big));
            lane_hit_effect = LaneHitEffect(drum_type, Judgments::OK);
            note_judgments[curr_note.index] = Judgments::OK;
            ok_count++;
            score += 10 * std::floor(base_score / 2 / 10);
            if (base_score_list.size() < 5) {
                base_score_list.push_back(ScoreCounterAnimation(player_num, 10 * std::floor(base_score / 2 / 10), is_2p));
            }
            if (lane_pos != 0) lane.erase(lane.begin() + lane_pos);
            note_correct(curr_note, current_ms);
            if (dan_gauge) dan_gauge->add_ok();
            else if (gauge.has_value()) gauge->add_ok();
            branch_p_count += 0.5;
            branch_note_count++;
            if (background.has_value()) background->handle_ok(PlayerNum(1 + is_2p));

        } else if ((curr_note.hit_ms - bad_window_ms) <= ms_from_start && ms_from_start <= (curr_note.hit_ms + bad_window_ms)) {
            draw_judge_list.push_back(Judgment(Judgments::BAD, big));
            bad_count++;
            on_miss();
            combo = 0;
            branch_note_count++;
            // Same note as the GOOD/OK branches: curr_note may be lane[1] (stale head).
            const Note note = curr_note;
            record_hit_offset(note);
            lane.erase(lane.begin() + lane_pos);
            note_judgments[note.index] = Judgments::BAD;
            auto it = std::lower_bound(draw_note_buffer.begin(), draw_note_buffer.end(),
                                       note.index, [](const Note& n, int idx) { return n.index < idx; });
            if (it != draw_note_buffer.end() && *it == note) draw_note_buffer.erase(it);
            if (dan_gauge) dan_gauge->add_bad();
            else if (gauge.has_value()) gauge->add_bad();
            if (background.has_value()) background->handle_bad(PlayerNum(1 + is_2p));
        }
    }
}

void Player::drumroll_counter_manager(double current_ms) {
    if (is_drumroll && curr_drumroll_count > 0 && drumroll_counter == std::nullopt) {
        drumroll_counter = DrumrollCounter();
    }

    if (drumroll_counter.has_value()) {
        if (drumroll_counter->is_finished() && !is_drumroll) {
            drumroll_counter.reset();
        } else if (is_drumroll) {
            drumroll_counter->update(current_ms, curr_drumroll_count);
        } else {
            drumroll_counter->update_animations(current_ms);
        }
    }
}

void Player::balloon_counter_manager(double current_ms) {
    if (!is_balloon && balloon_idle && !balloon_counter.has_value()) {   // never hit
        balloon_idle = false;
        chara->set_anim(rest_anim());
    }
    if (!is_balloon && balloon_counter.has_value() && !balloon_counter->has_popped()) {
        balloon_counter.reset();
        chara->set_anim(rest_anim());
        chara->set_anim(AnimIndex::DON_BALLOON_FAILURE);
    }
    if (balloon_counter.has_value()) {
        balloon_counter->update(current_ms, curr_balloon_count);
        if (balloon_counter->is_finished()) {
            if (score_method == ScoreMethod::GEN3) {
                score += 5000;
                base_score_list.push_back(ScoreCounterAnimation(player_num, 5000, is_2p));
            }
            balloon_counter.reset();
            chara->set_anim(rest_anim());
            chara->set_anim(AnimIndex::DON_BALLOON_SUCCESS);
        }
    }
}

void Player::kusudama_counter_manager(double current_ms) {
    if (!is_balloon && kusudama_counter.has_value() && !kusudama_counter->has_popped()) {
        kusudama_counter.reset();
    }
    if (kusudama_counter.has_value()) {
        kusudama_counter->update(current_ms, kusudama_shared_hits);
        if (kusudama_counter->is_finished()) {
            kusudama_counter.reset();
        }
    }
}

void Player::cut_to_end(double now, int prev_good, int prev_ok, int prev_bad) {
    draw_note_list.clear();
    draw_note_buffer.clear();
    don_notes.clear();
    kat_notes.clear();
    other_notes.clear();
    barlines.clear();
    branch_m.clear();
    branch_e.clear();
    branch_n.clear();
    timeline.clear();
    is_drumroll = false;
    is_balloon = false;
    drumroll_counter.reset();
    balloon_counter.reset();
    kusudama_counter.reset();
    end_time = now - 1000.0;

    skipped_run = true;
    int song_good = good_count - prev_good;
    int song_ok   = ok_count   - prev_ok;
    bad_count = prev_bad + std::max(0, judgeable_note_count - song_good - song_ok);
}

void Player::spawn_hit_effects(DrumType drum_type, Side side) {
    lane_hit_effect = LaneHitEffect(drum_type, Judgments::BAD); //judgment parameter workaround
    if (draw_drum_hit_list.size() < 4) {
        draw_drum_hit_list.push_back(std::make_unique<DrumHitEffect>(drum_type, side));
    }
}

void Player::handle_input(double ms_from_start, double current_ms, std::optional<Background>& background) {
    if (modifiers.auto_play) return;

    if (replay_active) {
        while (replay_cursor < replay_log.size() && replay_log[replay_cursor].first <= ms_from_start) {
            auto [log_ms, log_type] = replay_log[replay_cursor];
            DrumType drum_type = (log_type == InputLogType::DON_L || log_type == InputLogType::DON_R)
                ? DrumType::DON : DrumType::KAT;
            Side side = (log_type == InputLogType::DON_L || log_type == InputLogType::KAT_L)
                ? Side::LEFT : Side::RIGHT;
            spawn_hit_effects(drum_type, side);
            audio.play_sound(drum_type == DrumType::DON ? don_hitsound : kat_hitsound, VolumePreset::HITSOUND);
            check_note(log_ms, drum_type, current_ms, background);
            ++replay_cursor;
        }
        return;
    }

    struct InputCheck {
        bool (*check_func)(PlayerNum, float*);
        DrumType drum_type;
        Side side;
        const std::string* sound;
    };

    const InputCheck input_checks[] = {
        InputCheck{is_l_don_pressed, DrumType::DON, Side::LEFT, &don_hitsound},
        InputCheck{is_r_don_pressed, DrumType::DON, Side::RIGHT, &don_hitsound},
        InputCheck{is_l_kat_pressed, DrumType::KAT, Side::LEFT, &kat_hitsound},
        InputCheck{is_r_kat_pressed, DrumType::KAT, Side::RIGHT, &kat_hitsound}
    };

    for (const auto& input : input_checks) {

        float strength = 1.0f;
        while (input.check_func(player_num, &strength)) {
            spawn_hit_effects(input.drum_type, input.side);
            audio.play_sound(*input.sound, VolumePreset::HITSOUND, strength);
            InputLogType log_type;
            if (input.drum_type == DrumType::DON) {
                log_type = input.side == Side::LEFT ? InputLogType::DON_L : InputLogType::DON_R;
            } else {
                log_type = input.side == Side::LEFT ? InputLogType::KAT_L : InputLogType::KAT_R;
            }
            input_log.insert({ms_from_start, log_type});
            check_note(ms_from_start, input.drum_type, current_ms, background);
        }
    }
}

void Player::draw_bar(double current_ms, float y, const Note& bar) {
    if (!bar.display) return;
    float x_position = get_position_x(bar, current_ms) + judge_x;
    float y_position = get_position_y(bar, current_ms) + judge_y + y;
    float angle;
    if (y_position != 0) {
        angle = std::atan2(bar.scroll_y, bar.scroll_x) * 180.0 / PI;
    } else {
        angle = 0;
    }
    tex.draw_texture(t_notes_0, {.frame=bar.is_branch_start, .x=x_position+tex.skin_config[SC::MOJI_DRUMROLL].x - (t_notes_9->width/2.0f), .y=y_position+tex.skin_config[SC::MOJI_DRUMROLL].y, .rotation=angle});
}

void Player::draw_drumroll(double current_ms, float y, const Note& head, int current_eighth, bool moji_pass) {
    if (head.sudden_appear_ms.has_value() && head.sudden_moving_ms.has_value()) {
        double appear_ms = head.hit_ms - head.sudden_appear_ms.value();
        double moving_start_ms = head.hit_ms - head.sudden_moving_ms.value();
        if (current_ms < appear_ms) return;
        if (current_ms < moving_start_ms) {
            current_ms = moving_start_ms;
        }
    }
    float start_position = get_position_x(head, current_ms);
    auto it = std::lower_bound(draw_note_buffer.begin(), draw_note_buffer.end(),
                               head.index + 1, [](const Note& n, int idx) { return n.index < idx; });
    while (it != draw_note_buffer.end() && it->type != NoteType::TAIL) ++it;

    if (it == draw_note_buffer.end()) return;  // tail not loaded yet
    auto& tail = *it;
    bool is_big = head.type == NoteType::ROLL_HEAD_L;
    float end_position = get_position_x(tail, current_ms);
    float length = end_position - start_position;
    ray::Color color = ray::Color{255, (unsigned char)head.color.value(), (unsigned char)head.color.value(), 255};
    float y_pos = y + tex.skin_config[SC::NOTES].y + get_position_y(head, current_ms) + judge_y;
    start_position += judge_x;
    end_position += judge_x;
    float moji_y = y + tex.skin_config[SC::MOJI].y;
    if (moji_pass) {
        tex.draw_texture(t_moji_drumroll_mid, {.x=start_position, .y=moji_y+judge_y, .x2=length});
        tex.draw_texture(t_moji, {.frame=head.moji, .x=start_position - (t_moji->width/2.0f), .y=moji_y+judge_y});
        tex.draw_texture(t_moji, {.frame=tail.moji, .x=end_position - (t_moji->width/2.0f), .y=moji_y+judge_y});
        return;
    }

    if (head.display) {
        tex.draw_texture(t_notes_8, {.color=color, .frame=is_big, .x=start_position, .y=y_pos, .x2=length+tex.skin_config[SC::DRUMROLL_WIDTH_OFFSET].width});
        if (is_big) {
            tex.draw_texture(t_drumroll_big_tail, {.color=color, .x=end_position, .y=y_pos});
        } else {
            tex.draw_texture(t_drumroll_tail, {.color=color, .x=end_position, .y=y_pos});
        }
        tex.draw_texture(note_tex_ids[(int)head.type], {.color=color, .frame=current_eighth % 2, .x=start_position - t_notes_9->width/2.0f, .y=y_pos+judge_y});
    }
}

void Player::draw_balloon(double current_ms, float y, const Note& head, int current_eighth, bool moji_pass) {
    float offset = tex.skin_config[SC::BALLOON_OFFSET].x;
    if (head.sudden_appear_ms.has_value() && head.sudden_moving_ms.has_value()) {
        double appear_ms = head.hit_ms - head.sudden_appear_ms.value();
        double moving_start_ms = head.hit_ms - head.sudden_moving_ms.value();
        if (current_ms < appear_ms) return;
        if (current_ms < moving_start_ms) {
            current_ms = moving_start_ms;
        }
    }
    float start_position = get_position_x(head, current_ms);
    auto it = std::lower_bound(draw_note_buffer.begin(), draw_note_buffer.end(),
                               head.index + 1, [](const Note& n, int idx) { return n.index < idx; });
    while (it != draw_note_buffer.end() && it->type != NoteType::TAIL) ++it;

    if (it == draw_note_buffer.end()) return;  // tail not loaded yet
    auto& tail = *it;
    float end_position = get_position_x(tail, current_ms);
    float pause_position = JudgePos::X + judge_x;
    float y_pos = y + tex.skin_config[SC::NOTES].y + get_position_y(head, current_ms) + judge_y;
    float moji_y = y + tex.skin_config[SC::MOJI].y + get_position_y(head, current_ms) + judge_y;
    start_position += judge_x;
    end_position += judge_x;
    float position;
    if (current_ms >= tail.hit_ms) {
        position = end_position;
    } else if (current_ms >= head.hit_ms) {
        position = pause_position;
    } else {
        position = start_position;
    }
    if (moji_pass) {
        tex.draw_texture(t_moji, {.frame=head.moji, .x=position - (t_moji->width/2.0f), .y=moji_y});
        return;
    }
    if (head.display) {
        tex.draw_texture(note_tex_ids[(int)head.type], {.frame=current_eighth % 2, .x=position-offset - t_notes_9->width/2.0f, .y=y_pos});
        tex.draw_texture(t_notes_10, {.frame=current_eighth % 2, .x=position-offset+t_notes_10->width - t_notes_9->width/2.0f, .y=y_pos});
    }
}

void Player::draw_notes(double current_ms, float y) {
    current_ms += visual_offset;
    for (auto it = barlines.rbegin(); it != barlines.rend(); ++it) {
        draw_bar(current_ms, y, *it);
    }

    if (draw_note_buffer.empty()) return;

    double eighth_in_ms = (bpm == 0) ? 0 : (60000.0 * 4.0 / bpm) / 8.0;
    int current_eighth = 0;
    if (combo >= 50 && eighth_in_ms != 0) {
        current_eighth = static_cast<int>(current_ms / eighth_in_ms);
    }

    auto skip_note = [&](const Note& note) {
        // only the balloon being hit: after a pop the counter plays on while other_notes[0] is already the next balloon
        if (is_balloon && balloon_counter.has_value() && note.type == NoteType::BALLOON_HEAD && !other_notes.empty() && note.index == other_notes[0].index) {
            return true;
        }
        if (kusudama_owner()->kusudama_counter.has_value() && note.type == NoteType::KUSUDAMA && !other_notes.empty() && note.index == other_notes[0].index) {
            return true;
        }
        return note.type == NoteType::TAIL;
    };

    // nullopt = note not visible yet (sudden command); otherwise screen position
    auto note_position = [&](const Note& note) -> std::optional<std::pair<float, float>> {
        float x_position, y_position;
        if (note.sudden_appear_ms.has_value() && note.sudden_moving_ms.has_value()) {
            double appear_ms = note.hit_ms - note.sudden_appear_ms.value();
            double moving_start_ms = note.hit_ms - note.sudden_moving_ms.value();

            if (current_ms < appear_ms) {
                return std::nullopt;
            }

            double effective_ms = (current_ms < moving_start_ms) ? moving_start_ms : current_ms;

            x_position = get_position_x(note, effective_ms);
            y_position = get_position_y(note, current_ms);
        } else {
            x_position = get_position_x(note, current_ms);
            y_position = get_position_y(note, current_ms);
        }
        return std::make_pair(x_position + judge_x, y_position + judge_y + y);
    };

    for (auto it = draw_note_buffer.rbegin(); it != draw_note_buffer.rend(); ++it) {
        auto& note = *it;
        if (skip_note(note)) continue;
        auto pos = note_position(note);
        if (!pos) continue;
        if (note.color.has_value()) {
            draw_drumroll(current_ms, y, note, current_eighth, false);
        } else if (note.type == NoteType::BALLOON_HEAD) {
            draw_balloon(current_ms, y, note, current_eighth, false);
        } else if (note.display) {
            tex.draw_texture(note_tex_ids[(int)note.type], {.frame=current_eighth % 2, .center=true, .x=pos->first - (t_notes_9->width/2.0f), .y=pos->second+tex.skin_config[SC::NOTES].y});
        }
    }

    for (auto it = draw_note_buffer.rbegin(); it != draw_note_buffer.rend(); ++it) {
        auto& note = *it;
        if (skip_note(note)) continue;
        auto pos = note_position(note);
        if (!pos) continue;

        if (note.color.has_value()) {
            draw_drumroll(current_ms, y, note, current_eighth, true);
        } else if (note.type == NoteType::BALLOON_HEAD) {
            draw_balloon(current_ms, y, note, current_eighth, true);
        } else {
            tex.draw_texture(t_moji, {.frame=note.moji, .x=pos->first - (t_moji->width/2.0f), .y=tex.skin_config[SC::MOJI].y + pos->second});
        }
    }
}

void Player::draw_song_timer(double current_ms, float y) {
    float progress = end_time > 0 ? (float)(current_ms / end_time) : 0.0f;
    float width = tex.skin_config[SC::SONG_TIMER].width * std::max(std::min(progress, 1.0f), 0.0f);
    ray::DrawRectangle(tex.skin_config[SC::SONG_TIMER].x, y + tex.skin_config[SC::SONG_TIMER].y, width, tex.skin_config[SC::SONG_TIMER].height, ray::Color(0, 255, 158, 255));
    tex.draw_texture(t_timer, {.y=y});
}

void Player::draw_modifiers(float y) {
    auto icon_y = [&](TextureObject* id) {
        if (!is_2p) return y;
        float cover_h = (float)t_lane_score_cover->y2[0];
        float json_y  = (float)id->y[0];
        float icon_h  = (float)id->y2[0];
        return y + tex.skin_config[SC::SCORE_COUNTER_2P_Y_OFFSET].y
                 + cover_h - icon_h - 2.0f * json_y;
    };

    // t_badges/t_mod_shinuchi are resolved once in init_player_textures() (modifiers
    // and score_method are fixed for this Player's lifetime).
    const std::vector<TextureObject*>& badges = t_badges;

    const SkinInfo* grid = tex.skin_entry("mod_badge_grid");
    if (grid && grid->width > 0) {
        // Sequential slots on the skin's grid (columns in font_size, default 3).
        const int cols = grid->font_size > 0 ? grid->font_size : 3;
        int slot = 0;
        for (TextureObject* id : badges) {
            const float gx = grid->x + (slot % cols) * grid->width;
            const float gy = grid->y + (slot / cols) * grid->height;
            float by = y + gy;
            if (is_2p) {
                float cover_h = (float)t_lane_score_cover->y2[0];
                float icon_h  = (float)id->y2[0];
                by = y + tex.skin_config[SC::SCORE_COUNTER_2P_Y_OFFSET].y + cover_h - icon_h - 2.0f * gy;
            }
            tex.draw_texture(id, {.x = gx - (float)id->x[0], .y = by - (float)id->y[0]});
            slot++;
        }
        if (score_method == ScoreMethod::SHINUCHI && t_mod_shinuchi)
            tex.draw_texture(t_mod_shinuchi, {.y = icon_y(t_mod_shinuchi)});
        return;
    }

    if (score_method == ScoreMethod::SHINUCHI && t_mod_shinuchi) {
        tex.draw_texture(t_mod_shinuchi, {.y=icon_y(t_mod_shinuchi)});
    }
    for (TextureObject* id : badges) {
        tex.draw_texture(id, {.y = icon_y(id)});
    }
}

void Player::draw_lane_cover(float y) {
    if (!is_balloon || (balloon_idle && !balloon_counter.has_value())) {
        if (is_2p) {
            chara->draw(tex.skin_config[SC::GAME_CHARA_P2].x, y + tex.skin_config[SC::GAME_CHARA_P2].y, 1.0f);
        } else {
            chara->draw(tex.skin_config[SC::GAME_CHARA_P1].x, y + tex.skin_config[SC::GAME_CHARA_P1].y, 1.0f);
        }
    }
    tex.draw_texture(lane_cover_tex_id, {.y=y});
    if (is_dan) tex.draw_texture(t_dan_lane_cover, {.y=y});
}


void Player::draw_overlays(float y, const ray::Shader& mask_shader) {
    tex.draw_texture(t_drum, {.y=y});
    if (ending_anim.has_value()) {
        if (ending_background && ending_background->wants_draw_ending())
            ending_background->draw_ending(player_num);
        else
            std::visit([](auto& anim) { anim.draw(); }, ending_anim.value());
    }

    for (auto& anim : draw_drum_hit_list) {
        anim->draw(y);
    }
    for (NoteArc& anim : draw_arc_list) {
        anim.draw(y, mask_shader);
    }
    for (GaugeHitEffect& anim : gauge_hit_effect) {
        anim.draw(y);
    }
    score_counter.draw(y);
    for (ScoreCounterAnimation& anim : base_score_list) {
        anim.draw(y);
    }

    combo_display.draw(y);
    if (combo_announce.has_value()) {
        combo_announce->draw(y + (tex.skin_config[SC::COMBO_ANNOUNCE_P2_Y_OFFSET].y * is_2p));
    }
    tex.draw_texture(lane_icon_tex_id, {.y=y, .index=is_2p});
    int frame = is_dan ? 6 : difficulty;
    int index = is_dan ? 0 : is_2p;
    tex.draw_texture(t_lane_difficulty, {.frame=frame, .y=y, .index=index});
    draw_modifiers(y);
    if (judge_counter.has_value()) {
        judge_counter->draw();
    }

    if (modifiers.auto_play) {
        tex.draw_texture(t_auto_icon, {.y=y, .index=is_2p});
    } else {
        if (is_2p) {
            nameplate.draw(tex.skin_config[SC::GAME_NAMEPLATE_2P].x, y + tex.skin_config[SC::GAME_NAMEPLATE_2P].y);
        } else {
            nameplate.draw(tex.skin_config[SC::GAME_NAMEPLATE_1P].x, y + tex.skin_config[SC::GAME_NAMEPLATE_1P].y);
        }
    }
    if (is_balloon) {
        float rig_2p_y = 0.0f;
        if (is_2p && balloon_counter.has_value()) {
            if (const SkinInfo* p2 = tex.skin_entry("balloon_counter_2p_offset"))
                rig_2p_y = p2->y;
        }
        if (!balloon_idle || balloon_counter.has_value()) {
            chara->draw(tex.skin_config[SC::GAME_CHARA_BALLOON].x, y + tex.skin_config[SC::GAME_CHARA_BALLOON].y + rig_2p_y, 1.0f);
        }
    }

    if (drumroll_counter.has_value()) {
        drumroll_counter->draw(y + (tex.skin_config[SC::COMBO_ANNOUNCE_P2_Y_OFFSET].y * is_2p));
    }
    if (balloon_counter.has_value()) {
        balloon_counter->draw(y);
    }
    if (kusudama_counter.has_value()) {
        kusudama_counter->draw();
    }
    // Practice mode draws the lyric itself, after the large drums, so it is not hidden.
    if (!practice_lyric) draw_lyric(y);
}

void Player::draw_lyric(float y) {
    (void)y;
    if (!current_lyric.has_value()) return;
    const SkinInfo* lyric_cfg = tex.skin_entry("lyric");
    float lyric_y = (lyric_cfg && lyric_cfg->y > 0) ? lyric_cfg->y
                                                    : static_cast<float>(tex.screen_height - (int)(current_lyric->height*1.5));
    current_lyric->draw({.x=(int)(tex.screen_width/2) - current_lyric->width/2, .y=lyric_y});
}

void Player::seek_to(double resume_time) {
    don_notes.clear();
    kat_notes.clear();
    other_notes.clear();
    draw_note_list.clear();
    draw_note_buffer.clear();
    barlines.clear();
    timeline_buffer.clear();
    last_jpos_key = {-1e300, -1};
    draw_judge_list.clear();
    draw_drum_hit_list.clear();
    draw_arc_list.clear();
    gauge_hit_effect.clear();
    lane_hit_effect.reset();
    gogo_time.reset();
    fireworks.reset();
    drumroll_counter.reset();
    balloon_counter.reset();
    kusudama_counter.reset();
    combo_announce.reset();
    is_drumroll = false;
    is_balloon = false;
    curr_drumroll_count = 0;
    curr_balloon_count = 0;
    kusudama_shared_hits = 0;

    reset_chart();
    resume_filter_ms = resume_time;

    while (!timeline.empty() && timeline.front().start_time <= resume_time) {
        TimelineObject entry = timeline.front();
        timeline.pop_front();
        timeline_buffer.push_back(entry);
        int idx = (int)timeline_buffer.size() - 1;
        const size_t before = timeline_buffer.size();
        handle_bpmchange(resume_time, entry, idx);
        if (timeline_buffer.size() != before) continue;
        handle_judgeposition(resume_time, entry, idx);
        if (timeline_buffer.size() != before) continue;
        handle_gogotime(resume_time, entry, idx);
        if (timeline_buffer.size() != before) continue;
        handle_branch_param(resume_time, entry, idx);
        if (timeline_buffer.size() != before) continue;
        handle_lyric(resume_time, entry, idx);
        if (timeline_buffer.size() != before) continue;
        handle_camera(resume_time, entry, idx);
        if (timeline_buffer.size() != before) continue;
        handle_section(resume_time, entry, idx);
    }

    const double boundary_eps = 1.0;
    auto filter = [resume_time, boundary_eps](std::deque<Note>& q) {
        while (!q.empty() && q.front().hit_ms < resume_time - boundary_eps) q.pop_front();
    };
    filter(don_notes);
    filter(kat_notes);
    filter(other_notes);

    draw_note_list.erase(
        std::remove_if(draw_note_list.begin(), draw_note_list.end(),
            [resume_time, boundary_eps](const Note& n) { return n.hit_ms < resume_time - boundary_eps; }),
        draw_note_list.end());
}