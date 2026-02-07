#include "player.h"
#include "kusudama_counter.h"

Player::Player(std::optional<TJAParser>& parser_ref, PlayerNum player_num_param, int difficulty_param,
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
    , combo_display(combo, 0, is_2p)
    , score_counter(is_2p, 0)
{
    reset_chart();
    don_hitsound = "hitsound_don_" + std::to_string((int)player_num) + "p";
    kat_hitsound = "hitsound_kat_" + std::to_string((int)player_num) + "p";

    if (parser.has_value() && !parser->metadata.course_data.empty()) {
        if (parser->metadata.course_data[difficulty].is_branching) {
        branch_indicator = BranchIndicator(is_2p);
        }
    }
    //self.ending_anim: Optional[FailAnimation | ClearAnimation | FCAnimation] = None
    //plate_info = global_data.config[f"nameplate_{self.is_2p+1}p"]
    //self.nameplate = Nameplate(plate_info["name"], plate_info["title"], global_data.player_num, plate_info["dan"], plate_info["gold"], plate_info["rainbow"], plate_info["title_bg"])
    //self.chara = Chara2D(player_num - 1, self.bpm)
    if (global_data.config->general.judge_counter) {
        judge_counter = JudgeCounter();
    }
}

void Player::handle_timeline(double ms_from_start) {
    if (timeline.empty() || timeline_index >= timeline.size()) return;
    TimelineObject timeline_object = timeline[timeline_index];

    //self.handle_scroll_type_commands(current_ms, timeline_object)
    //self.handle_bpmchange(current_ms, timeline_object)
    //self.handle_judgeposition(current_ms, timeline_object)
    //self.handle_gogotime(current_ms, timeline_object)
}

void Player::autoplay_manager(double ms_from_start, double current_ms) {// Background background) {
    if (!modifiers.auto_play) return;

    double subdivision_in_ms;
    DrumType hit_type;
    if (is_drumroll || is_balloon) {
        if (bpm == 0) {
            subdivision_in_ms = 0;
        } else {
            subdivision_in_ms = ms_from_start / ((60000 * 4 / bpm) / 24);
        }
        if (subdivision_in_ms > last_subdivision) {
            last_subdivision = subdivision_in_ms;
            hit_type = DrumType::DON;
            autoplay_hit_side = autoplay_hit_side == Side::LEFT ? Side::RIGHT : Side::LEFT;
            spawn_hit_effects(hit_type, autoplay_hit_side);
            audio->play_sound("hitsound_don_" + std::to_string((int)player_num) + "p", "hitsound");
            check_note(ms_from_start, hit_type, current_ms);//, background);
    } else {
        while (!don_notes.empty() && ms_from_start >= don_notes[0].hit_ms) {
            hit_type = DrumType::DON;
            autoplay_hit_side = autoplay_hit_side == Side::LEFT ? Side::RIGHT : Side::LEFT;
            spawn_hit_effects(hit_type, autoplay_hit_side);
            audio->play_sound("hitsound_don_" + std::to_string((int)player_num) + "p", "hitsound");
            check_note(ms_from_start, hit_type, current_ms);//, background);
        }

        while (!kat_notes.empty() && ms_from_start >= kat_notes[0].hit_ms) {
            hit_type = DrumType::KAT;
            autoplay_hit_side = autoplay_hit_side == Side::LEFT ? Side::RIGHT : Side::LEFT;
            spawn_hit_effects(hit_type, autoplay_hit_side);
            audio->play_sound("hitsound_don_" + std::to_string((int)player_num) + "p", "hitsound");
            check_note(ms_from_start, hit_type, current_ms);//, background);
        }
        }
    }
}

void Player::merge_branch_section(NoteList branch_section, double current_ms) {
    draw_note_list.insert(draw_note_list.end(),
                          branch_section.draw_notes.begin(),
                          branch_section.draw_notes.end());

    draw_bar_list.insert(draw_bar_list.end(),
                         branch_section.bars.begin(),
                         branch_section.bars.end());

    std::sort(draw_note_list.begin(), draw_note_list.end(),
              [](const Note& a, const Note& b) { return a.load_ms < b.load_ms; });

    std::sort(this->draw_bar_list.begin(), this->draw_bar_list.end(),
              [](const Note& a, const Note& b) { return a.load_ms < b.load_ms; });

    std::vector<Note> total_don;
    std::vector<Note> total_kat;
    std::vector<Note> total_other;

    for (const auto& note : branch_section.play_notes) {
        if (note.type == (int)NoteType::DON || note.type == (int)NoteType::DON_L) {
            total_don.push_back(note);
        } else if (note.type == (int)NoteType::KAT || note.type == (int)NoteType::KAT_L) {
            total_kat.push_back(note);
        } else {
            total_other.push_back(note);
        }
    }

    std::vector<Note> all_don(don_notes.begin(), don_notes.end());
    std::vector<Note> all_kat(kat_notes.begin(), kat_notes.end());
    std::vector<Note> all_other(other_notes.begin(), other_notes.end());

    all_don.insert(all_don.end(), total_don.begin(), total_don.end());
    all_kat.insert(all_kat.end(), total_kat.begin(), total_kat.end());
    all_other.insert(all_other.end(), total_other.begin(), total_other.end());

    std::sort(all_don.begin(), all_don.end());
    std::sort(all_kat.begin(), all_kat.end());
    std::sort(all_other.begin(), all_other.end());

    don_notes.clear();
    kat_notes.clear();
    other_notes.clear();

    for (const auto& note : all_don) {
        if (note.hit_ms > current_ms) {
            don_notes.push_back(note);
        }
    }

    for (const auto& note : all_kat) {
        if (note.hit_ms > current_ms) {
            kat_notes.push_back(note);
        }
    }

    for (const auto& note : all_other) {
        if (note.hit_ms > current_ms) {
            other_notes.push_back(note);
        }
    }
}

void Player::evaluate_branch(double current_ms) {
    float e_req = std::get<0>(curr_branch_reqs);
    float m_req = std::get<1>(curr_branch_reqs);
    double end_time = std::get<2>(curr_branch_reqs);
    int total_notes = std::get<3>(curr_branch_reqs);
    if (current_ms >= end_time) {
        is_branch = false;
        if (branch_condition == "p") {
            branch_condition_count = std::max(std::min((branch_condition_count/total_notes)*100, 100), 0);
        }
        if (branch_indicator.has_value()) {
            //logger.info(f"Branch set to {self.branch_indicator.difficulty} based on conditions {self.branch_condition_count}, {e_req, m_req}")
        }
        if (branch_condition_count >= e_req && branch_condition_count < m_req && e_req >= 0) {
            if (!branch_e.empty()) {
                merge_branch_section(branch_e.front(), current_ms);
                branch_e.erase(branch_e.begin());
                if (branch_indicator.has_value() and branch_indicator->difficulty != BranchDifficulty::EXPERT) {
                    if (branch_indicator->difficulty == BranchDifficulty::MASTER) {
                        branch_indicator->level_down(BranchDifficulty::EXPERT);
                    } else {
                        branch_indicator->level_up(BranchDifficulty::EXPERT);
                    }
                }
            }
            if (!branch_m.empty()) {
                branch_m.erase(branch_m.begin());
            }
            if (!branch_n.empty()) {
                branch_n.erase(branch_n.begin());
            }
        } else if (branch_condition_count >= m_req) {
            if (!branch_m.empty()) {
                merge_branch_section(branch_m.front(), current_ms);
                branch_m.erase(branch_m.begin());
                if (branch_indicator.has_value() and branch_indicator->difficulty != BranchDifficulty::MASTER) {
                    branch_indicator->level_up(BranchDifficulty::MASTER);
                }
            }
            if (!branch_n.empty()) {
                branch_n.erase(branch_n.begin());
            }
            if (!branch_e.empty()) {
                branch_e.erase(branch_e.begin());
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
                branch_m.erase(branch_m.begin());
            }
            if (!branch_e.empty()) {
                branch_e.erase(branch_e.begin());
            }
        }
        branch_condition_count = 0;
    }
}

void Player::update(double ms_from_start, double current_ms) {
    note_manager(ms_from_start);//, background);
    combo_display.update(current_ms, combo);
    if (combo_announce.has_value()) {
        combo_announce->update(current_ms);
    }
    drumroll_counter_manager(current_ms);
    balloon_counter_manager(current_ms);
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
    if (lane_hit_effect.has_value()) {
        lane_hit_effect->update(current_ms);
        if (lane_hit_effect->is_finished()) {
            lane_hit_effect.reset();
        }
    }
    for (auto it = draw_drum_hit_list.begin(); it != draw_drum_hit_list.end(); ) {
        it->update(current_ms);
        if (it->is_finished()) {
            it = draw_drum_hit_list.erase(it);
        } else {
            ++it;
        }
    }
    handle_timeline(ms_from_start);
    if (delay_start.has_value() && delay_end.has_value()) {
        if (ms_from_start >= delay_end.value()) {
            double delay = delay_end.value() - delay_start.value();
            for (auto& note : don_notes) note.load_ms += delay;
            for (auto& note : kat_notes) note.load_ms += delay;
            for (auto& note : other_notes) note.load_ms += delay;
            for (auto& note : current_bars) note.load_ms += delay;
            for (auto& note : draw_bar_list) note.load_ms += delay;
            delay_start.reset();
            delay_end.reset();
        }
    }

    for (auto it = draw_arc_list.begin(); it != draw_arc_list.end(); ) {
        it->update(current_ms);
        if (it->is_finished()) {
            int note_type = it->note_type;
            bool is_big = it->is_big;
            it = draw_arc_list.erase(it);
            gauge_hit_effect.push_back(GaugeHitEffect(note_type, is_big, is_2p));
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
        } else {
            ++it;
        }
    }
    score_counter.update(current_ms, score);
    autoplay_manager(ms_from_start, current_ms);//, background);
    handle_input(ms_from_start, current_ms);//, background);
    //self.nameplate.update(current_ms)
    if (gauge.has_value()) {
        gauge->update(current_ms);
    }
    if (judge_counter.has_value()) {
        judge_counter->update(good_count, ok_count, bad_count, total_drumroll);
    }
    if (branch_indicator.has_value()) {
        branch_indicator->update(current_ms);
    }
    //if self.ending_anim is not None:
        //self.ending_anim.update(current_ms)

    if (is_branch) {
        evaluate_branch(ms_from_start);
    }

    /*if self.gauge is None:
        self.chara.update(current_ms, self.bpm, False, False)
    else:
        self.chara.update(current_ms, self.bpm, self.gauge.is_clear, self.gauge.is_rainbow)
    */
}

void Player::draw(double ms_from_start, ray::Shader& mask_shader) {//dan_transition = None
    tex.draw_texture("lane", "lane_background", {.index = is_2p});
    if (player_num == PlayerNum::AI) tex.draw_texture("lane", "ai_lane_background");
    if (branch_indicator.has_value()) {
        branch_indicator->draw();
    }
    if (gauge.has_value()) {
        gauge->draw();
    }
    if (lane_hit_effect.has_value()) {
        lane_hit_effect->draw();
    }
    tex.draw_texture("lane", "lane_hit_circle", {.x =  judge_x, .y =  judge_y, .index = is_2p});

    if (gogo_time.has_value()) {
        gogo_time->draw(judge_x, judge_y);
    }
    for (Judgment anim : draw_judge_list) {
        anim.draw(judge_x, judge_y);
    }

    draw_bars(ms_from_start);
    draw_notes(ms_from_start);
    /*if (dan_transition.has_value()) {
        dan_transition->draw();
    }*/

    draw_overlays(mask_shader);
}

void Player::get_load_time(Note& note) {
    int note_half_w = tex.textures["notes"]["1"]->width / 2;
    float travel_distance = tex.screen_width - JudgePos::X;
    float base_pixels_per_ms = (note.bpm / 240000 * abs(note.scroll_x) * travel_distance);
    if (base_pixels_per_ms == 0) {
        base_pixels_per_ms = (note.bpm / 240000 * abs(note.scroll_y) * travel_distance);
    }
    float normal_travel_ms = (travel_distance + note_half_w) / base_pixels_per_ms;

    if (!note.sudden_appear_ms.has_value() ||
        !note.sudden_moving_ms.has_value() ||
        note.sudden_appear_ms.value() == std::numeric_limits<float>::infinity()) {
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

void Player::reset_chart() {
    auto [notes, branch_m_temp, branch_e_temp, branch_n_temp] = parser->notes_to_position(difficulty);
    this->branch_m = branch_m_temp;
    this->branch_e = branch_e_temp;
    this->branch_n = branch_n_temp;
    auto [play_notes, draw_notes_temp, draw_bars_temp] = apply_modifiers(notes, modifiers);

    this->draw_note_list = draw_notes_temp;
    this->draw_bar_list = draw_bars_temp;
    for (const Note note : notes.play_notes) {
        if (note.type == (int)NoteType::DON || note.type == (int)NoteType::DON_L) {
            don_notes.push_back(note);
        } else if (note.type == (int)NoteType::KAT || note.type == (int)NoteType::KAT_L) {
            kat_notes.push_back(note);
        } else {
            other_notes.push_back(note);
        }
    }

    int total_notes = std::count_if(notes.play_notes.begin(), notes.play_notes.end(),
        [](const Note& note) {
            return note.type > 0 && note.type < 5;
        });

    for (const auto& branch_section : this->branch_m) {
        total_notes += std::count_if(branch_section.play_notes.begin(), branch_section.play_notes.end(),
            [](const Note& note) {
                return note.type > 0 && note.type < 5;
            });
    }
    base_score = 0;
    score_init = 0;
    score_diff = 0;
    if (score_method == ScoreMethod::SHINUCHI) {
        base_score = calculate_base_score(notes, this->branch_m);
    } else if (score_method == ScoreMethod::GEN3) {
        score_diff = parser->metadata.course_data[difficulty].scorediff;
        if (score_diff <= 0) {
            //logger.warning("Error: No scorediff specified or scorediff less than 0 | Using shinuchi scoring method instead")
            score_diff = 0;
        }

        std::vector<int> score_init_list = parser->metadata.course_data[difficulty].scoreinit;
        if (score_init_list.size() <= 0) {
            //logger.warning("Error: No scoreinit specified or scoreinit less than 0 | Using shinuchi scoring method instead")
            score_init = calculate_base_score(notes, this->branch_m);
            score_diff = 0;
        } else {
            score_init = score_init_list[0];
            //logger.debug(f"debug | score init: {self.score_init} | score diff: {self.score_diff}")
        }
    }

    timeline = notes.timeline;
    timeline_index = 0;
    is_drumroll = false;
    curr_drumroll_count = 0;
    is_balloon = false;
    curr_balloon_count = 0;
    is_branch = false;
    branch_condition_count = 0;
    branch_condition = "";
    balloon_index = 0;
    bpm = 120;
    if (!timeline.empty() && timeline[timeline_index].bpm.has_value()) {
        bpm = timeline[timeline_index].bpm.value();
    }

    Note* last_note = nullptr;

    if (!draw_note_list.empty()) {
        last_note = &draw_note_list[0];
    }
    if (!this->branch_m.empty() && !this->branch_m[0].draw_notes.empty()) {
        last_note = &this->branch_m[0].draw_notes[0];
    }

    for (Note& note : this->draw_note_list) {
        get_load_time(note);

        if (note.type == (int)NoteType::TAIL) {
            note.load_ms = last_note->load_ms;
            last_note->unload_ms = note.unload_ms;
        }

        last_note = &note;
    }

    std::sort(this->draw_note_list.begin(), this->draw_note_list.end(),
        [](const Note& a, const Note& b) {
            return a.load_ms < b.load_ms;
        });

    /*
    for i, o in enumerate(self.timeline):
        if hasattr(o, "bpmchange"):
            hit_ms = o.hit_ms
            bpmchange = o.bpmchange
            for note in chain(self.draw_note_list, self.draw_bar_list):
                if note.hit_ms > hit_ms:
                    note.hit_ms = (note.hit_ms - hit_ms) / bpmchange + hit_ms
            for i2 in range(i + 1, len(self.timeline)):
                o2 = self.timeline[i2]
                if not hasattr(o2, "bpmchange"):
                    continue
                o2.hit_ms = (o2.hit_ms - hit_ms) / bpmchange + hit_ms
        elif hasattr(o, "delay"):
            hit_ms = o.hit_ms
            delay = o.delay
            for note in chain(self.draw_note_list, self.draw_bar_list):
                if note.hit_ms > hit_ms:
                    note.hit_ms += delay
            for i2 in range(i + 1, len(self.timeline)):
                o2 = self.timeline[i2]
                if not hasattr(o2, "delay"):
                    continue
                o2.hit_ms += delay
     */

    end_time = !notes.play_notes.empty() ? notes.play_notes.back().hit_ms : 0;
    int stars;
    if (parser->metadata.course_data.empty()) {
        stars = 10;
    } else {
        stars = parser->metadata.course_data[difficulty].level;
    }
    gauge = Gauge(player_num, difficulty, stars, total_notes, is_2p);

    std::vector<std::reference_wrapper<std::vector<NoteList>>> branches = {
        std::ref(this->branch_m),
        std::ref(this->branch_e),
        std::ref(this->branch_n)
    };

    for (auto& branch_ref : branches) {
        auto& branch = branch_ref.get();

        if (!branch.empty()) {
            for (auto& section : branch) {
                auto [play_notes, draw_notes, bars] = apply_modifiers(section, modifiers);
                section.play_notes = std::move(play_notes);
                section.draw_notes = std::move(draw_notes);
                section.bars = std::move(bars);

                if (!section.draw_notes.empty()) {
                    for (auto& note : section.draw_notes) {
                        get_load_time(note);
                    }
                    std::sort(section.draw_notes.begin(), section.draw_notes.end(),
                        [](const auto& a, const auto& b) {
                            return a.load_ms < b.load_ms;
                        });
                }
                if (!section.bars.empty()) {
                    for (auto& note : section.bars) {
                        get_load_time(note);
                    }
                    std::sort(section.bars.begin(), section.bars.end(),
                        [](const auto& a, const auto& b) {
                            return a.load_ms < b.load_ms;
                        });
                }
                if (!section.play_notes.empty()) {
                    end_time = std::max(end_time, (float)section.play_notes.back().hit_ms);
                }
            }
        }
    }
}

float Player::get_position_x(Note note, double current_ms) {
    if (delay_start.has_value()) {
        current_ms = delay_start.value();
    }
    float speedx = note.bpm / 240000 * note.scroll_x * (tex.screen_width - JudgePos::X);
    return JudgePos::X + (note.hit_ms - current_ms) * speedx;
}

float Player::get_position_y(Note note, double current_ms) {
    if (delay_start.has_value()) {
        current_ms = delay_start.value();
    }
    float speedy = note.bpm / 240000 * note.scroll_y * ((tex.screen_width - JudgePos::X)/tex.screen_width) * tex.screen_width;
    return (note.hit_ms - current_ms) * speedy;
}

void Player::bar_manager(double current_ms) {
    Note* inserted_bar = nullptr;

    if (!draw_bar_list.empty() && current_ms >= draw_bar_list[0].load_ms) {
        Note bar_to_insert = draw_bar_list[0];
        draw_bar_list.erase(draw_bar_list.begin());

        auto insert_pos = std::lower_bound(current_bars.begin(), current_bars.end(), bar_to_insert,
            [](const Note& a, const Note& b) {
                return a.load_ms < b.load_ms;
            });
        auto inserted_it = current_bars.insert(insert_pos, bar_to_insert);
        inserted_bar = &(*inserted_it);
    }

    if (current_bars.empty()) return;

    if (inserted_bar && inserted_bar->branch_params.has_value()) {
        std::string params = inserted_bar->branch_params.value();
        inserted_bar->branch_params.reset();

        std::vector<std::string> parts;
        std::stringstream ss(params);
        std::string part;
        while (std::getline(ss, part, ',')) {
            parts.push_back(part);
        }

        if (parts.size() >= 3) {
            std::string branch_cond = parts[0];
            float e_req = std::stof(parts[1]);
            float m_req = std::stof(parts[2]);

            spdlog::info("branch condition measures started with conditions {}, {}, {}, {}", branch_cond, e_req, m_req, current_bars.back().hit_ms);

            if (!is_branch) {
                is_branch = true;
                branch_condition = branch_cond;

                if (branch_cond == "r") {
                    double end_time = std::numeric_limits<double>::max();
                    if (!branch_m.empty() && !branch_m[0].bars.empty()) {
                        end_time = branch_m[0].bars[0].load_ms;
                    } else if (!branch_e.empty() && !branch_e[0].bars.empty()) {
                        end_time = branch_e[0].bars[0].load_ms;
                    } else if (!branch_n.empty() && !branch_n[0].bars.empty()) {
                        end_time = branch_n[0].bars[0].load_ms;
                    }
                    curr_branch_reqs = std::make_tuple(e_req, m_req, end_time, 1);
                } else if (branch_cond == "p") {
                    double start_time = !current_bars.empty() ? current_bars[0].hit_ms : current_bars.back().hit_ms;
                    double branch_start_time = std::numeric_limits<double>::max();
                    if (!branch_m.empty() && !branch_m[0].bars.empty()) {
                        branch_start_time = branch_m[0].bars[0].load_ms;
                    } else if (!branch_e.empty() && !branch_e[0].bars.empty()) {
                        branch_start_time = branch_e[0].bars[0].load_ms;
                    } else if (!branch_n.empty() && !branch_n[0].bars.empty()) {
                        branch_start_time = branch_n[0].bars[0].load_ms;
                    }

                    std::vector<std::vector<Note>*> note_lists;
                    note_lists.push_back(&current_notes_draw);
                    if (!branch_n.empty()) {
                        note_lists.push_back(&branch_n[0].draw_notes);
                    }
                    if (!branch_e.empty()) {
                        note_lists.push_back(&branch_e[0].draw_notes);
                    }
                    if (!branch_m.empty()) {
                        note_lists.push_back(&branch_m[0].draw_notes);
                    }
                    if (!draw_note_list.empty()) {
                        note_lists.push_back(&draw_note_list);
                    }

                    std::set<double> seen_notes_times;
                    for (auto* notes : note_lists) {
                        for (const Note& note : *notes) {
                            if (note.type > 0 && note.type <= 4 &&
                                start_time <= note.hit_ms && note.hit_ms < branch_start_time) {
                                seen_notes_times.insert(note.hit_ms);
                            }
                        }
                    }

                    int note_count = std::max((int)seen_notes_times.size(), 1);
                    curr_branch_reqs = std::make_tuple(e_req, m_req, branch_start_time, note_count);
                }
            }
        }
    }

    Note& first_bar = current_bars[0];
    if (current_ms >= first_bar.unload_ms) {
        current_bars.erase(current_bars.begin());
    }
}

void Player::play_note_manager(double current_ms) {//, background: Optional[Background]):
    if (!don_notes.empty() && don_notes[0].hit_ms + Timing::BAD < current_ms) {
        combo = 0;
        /*if background is not None:
            if self.is_2p:
                background.add_chibi(True, 2)
            else:
                background.add_chibi(True, 1)*/
        bad_count++;
        if (gauge.has_value()) {
            gauge->add_bad();
        }
        don_notes.pop_front();
        if (is_branch && branch_condition == "p") {
            branch_condition_count--;
        }
    }

    if (!kat_notes.empty() && kat_notes[0].hit_ms + Timing::BAD < current_ms) {
        combo = 0;
        /*if background is not None:
            if self.is_2p:
                background.add_chibi(True, 2)
            else:
                background.add_chibi(True, 1)*/
        bad_count++;
        if (gauge.has_value()) {
            gauge->add_bad();
        }
        kat_notes.pop_front();
        if (is_branch && branch_condition == "p") {
            branch_condition_count--;
        }
    }

    if (other_notes.empty()) return;

    Note note = other_notes.front();
    if (note.hit_ms <= current_ms) {
        if (note.type == (int)NoteType::ROLL_HEAD || note.type == (int)NoteType::ROLL_HEAD_L) {
            is_drumroll = true;
        } else if (note.type == (int)NoteType::BALLOON_HEAD || note.type == (int)NoteType::KUSUDAMA) {
            is_balloon = true;
        } else if (note.type == (int)NoteType::TAIL) {
            other_notes.pop_front();
            is_drumroll = false;
            is_balloon = false;
            curr_balloon_count = 0;
            curr_drumroll_count = 0;
            return;
        }
        Note tail = other_notes[1];
        if (tail.hit_ms <= current_ms) {
            other_notes.pop_front();
            other_notes.pop_front();
            is_drumroll = false;
            is_balloon = false;
            curr_balloon_count = 0;
            curr_drumroll_count = 0;
        }
    }
}

bool Player::is_balloon_type(int type) {
    return type == (int)NoteType::ROLL_HEAD ||
           type == (int)NoteType::ROLL_HEAD_L ||
           type == (int)NoteType::BALLOON_HEAD ||
           type == (int)NoteType::KUSUDAMA;
}

void Player::draw_note_manager(double current_ms) {
    if (!draw_note_list.empty() && current_ms >= draw_note_list[0].load_ms) {
        Note current_note = draw_note_list.front();
        draw_note_list.erase(draw_note_list.begin());

        if (current_note.type >= 5 && current_note.type <= 7) {
            auto pos = std::lower_bound(current_notes_draw.begin(), current_notes_draw.end(),
                                        current_note,
                                        [](const auto& a, const auto& b) { return a.index < b.index; });
            current_notes_draw.insert(pos, current_note);

            auto tail_it = std::find_if(draw_note_list.begin(), draw_note_list.end(),
                                        [&current_note](const auto& note) {
                                            return note.type == (int)NoteType::TAIL && note.index > current_note.index;
                                        });

            if (tail_it != draw_note_list.end()) {
                auto tail_note = *tail_it;

                pos = std::lower_bound(current_notes_draw.begin(), current_notes_draw.end(),
                                      tail_note,
                                      [](const auto& a, const auto& b) { return a.index < b.index; });
                current_notes_draw.insert(pos, tail_note);

                draw_note_list.erase(tail_it);
            }
        } else {
            auto pos = std::lower_bound(current_notes_draw.begin(), current_notes_draw.end(),
                                        current_note,
                                        [](const auto& a, const auto& b) { return a.index < b.index; });
            current_notes_draw.insert(pos, current_note);
        }
    }

    if (current_notes_draw.empty()) return;

    if (is_drumroll && !other_notes.empty()) {
        int active_drumroll_index = other_notes[0].index;
        auto drumroll_it = std::find_if(current_notes_draw.begin(), current_notes_draw.end(),
                                         [active_drumroll_index](const Note& n) {
                                             return n.index == active_drumroll_index &&
                                                    (n.type == (int)NoteType::ROLL_HEAD ||
                                                     n.type == (int)NoteType::ROLL_HEAD_L);
                                         });
        if (drumroll_it != current_notes_draw.end() && drumroll_it->color.has_value() &&
            last_drumroll_color_time + 16.67f < current_ms) {
            last_drumroll_color_time = current_ms;
            drumroll_it->color = std::min(255, drumroll_it->color.value() + 1);
        }
    }

    Note note = current_notes_draw[0];

    if (is_balloon_type(note.type) && current_notes_draw.size() > 1) {
        note = current_notes_draw[1];
    }

    if (current_ms >= note.unload_ms) {
        current_notes_draw.erase(current_notes_draw.begin());
    }
}

void Player::note_manager(double current_ms) {//, background: Optional[Background]):
    bar_manager(current_ms);
    play_note_manager(current_ms);//, background)
    draw_note_manager(current_ms);
}

void Player::note_correct(Note note, double current_ms) {
    if (!don_notes.empty() && don_notes[0] == note) {
        don_notes.pop_front();
    } else if (!kat_notes.empty() && kat_notes[0] == note) {
        kat_notes.pop_front();
    } else if (!other_notes.empty() && other_notes[0] == note) {
        other_notes.pop_front();
    }

    int index = note.index;
    if (note.type == (int)NoteType::BALLOON_HEAD) {
        if (!other_notes.empty()) {
            other_notes.pop_front();
        }
    }

    if (note.type < 7) {
        combo++;
        if (combo % 10 == 0) {
            //chara.set_animation("10_combo")
        }
        if (combo % 100 == 0) {
            combo_announce = ComboAnnounce(combo, current_ms, player_num, is_2p);
        }
        if (combo > max_combo) {
            max_combo = combo;
        }
        if (combo % 100 == 0 && score_method == ScoreMethod::GEN3) {
            score += 10000;
            base_score_list.push_back(ScoreCounterAnimation(player_num, 10000, is_2p));
        }
    }

    if (note.type != (int)NoteType::KUSUDAMA) {
        bool is_big = note.type == (int)NoteType::DON_L || note.type == (int)NoteType::KAT_L || note.type == (int)NoteType::BALLOON_HEAD;
        draw_arc_list.push_back(NoteArc(note.type, current_ms, PlayerNum(is_2p + 1), is_big, note.type == (int)NoteType::BALLOON_HEAD, judge_x, judge_y));
    }
    auto it = std::find(current_notes_draw.begin(), current_notes_draw.end(), note);
    if (it != current_notes_draw.end()) {
        current_notes_draw.erase(it);
    }
}

void Player::check_drumroll(double current_ms, DrumType drum_type) { //background: Optional[Background]
    draw_arc_list.push_back(NoteArc((int)drum_type, current_ms, PlayerNum(is_2p + 1), (int)drum_type == 3 || (int)drum_type == 4, false));
    curr_drumroll_count++;
    total_drumroll++;
    if (is_branch && branch_condition == "r") {
        branch_condition_count++;
    }
    /*if background is not None:
        background.add_renda()*/
    score += 100;
    if (base_score_list.size() < 5) {
        base_score_list.push_back(ScoreCounterAnimation(player_num, 100, is_2p));
    }
    if (current_notes_draw.empty()) return;
    if (!other_notes.empty()) {
        int active_drumroll_index = other_notes[0].index;
        auto drumroll_it = std::find_if(current_notes_draw.begin(), current_notes_draw.end(),
                                         [active_drumroll_index](const Note& n) {
                                             return n.index == active_drumroll_index &&
                                                    (n.type == (int)NoteType::ROLL_HEAD ||
                                                     n.type == (int)NoteType::ROLL_HEAD_L);
                                         });
        if (drumroll_it != current_notes_draw.end() && drumroll_it->color.has_value()) {
            drumroll_it->color.value() = std::max(0, 255 - (curr_drumroll_count * 10));
        }
    }
}

void Player::check_balloon(double current_ms, DrumType drum_type, Note balloon) {
    if (drum_type != DrumType::DON) return;
    if (balloon.is_kusudama.value()) {
        check_kusudama(current_ms, balloon);
        return;
    }
    if (!balloon_counter.has_value()) {
        balloon_counter = BalloonCounter(balloon.count.value(), player_num, is_2p);
    }
    curr_balloon_count++;
    total_drumroll++;
    score += 100;
    if (base_score_list.size() < 5) {
        base_score_list.push_back(ScoreCounterAnimation(player_num, 100, is_2p));
    }
    if (curr_balloon_count == balloon.count.value()) {
        is_balloon = false;
        balloon_counter->update(current_ms, curr_balloon_count);
        audio->play_sound("balloon_pop", "hitsound");
        note_correct(balloon, current_ms);
        curr_balloon_count = 0;
    }
}

void Player::check_kusudama(double current_ms, Note balloon) {
    if (!kusudama_counter.has_value()) {
        kusudama_counter = KusudamaCounter(balloon.count.value());
    }
    curr_balloon_count++;
    total_drumroll++;
    score += 100;
    base_score_list.push_back(ScoreCounterAnimation(player_num, 100, is_2p));
    if (curr_balloon_count == balloon.count) {
        audio->play_sound("kusudama_pop", "hitsound");
        is_balloon = false;
        balloon.popped = true;
        kusudama_counter->update(current_ms, balloon.popped.value());
        curr_balloon_count = 0;
    }
}

void Player::check_note(double ms_from_start, DrumType drum_type, double current_ms) {//, background: Optional[Background]):
    if (don_notes.empty() && kat_notes.empty() && other_notes.empty()) return;

    float good_window_ms;
    float ok_window_ms;
    float bad_window_ms;
    if (difficulty < (int)Difficulty::NORMAL) {
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
        check_drumroll(current_ms, drum_type);
        return;
    } else if (is_balloon && !other_notes.empty()) {
        curr_note = other_notes.front();
        check_balloon(current_ms, drum_type, curr_note);
        return;
    } else if (drum_type == DrumType::DON) {
        if (don_notes.empty()) return;
        curr_note = don_notes.front();
    } else if (drum_type == DrumType::KAT) {
        if (kat_notes.empty()) return;
        curr_note = kat_notes.front();
    }

    {
        curr_drumroll_count = 0;

        if (ms_from_start > (curr_note.hit_ms + bad_window_ms)) return;

        bool big = curr_note.type == (int)NoteType::DON_L || curr_note.type == (int)NoteType::KAT_L;
        if ((curr_note.hit_ms - good_window_ms <= ms_from_start) && (ms_from_start <= curr_note.hit_ms + good_window_ms)) {
            if (draw_judge_list.size() < 7) {
                draw_judge_list.push_back(Judgment(Judgments::GOOD, big, is_2p));
            }
            lane_hit_effect = LaneHitEffect(drum_type, Judgments::GOOD, is_2p);
            good_count++;
            score += base_score;
            if (base_score_list.size() < 5) {
                base_score_list.push_back(ScoreCounterAnimation(player_num, base_score, is_2p));
            }
            note_correct(curr_note, current_ms);
            if (gauge.has_value()) {
                gauge->add_good();
            }
            if (is_branch && branch_condition == "p") {
                branch_condition_count++;
            }
            /*if background is not None:
                if self.is_2p:
                    background.add_chibi(False, 2)
                else:
                    background.add_chibi(False, 1)*/

        } else if ((curr_note.hit_ms - ok_window_ms) <= ms_from_start && ms_from_start <= (curr_note.hit_ms + ok_window_ms)) {
            draw_judge_list.push_back(Judgment(Judgments::OK, big, is_2p));
            lane_hit_effect = LaneHitEffect(drum_type, Judgments::OK, is_2p);
            ok_count++;
            score += 10 * std::floor(base_score / 2 / 10);
            if (base_score_list.size() < 5) {
                base_score_list.push_back(ScoreCounterAnimation(player_num, 10 * std::floor(base_score / 2 / 10), is_2p));
            }
            note_correct(curr_note, current_ms);
            if (gauge.has_value()) {
                gauge->add_ok();
            }
            if (is_branch && branch_condition == "p") {
                branch_condition_count += 0.5;
            }
            /*if background is not None:
                if self.is_2p:
                    background.add_chibi(False, 2)
                else:
                    background.add_chibi(False, 1)*/

        } else if ((curr_note.hit_ms - bad_window_ms) <= ms_from_start && ms_from_start <= (curr_note.hit_ms + bad_window_ms)) {
            draw_judge_list.push_back(Judgment(Judgments::BAD, big, is_2p));
            bad_count++;
            combo = 0;
            Note note;
            if (drum_type == DrumType::DON) {
                note = don_notes.front();
                don_notes.pop_front();
            } else {
                note = kat_notes.front();
                kat_notes.pop_front();
            }
            current_notes_draw.erase(
                std::remove(current_notes_draw.begin(), current_notes_draw.end(), note),
                current_notes_draw.end()
            );
            if (gauge.has_value()) {
                gauge->add_bad();
            }
            /*if background is not None:
                if self.is_2p:
                    background.add_chibi(True, 2)
                else:
                    background.add_chibi(True, 1)*/
        }
    }
}

void Player::drumroll_counter_manager(double current_ms) {
    if (is_drumroll && curr_drumroll_count > 0 && drumroll_counter == std::nullopt) {
        drumroll_counter = DrumrollCounter(is_2p);
    }

    if (drumroll_counter.has_value()) {
        if (drumroll_counter->is_finished() && !is_drumroll) {
            drumroll_counter.reset();
        } else {
            drumroll_counter->update(current_ms, curr_drumroll_count);
        }
    }
}

void Player::balloon_counter_manager(double current_ms) {
    if (!is_balloon && balloon_counter.has_value()) {
        balloon_counter.reset();
    }
    if (balloon_counter.has_value()) {
        //chara.set_animation("balloon_popping");
        balloon_counter->update(current_ms, curr_balloon_count);
        if (balloon_counter->is_finished()) {
            if (score_method == ScoreMethod::GEN3) {
                score += 5000;
                base_score_list.push_back(ScoreCounterAnimation(player_num, 5000, is_2p));
            }
            balloon_counter.reset();
            //chara.set_animation("balloon_pop");
        }
    if (kusudama_counter.has_value()) {
        kusudama_counter->update(current_ms, !is_balloon);
        kusudama_counter->update_count(curr_balloon_count);
        if (kusudama_counter->is_finished()) {
            kusudama_counter.reset();
            }
        }
    }
}

void Player::spawn_hit_effects(DrumType drum_type, Side side) {
    lane_hit_effect = LaneHitEffect(drum_type, Judgments::BAD, is_2p); //judgment parameter workaround
    if (draw_drum_hit_list.size() < 4) {
        draw_drum_hit_list.push_back(DrumHitEffect(drum_type, side, is_2p));
    }
}

void Player::handle_input(double ms_from_start, double current_ms) { //, Background* background) {
    struct InputCheck {
        bool (*check_func)(PlayerNum);
        DrumType drum_type;
        Side side;
        std::string sound;
    };

    std::vector<InputCheck> input_checks = {
        InputCheck{is_l_don_pressed, DrumType::DON, Side::LEFT, don_hitsound},
        InputCheck{is_r_don_pressed, DrumType::DON, Side::RIGHT, don_hitsound},
        InputCheck{is_l_kat_pressed, DrumType::KAT, Side::LEFT, kat_hitsound},
        InputCheck{is_r_kat_pressed, DrumType::KAT, Side::RIGHT, kat_hitsound}
    };

    for (const auto& input : input_checks) {
        if (input.check_func(player_num)) {
            spawn_hit_effects(input.drum_type, input.side);
            audio->play_sound(input.sound, "hitsound");
            check_note(ms_from_start, input.drum_type, current_ms);//, background);
        }
    }
}

void Player::draw_bars(double current_ms) {
    if (current_bars.empty()) return;

    for (auto it = current_bars.rbegin(); it != current_bars.rend(); ++it) {
        auto& bar = *it;
        if (!bar.display) continue;
        float x_position = get_position_x(bar, current_ms) + judge_x;
        float y_position = get_position_y(bar, current_ms) + judge_y;
        float angle;
        if (y_position != 0) {
            angle = std::atan2(bar.scroll_y, bar.scroll_x) * 180.0 / PI;
        } else {
            angle = 0;
        }
        tex.draw_texture("notes", std::to_string(bar.type), {.x=x_position+tex.skin_config["moji_drumroll"].x - (tex.textures["notes"]["1"]->width/2.0f), .y=y_position+tex.skin_config["moji_drumroll"].y+(is_2p*tex.skin_config["2p_offset"].y), .rotation=angle});
    }
}

void Player::draw_drumroll(double current_ms, Note head, int current_eighth) {
    if (head.sudden_appear_ms.has_value() && head.sudden_moving_ms.has_value()) {
        double appear_ms = head.hit_ms - head.sudden_appear_ms.value();
        double moving_start_ms = head.hit_ms - head.sudden_moving_ms.value();
        if (current_ms < appear_ms) return;
        if (current_ms < moving_start_ms) {
            current_ms = moving_start_ms;
        }
    }
    float start_position = get_position_x(head, current_ms);
    auto it = std::find_if(current_notes_draw.begin() + 1, current_notes_draw.end(),
        [&head](const auto& note) {
            return note.type == (int)NoteType::TAIL && note.index > head.index;
        });

    auto& tail = (it != current_notes_draw.end()) ? *it : current_notes_draw[1];
    bool is_big = head.type == (int)NoteType::ROLL_HEAD_L;
    float end_position = get_position_x(tail, current_ms);
    float length = end_position - start_position;
    ray::Color color = ray::Color{255, (unsigned char)head.color.value(), (unsigned char)head.color.value(), 255};
    float y = tex.skin_config["notes"].y + get_position_y(head, current_ms) + judge_y;
    start_position += judge_x;
    end_position += judge_x;
    float moji_y = tex.skin_config["moji"].y;
    if (head.display) {
        tex.draw_texture("notes", "8", {.color=color, .frame=is_big, .x=start_position, .y=y+(is_2p*tex.skin_config["2p_offset"].y), .x2=length+tex.skin_config["drumroll_width_offset"].width});
        if (is_big) {
            tex.draw_texture("notes", "drumroll_big_tail", {.color=color, .x=end_position, .y=y+(is_2p*tex.skin_config["2p_offset"].y)});
        } else {
            tex.draw_texture("notes", "drumroll_tail", {.color=color, .x=end_position, .y=y+(is_2p*tex.skin_config["2p_offset"].y)});
        }
        tex.draw_texture("notes", std::to_string(head.type), {.color=color, .frame=current_eighth % 2, .x=start_position - tex.textures["notes"]["1"]->width/2.0f, .y=y+(is_2p*tex.skin_config["2p_offset"].y)+judge_y});
    }

    tex.draw_texture("notes", "moji_drumroll_mid", {.x=start_position, .y=moji_y+(is_2p*tex.skin_config["2p_offset"].y)+judge_y, .x2=length});
    tex.draw_texture("notes", "moji", {.frame=head.moji, .x=start_position - (tex.textures["notes"]["moji"]->width/2.0f), .y=moji_y+(is_2p*tex.skin_config["2p_offset"].y)+judge_y});
    tex.draw_texture("notes", "moji", {.frame=tail.moji, .x=end_position - (tex.textures["notes"]["moji"]->width/2.0f), .y=moji_y+(is_2p*tex.skin_config["2p_offset"].y)+judge_y});
}

void Player::draw_balloon(double current_ms, Note head, int current_eighth) {
    float offset = tex.skin_config["balloon_offset"].x;
    if (head.sudden_appear_ms.has_value() && head.sudden_moving_ms.has_value()) {
        double appear_ms = head.hit_ms - head.sudden_appear_ms.value();
        double moving_start_ms = head.hit_ms - head.sudden_moving_ms.value();
        if (current_ms < appear_ms) return;
        if (current_ms < moving_start_ms) {
            current_ms = moving_start_ms;
        }
    }
    float start_position = get_position_x(head, current_ms);
    auto it = std::find_if(current_notes_draw.begin() + 1, current_notes_draw.end(),
        [&head](const auto& note) {
            return note.type == (int)NoteType::TAIL && note.index > head.index;
        });

    auto& tail = (it != current_notes_draw.end()) ? *it : current_notes_draw[1];
    float end_position = get_position_x(tail, current_ms);
    float pause_position = JudgePos::X + judge_x;
    float y = tex.skin_config["notes"].y + get_position_y(head, current_ms) + judge_y;
    float moji_y = tex.skin_config["moji"].y + get_position_y(head, current_ms) + judge_y;
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
    if (head.display) {
        tex.draw_texture("notes", std::to_string(head.type), {.frame=current_eighth % 2, .x=position-offset - tex.textures["notes"]["1"]->width/2.0f, .y=y+(is_2p*tex.skin_config["2p_offset"].y)});
        tex.draw_texture("notes", "10", {.frame=current_eighth % 2, .x=position-offset+tex.textures["notes"]["10"]->width - tex.textures["notes"]["1"]->width/2.0f, .y=y+(is_2p*tex.skin_config["2p_offset"].y)});
    }
    tex.draw_texture("notes", "moji", {.frame=head.moji, .x=position - (tex.textures["notes"]["moji"]->width/2.0f), .y=moji_y + (is_2p*tex.skin_config["2p_offset"].y)});
}

void Player::draw_notes(double current_ms) {
    if (current_notes_draw.empty()) {
        return;
    }

    for (auto it = current_notes_draw.rbegin(); it != current_notes_draw.rend(); ++it) {
        auto& note = *it;

        if (balloon_counter.has_value() && note.type == (int)NoteType::BALLOON_HEAD && !other_notes.empty() && note.index == other_notes[0].index) {
            continue;
        }

        if (note.type == (int)NoteType::TAIL) {
            continue;
        }

        double eighth_in_ms = (bpm == 0) ? 0 : (60000.0 * 4.0 / bpm) / 8.0;
        int current_eighth = 0;

        if (combo >= 50 && eighth_in_ms != 0) {
            current_eighth = static_cast<int>(current_ms / eighth_in_ms);
        }

        float x_position, y_position;
        if (note.sudden_appear_ms.has_value() && note.sudden_moving_ms.has_value()) {
            double appear_ms = note.hit_ms - note.sudden_appear_ms.value();
            double moving_start_ms = note.hit_ms - note.sudden_moving_ms.value();

            if (current_ms < appear_ms) {
                continue;
            }

            double effective_ms = (current_ms < moving_start_ms) ? moving_start_ms : current_ms;

            x_position = get_position_x(note, effective_ms);
            y_position = get_position_y(note, current_ms);
            } else {
            x_position = get_position_x(note, current_ms);
            y_position = get_position_y(note, current_ms);
        }

        x_position += judge_x;
        y_position += judge_y;

        if (note.color.has_value()) {
            draw_drumroll(current_ms, note, current_eighth);
        } else if (note.count.has_value()) {
            if (!note.is_kusudama.value()) {
                draw_balloon(current_ms, note, current_eighth);
            }
        } else {
            if (note.display) {
                tex.draw_texture("notes", std::to_string(note.type), {.frame=current_eighth % 2, .center=true, .x=x_position - (tex.textures["notes"]["1"]->width/2.0f), .y=y_position+tex.skin_config["notes"].y+(is_2p*tex.skin_config["2p_offset"].y)});
            }
            tex.draw_texture("notes", "moji", {.frame=note.moji, .x=x_position - (tex.textures["notes"]["moji"]->width/2.0f), .y=tex.skin_config["moji"].y + y_position+(is_2p*tex.skin_config["2p_offset"].y)});
        }
    }
}

void Player::draw_modifiers() {

    if (score_method == ScoreMethod::SHINUCHI) {
        tex.draw_texture("lane", "mod_shinuchi", {.index=is_2p});
    }

    if (modifiers.speed >= 4) {
        tex.draw_texture("lane", "mod_yonbai", {.index=is_2p});
    } else if (modifiers.speed >= 3) {
        tex.draw_texture("lane", "mod_sanbai", {.index=is_2p});
    } else if (modifiers.speed > 1) {
        tex.draw_texture("lane", "mod_baisaku", {.index=is_2p});
    }

    if (modifiers.display) {
        tex.draw_texture("lane", "mod_doron", {.index=is_2p});
    }
    if (modifiers.inverse) {
        tex.draw_texture("lane", "mod_abekobe", {.index=is_2p});
    }
    if (modifiers.random == 2) {
        tex.draw_texture("lane", "mod_detarame", {.index=is_2p});
    } else if (modifiers.random == 1) {
        tex.draw_texture("lane", "mod_kimagure", {.index=is_2p});
    }
}

void Player::draw_overlays(ray::Shader mask_shader) {
    tex.draw_texture("lane", std::to_string((int)player_num) + "p_lane_cover", {.index=is_2p});
    if (is_dan) tex.draw_texture("lane", "dan_lane_cover");
    tex.draw_texture("lane", "drum", {.index=is_2p});
    //if self.ending_anim is not None:
        //self.ending_anim.draw()

    for (DrumHitEffect anim : draw_drum_hit_list) {
        anim.draw();
    }
    for (NoteArc anim : draw_arc_list) {
        anim.draw(mask_shader);
    }
    for (GaugeHitEffect anim : gauge_hit_effect) {
        anim.draw();
    }

    combo_display.draw();
    if (combo_announce.has_value()) {
        combo_announce->draw();
    }
    if (is_2p) {
        tex.draw_texture("lane", "lane_score_cover", {.mirror="vertical", .index=is_2p});
    } else {
        tex.draw_texture("lane", "lane_score_cover", {.index=is_2p});
    }
    tex.draw_texture("lane", std::to_string((int)player_num) + "p_icon", {.index=is_2p});
    if (is_dan) {
        tex.draw_texture("lane", "lane_difficulty", {.frame=6});
    } else {
        tex.draw_texture("lane", "lane_difficulty", {.frame=difficulty, .index=is_2p});
    }
    if (judge_counter.has_value()) {
        judge_counter->draw();
    }

    if (modifiers.auto_play) {
        tex.draw_texture("lane", "auto_icon", {.index=is_2p});
    } else {
        if (is_2p) {
            //self.nameplate.draw(tex.skin_config["game_nameplate_1p"].x, tex.skin_config["game_nameplate_1p"].y)
        } else {
            //self.nameplate.draw(tex.skin_config["game_nameplate_2p"].x, tex.skin_config["game_nameplate_2p"].y)
        }
    }
    draw_modifiers();
    //self.chara.draw(y=(self.is_2p*tex.skin_config["game_2p_offset"].y))

    if (drumroll_counter.has_value()) {
        drumroll_counter->draw();
    }
    if (balloon_counter.has_value()) {
        balloon_counter->draw();
    }
    if (kusudama_counter.has_value()) {
        kusudama_counter->draw();
    }
    score_counter.draw();
    for (ScoreCounterAnimation anim : base_score_list) {
        anim.draw();
    }
}
