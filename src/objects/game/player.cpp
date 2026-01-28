#include "player.h"

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
{
    reset_chart();
    don_hitsound = "hitsound_don_" + std::to_string((int)player_num) + "p";
    kat_hitsound = "hitsound_kat_" + std::to_string((int)player_num) + "p";

    //self.drumroll_counter: Optional[DrumrollCounter] = None
    //self.balloon_anim: Optional[BalloonAnimation] = None
    //self.kusudama_anim: Optional[KusudamaAnimation] = None
    //self.base_score_list: list[ScoreCounterAnimation] = []
    //self.score_counter = ScoreCounter(self.score, self.is_2p)
    //self.gogo_time: Optional[GogoTime] = None
    //self.delay_start: Optional[float] = None
    //self.delay_end: Optional[float] = None
    //self.combo_announce = ComboAnnounce(self.combo, 0, player_num, self.is_2p)
    /*if (!parser->metadata.course_data) {
        self.branch_indicator = None
    } else {
        self.branch_indicator = BranchIndicator(self.is_2p) if parser and parser.metadata.course_data[self.difficulty].is_branching else None
    }*/
    //self.ending_anim: Optional[FailAnimation | ClearAnimation | FCAnimation] = None
    //plate_info = global_data.config[f"nameplate_{self.is_2p+1}p"]
    //self.nameplate = Nameplate(plate_info["name"], plate_info["title"], global_data.player_num, plate_info["dan"], plate_info["gold"], plate_info["rainbow"], plate_info["title_bg"])
    //self.chara = Chara2D(player_num - 1, self.bpm)
    /*if global_data.config["general"]["judge_counter"]:
        self.judge_counter = JudgeCounter()
    else:
        self.judge_counter = None*/

    //self.input_log: dict[float, str] = dict()
    /*if not parser.metadata.course_data:
        stars = 10
    else:
        stars = parser.metadata.course_data[self.difficulty].level
    self.gauge = Gauge(self.player_num, self.difficulty, stars, self.total_notes, self.is_2p)*/
}

void Player::update(double ms_from_start, double current_ms) {
    note_manager(ms_from_start);//, background);
    combo_display.update(current_ms, combo);
    //self.combo_announce.update(current_time)
    //self.drumroll_counter_manager(current_time)
    for (auto it = draw_judge_list.begin(); it != draw_judge_list.end(); ) {
        it->update(current_ms);
        if (it->is_finished()) {
            it = draw_judge_list.erase(it);
        } else {
            ++it;
        }
    }
    //self.balloon_manager(current_time)
    //if self.gogo_time is not None:
        //self.gogo_time.update(current_time)
    if (lane_hit_effect != std::nullopt) {
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
    //self.handle_timeline(ms_from_start)
    /*
    if self.delay_start is not None and self.delay_end is not None:
        # Currently, a delay is active: notes should be frozen at ms = delay_start
        # Check if it ended
        if ms_from_start >= self.delay_end:
            delay = self.delay_end - self.delay_start
            for note in chain(self.don_notes, self.kat_notes, self.other_notes, self.current_bars, self.draw_bar_list):
                note.load_ms += delay
            self.delay_start = None
            self.delay_end = None
     */

    for (auto it = draw_arc_list.begin(); it != draw_arc_list.end(); ) {
        it->update(current_ms);
        if (it->is_finished()) {
            // Save arc data before erasing
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
    //self.animation_manager(self.base_score_list, current_time)
    //self.score_counter.update(current_time, self.score)
    //self.autoplay_manager(ms_from_start, current_time, background)
    handle_input(ms_from_start, current_ms);//, background);
    //self.nameplate.update(current_time)
    //if self.gauge is not None:
        //self.gauge.update(current_time)
    //if self.judge_counter is not None:
        //self.judge_counter.update(self.good_count, self.ok_count, self.bad_count, self.total_drumroll)
    //if self.branch_indicator is not None:
        //self.branch_indicator.update(current_time)
    //if self.ending_anim is not None:
        //self.ending_anim.update(current_time)

    /*if self.is_branch:
        self.evaluate_branch(ms_from_start)

    if self.gauge is None:
        self.chara.update(current_time, self.bpm, False, False)
    else:
        self.chara.update(current_time, self.bpm, self.gauge.is_clear, self.gauge.is_rainbow)
    */
}

void Player::draw(double ms_from_start, ray::Shader& mask_shader) {
    //Group 1: Background and lane elements
    tex.draw_texture("lane", "lane_background", {.index = is_2p});
    if (player_num == PlayerNum::AI) tex.draw_texture("lane", "ai_lane_background");
    //if self.branch_indicator is not None:
        //self.branch_indicator.draw()
    //if self.gauge is not None:
        //self.gauge.draw()
    if (lane_hit_effect != std::nullopt) {
        lane_hit_effect->draw();
    }
    tex.draw_texture("lane", "lane_hit_circle", {.x =  judge_x, .y =  judge_y, .index = is_2p});

    //Group 2: judgment and hit effects
    //if self.gogo_time is not None:
        //self.gogo_time.draw(self.judge_x, self.judge_y)
    for (Judgment anim : draw_judge_list) {
        anim.draw(judge_x, judge_y);
    }

    //Group 3: Notes and bars (game content)
    //self.draw_bars(ms_from_start)
    draw_notes(ms_from_start);
    //if dan_transition is not None:
        //dan_transition.draw()

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

    note.load_ms = note.hit_ms - normal_travel_ms;
    note.unload_ms = note.hit_ms + normal_travel_ms;
    return;

    /*if (!note.sudden_appear_ms.has_value() ||
        !note.sudden_moving_ms.has_value() ||
        *note.sudden_appear_ms == std::numeric_limits<float>::infinity()) {
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
    note.unload_ms = note.hit_ms + unload_offset;*/
}

void Player::reset_chart() {
    auto [notes, branch_m, branch_e, branch_n] = parser->notes_to_position(difficulty);
    //self.play_notes, self.draw_note_list, self.draw_bar_list = deque(apply_modifiers(notes, self.modifiers)[0]), deque(apply_modifiers(notes, self.modifiers)[1]), deque(apply_modifiers(notes, self.modifiers)[2])

    draw_note_list = notes.draw_notes;
    draw_bar_list = notes.bars;
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
    /*
    if self.branch_m:
        for section in self.branch_m:
            self.total_notes += len([note for note in section.play_notes if 0 < note.type < 5])
            total_notes += section
     */
    base_score = 0;
    score_init = 0;
    score_diff = 0;
    /*
    if self.score_method == ScoreMethod.SHINUCHI:
        self.base_score = calculate_base_score(total_notes)
    elif self.score_method == ScoreMethod.GEN3:
        self.score_diff = self.parser.metadata.course_data[self.difficulty].scorediff
        if self.score_diff <= 0:
            logger.warning("Error: No scorediff specified or scorediff less than 0 | Using shinuchi scoring method instead")
            self.score_diff = 0

        score_init_list = self.parser.metadata.course_data[self.difficulty].scoreinit
        if len(score_init_list) <= 0:
            logger.warning("Error: No scoreinit specified or scoreinit less than 0 | Using shinuchi scoring method instead")
            self.score_init = calculate_base_score(total_notes)
            self.score_diff = 0 # in case there is a diff but not an init.
        else:
            self.score_init = score_init_list[0]
        logger.debug(f"debug | score init: {self.score_init} | score diff: {self.score_diff}")
     */

    //Note management
    timeline = notes.timeline;
    timeline_index = 0; //Range: [0, len(timeline)]
    //current_bars: list[Note] = []
    is_drumroll = false;
    curr_drumroll_count = 0;
    is_balloon = false;
    curr_balloon_count = 0;
    is_branch = false;
    //curr_branch_reqs = []
    branch_condition_count = 0;
    branch_condition = "";
    balloon_index = 0;
    bpm = 120;
    /*if (timeline and hasattr(self.timeline[self.timeline_index], "bpm"):
        self.bpm = self.timeline[self.timeline_index].bpm*/

    Note* last_note = nullptr;

    if (!draw_note_list.empty()) {
        last_note = &draw_note_list[0];
    } else if (!branch_m.empty() && !branch_m[0].draw_notes.empty()) {
        last_note = &branch_m[0].draw_notes[0];
    }

    for (Note& note : draw_note_list) {
        get_load_time(note);

        if (note.type == (int)NoteType::TAIL) {
            note.load_ms = last_note->load_ms;
            last_note->unload_ms = note.unload_ms;
        }

        last_note = &note;
    }

    for (Note& note : draw_bar_list) {
        get_load_time(note);

        if (note.type == (int)NoteType::TAIL) {
            note.load_ms = last_note->load_ms;
            last_note->unload_ms = note.unload_ms;
        }

        last_note = &note;
    }

    std::sort(draw_note_list.begin(), draw_note_list.end(),
        [](const Note& a, const Note& b) {
            return a.load_ms < b.load_ms;
        });

    //Handle HBSCROLL, BMSCROLL (pre-modify hit_ms, so that notes can"t be literally hit, but are still visually different) - basically it applies the transformations of #BPMCHANGE and #DELAY to hit_ms, so that notes can"t be hit even if its visaulyl
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

    //Decide end_time after all transforms have been applied
    end_time = !notes.play_notes.empty() ? notes.play_notes.back().hit_ms : 0;

    /*std::vector<std::reference_wrapper<std::vector<Section>>> branches = {
        std::ref(branch_m),
        std::ref(branch_e),
        std::ref(branch_n)
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
                    end_time = std::max(end_time, section.play_notes.back().hit_ms);
                }
            }
        }
    }*/
}

float Player::get_position_x(Note note, double current_ms) {
    /*if self.delay_start:
        current_ms = self.delay_start*/
    float speedx = note.bpm / 240000 * note.scroll_x * (tex.screen_width - JudgePos::X);
    return JudgePos::X + (note.hit_ms - current_ms) * speedx;
}

float Player::get_position_y(Note note, double current_ms) {
    /*if self.delay_start:
        current_ms = self.delay_start*/
    float speedy = note.bpm / 240000 * note.scroll_y * ((tex.screen_width - JudgePos::X)/tex.screen_width) * tex.screen_width;
    return (note.hit_ms - current_ms) * speedy;
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
        //self.input_log[self.don_notes[0].index] = "BAD"
        //if self.gauge is not None:
            //self.gauge.add_bad()
        don_notes.pop_front();
        //if self.is_branch and self.branch_condition == "p":
            //self.branch_condition_count -= 1
    }

    if (!kat_notes.empty() && kat_notes[0].hit_ms + Timing::BAD < current_ms) {
        combo = 0;
        /*if background is not None:
            if self.is_2p:
                background.add_chibi(True, 2)
            else:
                background.add_chibi(True, 1)*/
        bad_count++;
        //self.input_log[self.kat_notes[0].index] = "BAD"
        //if self.gauge is not None:
            //self.gauge.add_bad()
        kat_notes.pop_front();
        //if self.is_branch and self.branch_condition == "p":
            //self.branch_condition_count -= 1
    }

    if (other_notes.empty()) return;

    Note note = other_notes.front();
    if (note.hit_ms <= current_ms) {
        if (note.type == (int)NoteType::ROLL_HEAD or note.type == (int)NoteType::ROLL_HEAD_L) {
            is_drumroll = true;
        } else if (note.type == (int)NoteType::BALLOON_HEAD or note.type == (int)NoteType::KUSUDAMA) {
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
                                        [](const auto& note) { return note.type == (int)NoteType::TAIL; });

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

    if (current_notes_draw[0].color.has_value()) {
        current_notes_draw[0].color.value() = std::min(255, current_notes_draw[0].color.value() + 1);
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
    //bar_manager(current_ms)
    play_note_manager(current_ms);//, background)
    draw_note_manager(current_ms);
}

void Player::note_correct(Note note, double current_time) {
    if (!don_notes.empty() and don_notes[0] == note) {
        don_notes.pop_front();
    } else if (!kat_notes.empty() and kat_notes[0] == note) {
        kat_notes.pop_front();
    } else if (!other_notes.empty() and other_notes[0] == note) {
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
        /*if self.combo % 10 == 0:
            self.chara.set_animation("10_combo")
        if self.combo % 100 == 0:
            self.combo_announce = ComboAnnounce(self.combo, current_time, self.player_num, self.is_2p)
        if self.combo > self.max_combo:
            self.max_combo = self.combo
        if self.combo % 100 == 0 and self.score_method == ScoreMethod.GEN3:
            self.score += 10000
            self.base_score_list.append(ScoreCounterAnimation(self.player_num, 10000, self.is_2p))*/
    }

    if (note.type != (int)NoteType::KUSUDAMA) {
        bool is_big = note.type == (int)NoteType::DON_L or note.type == (int)NoteType::KAT_L or note.type == (int)NoteType::BALLOON_HEAD;
        bool is_balloon = note.type == (int)NoteType::BALLOON_HEAD;
        draw_arc_list.push_back(NoteArc(note.type, current_time, PlayerNum(is_2p + 1), is_big, is_balloon, judge_x, judge_y));
    }
    auto it = std::find(current_notes_draw.begin(), current_notes_draw.end(), note);
    if (it != current_notes_draw.end()) {
        current_notes_draw.erase(it);
    }
}

void Player::check_note(double ms_from_start, DrumType drum_type, double current_time) {//, background: Optional[Background]):
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
    /*
    if self.score_method == ScoreMethod.GEN3:
        self.base_score = self.score_init
        if 9 < self.combo and self.combo < 30:
            self.base_score = math.floor(self.score_init + 1 * self.score_diff)
        elif 29 < self.combo and self.combo < 50:
            self.base_score = math.floor(self.score_init + 2 * self.score_diff)
        elif 49 < self.combo and self.combo < 100:
            self.base_score = math.floor(self.score_init + 4 * self.score_diff)
        elif 99 < self.combo:
            self.base_score = math.floor(self.score_init + 8 * self.score_diff)
     */

    if (is_drumroll && !other_notes.empty()) {
        //self.check_drumroll(drum_type, background, current_time);
    } else if (is_balloon && !other_notes.empty()) {
        /*if not isinstance(curr_note, Balloon):
            return
        self.check_balloon(drum_type, curr_note, current_time)*/
    } else {
        curr_drumroll_count = 0;

        Note curr_note;
        if (drum_type == DrumType::DON) {
            if (don_notes.empty()) return;
            curr_note = don_notes.front();
        } else {
            if (kat_notes.empty()) return;
            curr_note = kat_notes.front();
        }

        //If the note is too far away, stop checking
        if (ms_from_start > (curr_note.hit_ms + bad_window_ms)) return;

        bool big = curr_note.type == (int)NoteType::DON_L or curr_note.type == (int)NoteType::KAT_L;
        if ((curr_note.hit_ms - good_window_ms <= ms_from_start) && (ms_from_start <= curr_note.hit_ms + good_window_ms)) {
            if (draw_judge_list.size() < 7) {
                draw_judge_list.push_back(Judgment(Judgments::GOOD, big, is_2p));
            }
            lane_hit_effect = LaneHitEffect(drum_type, Judgments::GOOD, is_2p);
            good_count++;
            score += base_score;
            //if len(self.base_score_list) < 5:
                //self.base_score_list.append(ScoreCounterAnimation(self.player_num, self.base_score, self.is_2p))
            //self.input_log[curr_note.index] = "GOOD"
            note_correct(curr_note, current_time);
            //if self.gauge is not None:
                //self.gauge.add_good()
            //if self.is_branch and self.branch_condition == "p":
                //self.branch_condition_count += 1
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
            //if len(self.base_score_list) < 5:
                //self.base_score_list.append(ScoreCounterAnimation(self.player_num, 10 * math.floor(self.base_score / 2 / 10), self.is_2p))
            //self.input_log[curr_note.index] = "OK"
            note_correct(curr_note, current_time);
            //if self.gauge is not None:
                //self.gauge.add_ok()
            //if self.is_branch and self.branch_condition == "p":
                //self.branch_condition_count += 0.5
            /*if background is not None:
                if self.is_2p:
                    background.add_chibi(False, 2)
                else:
                    background.add_chibi(False, 1)*/

        } else if ((curr_note.hit_ms - bad_window_ms) <= ms_from_start && ms_from_start <= (curr_note.hit_ms + bad_window_ms)) {
            //self.input_log[curr_note.index] = "BAD"
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
            /*if self.gauge is not None:
                self.gauge.add_bad()
            if background is not None:
                if self.is_2p:
                    background.add_chibi(True, 2)
                else:
                    background.add_chibi(True, 1)*/
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
            audio->playSound(input.sound, "hitsound");
            check_note(ms_from_start, input.drum_type, current_ms);//, background);
        }
    }
}

void Player::draw_notes(double current_ms) {
    if (current_notes_draw.empty()) {
        return;
    }

    for (auto it = current_notes_draw.rbegin(); it != current_notes_draw.rend(); ++it) {
        auto& note = *it;

        /*if (balloon_anim != nullptr && std::holds_alternative<Balloon>(note) &&
            std::get<Balloon>(note) == std::get<Balloon>(current_notes_draw[0])) {
            continue;
        }*/

        if (note.type == (int)NoteType::TAIL) {
            continue;
        }

        double eighth_in_ms = (bpm == 0) ? 0 : (60000.0 * 4.0 / bpm) / 8.0;
        int current_eighth = 0;

        if (combo >= 50 && eighth_in_ms != 0) {
            current_eighth = static_cast<int>(current_ms / eighth_in_ms);
        }

        float x_position, y_position;
        /*
        if (has_sudden) {
            double appear_ms = std::visit([](auto&& n) { return n.hit_ms - n.sudden_appear_ms; }, note);
            double moving_start_ms = std::visit([](auto&& n) { return n.hit_ms - n.sudden_moving_ms; }, note);

            if (current_ms < appear_ms) {
                continue;
            }

            double effective_ms = (current_ms < moving_start_ms) ? moving_start_ms : current_ms;

            x_position = get_position_x(note, effective_ms);
            y_position = get_position_y(note, current_ms);
        } else {
         */
            x_position = get_position_x(note, current_ms);
            y_position = get_position_y(note, current_ms);
        //}

        x_position += judge_x;
        y_position += judge_y;

        if (note.color.has_value()) {
            //draw_drumroll(current_ms, std::get<Drumroll>(note), current_eighth);
        } else if (note.count.has_value()) {
            if (!note.is_kusudama) {
                //draw_balloon(current_ms, balloon, current_eighth);
                tex.draw_texture("notes", "moji", {.frame=note.moji, .x=x_position, .y=tex.skin_config["moji"].y + y_position+(is_2p*tex.skin_config["2p_offset"].y)});
            }
        } else {
            if (note.display) {
                tex.draw_texture("notes", std::to_string(note.type), {.frame=current_eighth % 2, .center=true, .x=x_position - (tex.textures["notes"]["1"]->width/2.0f), .y=y_position+tex.skin_config["notes"].y+(is_2p*tex.skin_config["2p_offset"].y)});
            }
            tex.draw_texture("notes", "moji", {.frame=note.moji, .x=x_position - (tex.textures["notes"]["moji"]->width/2.0f), .y=tex.skin_config["moji"].y + y_position+(is_2p*tex.skin_config["2p_offset"].y)});
        }
    }
}

void Player::draw_overlays(ray::Shader mask_shader) {
    //Group 4: Lane covers and UI elements (batch similar textures)
    tex.draw_texture("lane", std::to_string((int)player_num) + "p_lane_cover", {.index=is_2p});
    if (is_dan) tex.draw_texture("lane", "dan_lane_cover");
    tex.draw_texture("lane", "drum", {.index=is_2p});
    //if self.ending_anim is not None:
        //self.ending_anim.draw()

    //Group 5: Hit effects and animations
    for (DrumHitEffect anim : draw_drum_hit_list) {
        anim.draw();
    }
    for (NoteArc anim : draw_arc_list) {
        anim.draw(mask_shader);
    }
    for (GaugeHitEffect anim : gauge_hit_effect) {
        anim.draw();
    }

    //Group 6: UI overlays
    combo_display.draw();
    //self.combo_announce.draw()
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
    //if self.judge_counter is not None:
        //self.judge_counter.draw()

    // Group 7: Player-specific elements
    if (modifiers.auto_play) {
        tex.draw_texture("lane", "auto_icon", {.index=is_2p});
    } else {
        if (is_2p) {
            //self.nameplate.draw(tex.skin_config["game_nameplate_1p"].x, tex.skin_config["game_nameplate_1p"].y)
        } else {
            //self.nameplate.draw(tex.skin_config["game_nameplate_2p"].x, tex.skin_config["game_nameplate_2p"].y)
        }
    }
    //self.draw_modifiers()
    //self.chara.draw(y=(self.is_2p*tex.skin_config["game_2p_offset"].y))

    // Group 8: Special animations and counters
    //if self.drumroll_counter is not None:
        //self.drumroll_counter.draw()
    //if self.balloon_anim is not None:
        //self.balloon_anim.draw()
    //if self.kusudama_anim is not None:
        //self.kusudama_anim.draw()
    //self.score_counter.draw()
    //for anim in self.base_score_list:
        //anim.draw()
}
