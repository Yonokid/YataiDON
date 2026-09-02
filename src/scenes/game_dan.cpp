#include "game_dan.h"
#include <algorithm>
#include "../libs/input.h"
#include "../libs/script.h"

void DanGameScreen::on_screen_start() {
    Screen::on_screen_start();
    mask_shader   = load_shader("shader/dummy.vs", "shader/mask.fs");
    ms_from_start = 0;
    start_ms      = 0;
    last_resync_ms = 0;
    start_delay   = 4000.0;
    song_started  = false;
    paused        = false;
    score_saved   = false;
    pause_time    = 0;
    song_index    = 0;
    prev_good = prev_ok = prev_bad = prev_drumroll = 0;

    JudgePos::X = tex.skin_config[SC::JUDGE_POS].x;
    JudgePos::Y = tex.skin_config[SC::JUDGE_POS].y;

    if (global_data.config->general.nijiiro_notes) {
        tex.load_folder("game", "notes_nijiiro");
    }
    auto rainbow_mask = std::dynamic_pointer_cast<SingleTexture>(tex.textures[BALLOON::RAINBOW_MASK]);
    auto rainbow      = std::dynamic_pointer_cast<SingleTexture>(tex.textures[BALLOON::RAINBOW]);
    if (rainbow_mask && rainbow) {
        SetShaderValueTexture(mask_shader, GetShaderLocation(mask_shader, "texture0"), rainbow_mask->texture);
        SetShaderValueTexture(mask_shader, GetShaderLocation(mask_shader, "texture1"), rainbow->texture);
    }

    init_dan();
    load_hitsounds();

    transition.emplace("", "", true);
    {
        const SessionData& _sd = global_data.session_data[(int)global_data.player_num];
        transition->set_dan(_sd.dan_color, _sd.song_title);
    }
    transition->start();

    result_transition = ResultTransition(PlayerNum::DAN);
    allnet_indicator  = AllNetIcon();
}

void DanGameScreen::init_dan() {
    SessionData& sd = global_data.session_data[(int)global_data.player_num];

    total_notes = 0;
    for (const auto& entry : sd.selected_dan) {
        try {
            SongParser sp(entry.song_path);
            auto [notes, bm, be, bn] = sp.notes_to_position(entry.difficulty);
            for (const Note& n : notes.notes)
                if (n.type >= NoteType::DON && n.type <= NoteType::KAT_L) total_notes++;
            for (auto& sec : bm)
                for (const Note& n : sec.notes)
                    if (n.type >= NoteType::DON && n.type <= NoteType::KAT_L) total_notes++;
        } catch (...) {}
    }
    if (total_notes == 0) total_notes = 1;

    dan_color = sd.dan_color;
    failed_out    = false;
    failed_out_at = 0.0;
    between.stop();
    exam_failed.assign(sd.selected_dan_exam.size(), false);
    exam_song_failed.assign(sd.selected_dan_exam.size(), {false, false, false});
    dan_info_cache.reset();
    song_max_combo = 0;
    dan_gauge = Gauge(GaugeMode::DAN, global_data.player_num, total_notes);

    // Create player for first song
    const auto& first = sd.selected_dan[0];
    sd.selected_difficulty = first.difficulty;
    parser.emplace(first.song_path, (int)start_delay);
    if (fs::exists(parser->metadata.wave))
        song_music = audio.load_sound(parser->metadata.wave, "song");

    players.clear();
    players.push_back(std::make_unique<Player>(
        parser, global_data.player_num, first.difficulty, false,
        get_player_modifiers(global_data.player_num)));
    players[0]->set_is_dan(true);
    players[0]->gauge.reset();
    players[0]->dan_gauge = &dan_gauge;

    init_skip();

    bpm        = parser->metadata.bpm;
    scene_preset = parser->metadata.scene_preset;
    background.emplace(global_data.player_num, bpm, "DAN");

    const std::string& lang = global_data.config->general.language;
    std::string title = sd.song_title;
    hori_name = std::make_unique<OutlinedText>(title, tex.skin_config[SC::DAN_TITLE].font_size, ray::WHITE, ray::BLACK, false);

    current_song_title = parser->metadata.title.count(lang) ? parser->metadata.title.at(lang) : parser->metadata.title.at("en");
    std::string subtitle = parser->metadata.subtitle.count(lang) ? parser->metadata.subtitle.at(lang) : "";
    song_info = SongInfo(current_song_title, subtitle, parser->metadata.subtitle_full_display, first.genre_index - 1, 1);

    start_ms = get_current_ms() - parser->metadata.offset * 1000 - (double)global_data.config->general.audio_offset;
}

void DanGameScreen::change_song() {
    SessionData& sd = global_data.session_data[(int)global_data.player_num];
    const auto& entry = sd.selected_dan[song_index];
    sd.selected_difficulty = entry.difficulty;

    if (song_music.has_value()) {
        audio.stop_sound(song_music.value());
        song_music.reset();
    }

    parser.emplace(entry.song_path, (int)start_delay);
    if (fs::exists(parser->metadata.wave))
        song_music = audio.load_sound(parser->metadata.wave, "song");

    song_started = false;

    players[0]->reload_for_dan(parser, entry.difficulty);
    players[0]->dan_gauge = &dan_gauge;

    init_skip();

    const std::string& lang = global_data.config->general.language;
    current_song_title = parser->metadata.title.count(lang) ? parser->metadata.title.at(lang) : parser->metadata.title.at("en");
    std::string subtitle = parser->metadata.subtitle.count(lang) ? parser->metadata.subtitle.at(lang) : "";
    song_info = SongInfo(current_song_title, subtitle, parser->metadata.subtitle_full_display, entry.genre_index - 1, song_index + 1);

    between.start(get_current_ms(), current_song_title, subtitle,
                  parser->metadata.subtitle_full_display);
    start_ms = get_current_ms() + DanBetween::SONG_OPEN_MS
             - parser->metadata.offset * 1000
             - (double)global_data.config->general.audio_offset;
}

void DanGameScreen::fill_unplayed_songs() {
    SessionData& sd = global_data.session_data[(int)global_data.player_num];
    const std::string& lang = global_data.config->general.language;

    for (int i = (int)sd.dan_result_data.songs.size(); i < (int)sd.selected_dan.size(); i++) {
        const DanSongEntry& entry = sd.selected_dan[i];
        DanResultSong res;
        res.genre_index         = entry.genre_index;
        res.selected_difficulty = entry.difficulty;
        res.diff_level          = entry.level;

        res.unreached = true;
        res.hidden    = entry.hidden;
        try {
            SongParser sp(entry.song_path);
            const auto& titles = sp.metadata.title;
            res.song_title = titles.count(lang) ? titles.at(lang)
                           : titles.count("en")  ? titles.at("en")
                           : titles.empty()      ? "" : titles.begin()->second;
        } catch (...) {
            spdlog::warn("Dan result: could not read {}", entry.song_path.string());
        }

        sd.dan_result_data.songs.push_back(res);
    }
}

const SkinInfo& DanGameScreen::dan_exam_info() {
    if (const SkinInfo* e = tex.skin_entry("dan_exam_info_game")) return *e;
    return tex.skin_config[SC::DAN_EXAM_INFO];
}

int DanGameScreen::get_exam_progress(const Exam& exam) {
    Player* p = players[0].get();
    float gauge_pct = (dan_gauge.gauge_max > 0)
        ? (dan_gauge.gauge_length / dan_gauge.gauge_max) * 100.0f : 0.0f;

    if (exam.type == "gauge")        return (int)gauge_pct;
    if (exam.type == "judgeperfect") return p->get_good();
    if (exam.type == "judgegood")    return p->get_ok();
    if (exam.type == "judgebad")     return p->get_bad();
    if (exam.type == "hit")          return p->get_good() + p->get_ok() + p->get_total_drumroll();
    if (exam.type == "score")        return p->get_score();
    if (exam.type == "combo")        return p->get_max_combo();
    if (exam.type == "renda")        return p->get_total_drumroll();
    return 0;
}

int DanGameScreen::get_exam_progress_song(const Exam& exam, int song_idx) {
    Player* p = players[0].get();
    int good, ok, bad, drum, score;
    if (song_idx < (int)song_stats.size()) {
        good  = song_stats[song_idx].good;  ok   = song_stats[song_idx].ok;
        bad   = song_stats[song_idx].bad;   drum = song_stats[song_idx].drumroll;
        score = song_stats[song_idx].score;
    } else if (song_idx == song_index) {
        good  = p->get_good() - prev_good;  ok   = p->get_ok() - prev_ok;
        bad   = p->get_bad()  - prev_bad;   drum = p->get_total_drumroll() - prev_drumroll;
        score = p->get_score() - prev_score;
    } else {
        return 0;                                   // not played yet
    }
    if (exam.type == "judgeperfect") return good;
    if (exam.type == "judgegood")    return ok;
    if (exam.type == "judgebad")     return bad;
    if (exam.type == "hit")          return good + ok + drum;
    if (exam.type == "score")        return score;
    if (exam.type == "renda")        return drum;
    if (exam.type == "combo") {
        if (song_idx < (int)song_stats.size()) return song_stats[song_idx].max_combo;
        if (song_idx == song_index)            return song_max_combo;
        return 0;
    }
    return get_exam_progress(exam);                 // gauge (course-wide by nature)
}

DanInfoCache DanGameScreen::calculate_dan_info() {
    DanInfoCache cache;
    Player* p = players[0].get();
    int used = p->get_good() + p->get_ok() + p->get_bad();
    cache.remaining_notes = std::max(0, total_notes - used);
    const bool near_end        = (float)total_notes * 0.10f > (float)cache.remaining_notes;
    const bool just_before_end = (float)total_notes * 0.05f > (float)cache.remaining_notes;

    const auto& exams = global_data.session_data[(int)global_data.player_num].selected_dan_exam;
    for (int i = 0; i < (int)exams.size(); i++) {
        const Exam& exam = exams[i];
        DanExamInfo info;
        info.exam_type  = exam.type;
        info.exam_range = exam.range;
        info.red_value  = exam.red;

        int val = get_exam_progress(exam);
        float progress = (exam.red > 0) ? (float)val / exam.red : 0.0f;

        if (exam.range == "less") {
            progress = 1.0f - progress;
            info.counter_value = std::max(0, exam.red - val);
        } else {
            info.counter_value = std::max(0, val);
        }
        progress = std::max(0.0f, std::min(1.0f, progress));
        info.progress = progress;

        float bar_full_w = dan_exam_info().width;
        info.bar_width = bar_full_w * progress;

        if (progress >= 1.0f)        info.bar_texture = "exam_max";
        else if (progress >= 0.5f)   info.bar_texture = "exam_gold";
        else                          info.bar_texture = "exam_red";
        info.bar_state = dan_bar_state(exam, val, true, near_end, just_before_end);

        info.gothrough  = exam.gothrough;
        info.song_count = std::min(song_index, 3);
        if (!exam.gothrough) {
            for (int j = 0; j < 3 && j <= song_index; j++) {
                int   sv = get_exam_progress_song(exam, j);
                float sp = (exam.red > 0) ? (float)sv / exam.red : 0.0f;
                if (exam.range == "less") { sp = 1.0f - sp; sv = std::max(0, exam.red - sv); }
                info.song_value[j]    = std::max(0, sv);
                info.song_progress[j] = std::max(0.0f, std::min(1.0f, sp));
                const bool live_j = (j == song_index);
                info.song_state[j]    = dan_bar_state(exam, get_exam_progress_song(exam, j),
                                                      live_j, near_end, just_before_end);
            }
            int cur = std::min(song_index, 2);
            info.counter_value = info.song_value[cur];
            info.progress      = info.song_progress[cur];
            info.bar_width     = bar_full_w * info.progress;
            if (info.progress >= 1.0f)      info.bar_texture = "exam_max";
            else if (info.progress >= 0.5f) info.bar_texture = "exam_gold";
            else                            info.bar_texture = "exam_red";
            info.bar_state = info.song_state[cur];
        }

        cache.exam_data.push_back(info);
    }
    return cache;
}

void DanGameScreen::check_exam_failures(bool course_finished, bool song_finished) {
    if (!dan_info_cache.has_value()) return;
    const auto& exams = global_data.session_data[(int)global_data.player_num].selected_dan_exam;
    for (int i = 0; i < (int)exams.size(); i++) {
        if (exam_failed[i]) continue;
        const Exam& exam = exams[i];
        int val = exam.gothrough ? get_exam_progress(exam)
                                  : get_exam_progress_song(exam, song_index);
        bool at_boundary = exam.gothrough ? course_finished
                                           : (song_finished || course_finished);

        if (exam.range == "more" && !at_boundary) continue;

        if (exam.range == "more" && val < exam.red) {
            exam_failed[i] = true;
            if (!exam.gothrough && i < (int)exam_song_failed.size() && song_index < 3)
                exam_song_failed[i][song_index] = true;
            audio.play_sound("exam_failed", VolumePreset::SOUND);
            spdlog::info("Dan exam {} ({}) failed: {} < {} ({}) at {} ms (song ends {} ms)",
                         i, exam.type, val, exam.red,
                         exam.gothrough ? "gothrough" : "per-song",
                         ms_from_start, players.empty() || !players[0] ? -1.0 : players[0]->end_time);
        } else if (exam.range == "less") {
            int remaining = std::max(0, exam.red - val);
            if (remaining == 0) {
                exam_failed[i] = true;
                if (!exam.gothrough && i < (int)exam_song_failed.size() && song_index < 3)
                    exam_song_failed[i][song_index] = true;
                audio.play_sound("dan_failed", VolumePreset::SOUND);
                spdlog::info("Dan exam {} ({}) failed: {} of {} used up ({}) at {} ms (song ends {} ms)",
                             i, exam.type, val, exam.red,
                             exam.gothrough ? "gothrough" : "per-song",
                             ms_from_start, players.empty() || !players[0] ? -1.0 : players[0]->end_time);
            }
        }
    }
}

int DanGameScreen::exam_tier(const Exam& exam, int value) {
    const bool has_gold = exam.gold > 0 && exam.gold != exam.red;
    if (exam.range == "less") {
        if (value >= exam.red) return 0;
        return has_gold && value < exam.gold ? 2 : 1;
    }
    if (value < exam.red) return 0;
    return has_gold && value >= exam.gold ? 2 : 1;
}

void DanGameScreen::save_result_data(bool all_failed) {
    SessionData& sd = global_data.session_data[(int)global_data.player_num];
    const int course_songs = std::min((int)sd.selected_dan.size(), 3);

    sd.dan_result_data.dan_color    = dan_color;
    sd.dan_result_data.dan_rank     = sd.dan_rank;
    sd.dan_result_data.dan_index    = sd.dan_index;
    sd.dan_result_data.dan_index_max = sd.dan_index_max;
    sd.dan_result_data.is_gaiden     = sd.dan_gaiden;
    sd.dan_result_data.dan_title    = sd.song_title;
    sd.dan_result_data.score        = players[0]->get_score();
    sd.dan_result_data.gauge_length = dan_gauge.gauge_length;
    sd.dan_result_data.max_combo    = players[0]->get_max_combo();
    sd.dan_result_data.exams        = sd.selected_dan_exam;

    sd.dan_result_data.exam_data.clear();
    int check = 2;                              // min tier over every exam
    const auto& exams = sd.selected_dan_exam;
    if (dan_info_cache.has_value()) {
        for (int i = 0; i < (int)dan_info_cache->exam_data.size(); i++) {
            DanResultExam re;
            re.progress      = dan_info_cache->exam_data[i].progress;
            re.counter_value = dan_info_cache->exam_data[i].counter_value;
            re.bar_texture   = dan_info_cache->exam_data[i].bar_texture;
            re.bar_state     = dan_info_cache->exam_data[i].bar_state;
            for (int j = 0; j < 3; j++)
                re.song_state[j] = dan_info_cache->exam_data[i].song_state[j];
            re.failed        = i < (int)exam_failed.size() && exam_failed[i];
            re.song_count    = dan_info_cache->exam_data[i].song_count;
            for (int j = 0; j < 3; j++) {
                re.song_value[j]    = dan_info_cache->exam_data[i].song_value[j];
                re.song_progress[j] = dan_info_cache->exam_data[i].song_progress[j];
            }
            if (i < (int)exams.size()) {
                const Exam& exam = exams[i];
                if (all_failed) {
                    re.tier = 0;
                } else if (exam.gothrough) {
                    re.tier = exam_tier(exam, get_exam_progress(exam));
                } else {
                    re.tier = 2;
                    for (int j = 0; j < course_songs; j++)
                        re.tier = std::min(re.tier, exam_tier(exam, get_exam_progress_song(exam, j)));
                }
                if (re.tier == 0) re.failed = true;
                re.bar_state = dan_bar_state(exam, get_exam_progress(exam));
                if (!exam.gothrough)
                    for (int j = 0; j < course_songs; j++)
                        re.song_state[j] = dan_bar_state(exam, get_exam_progress_song(exam, j));
            }
            check = std::min(check, re.tier);
            sd.dan_result_data.exam_data.push_back(re);
        }
    }
    if (exams.empty()) check = 0;               // a course with no exams cannot pass

    fill_unplayed_songs();

    int total_ok = 0, total_bad = 0;
    for (const auto& s : sd.dan_result_data.songs) { total_ok += s.ok; total_bad += s.bad; }

    int result;
    if (check == 2)      result = total_bad == 0 ? (total_ok == 0 ? 6 : 5) : 4;
    else if (check == 1) result = total_bad == 0 ? (total_ok == 0 ? 3 : 2) : 1;
    else                 result = 0;
    if (skipped) result = 0;
    sd.dan_result_data.odai_result = result;
    spdlog::info("Dan course verdict: check={} ok={} bad={} odai_result={}{}",
                 check, total_ok, total_bad, result, all_failed ? " (fail-out)" : "");
}

void DanGameScreen::trigger_fail_out(double current_ms) {
    failed_out    = true;
    failed_out_at = current_ms;
    if (song_music.has_value()) { audio.stop_sound(*song_music); song_music.reset(); }
    if (movie.has_value()) movie->stop();
    between.stop();

    SessionData& sd = global_data.session_data[(int)global_data.player_num];
    if ((int)sd.dan_result_data.songs.size() <= song_index) {
        DanResultSong song_res;
        song_res.song_title          = current_song_title;
        song_res.genre_index         = sd.selected_dan[song_index].genre_index;
        song_res.selected_difficulty = sd.selected_dan[song_index].difficulty;
        song_res.diff_level          = sd.selected_dan[song_index].level;
        song_res.good     = players[0]->get_good()           - prev_good;
        song_res.ok       = players[0]->get_ok()             - prev_ok;
        song_res.bad      = players[0]->get_bad()            - prev_bad;
        song_res.drumroll = players[0]->get_total_drumroll() - prev_drumroll;
        sd.dan_result_data.songs.push_back(song_res);
    }
    const bool reached_every_song = song_index >= (int)sd.selected_dan.size() - 1;
    save_result_data(!reached_every_song);
    score_saved = true;
    spdlog::info("Dan course ended early ({}): song {}/{} at {} ms",
                 skipped ? "skip" : "fail-out", song_index + 1,
                 (int)sd.selected_dan.size(), ms_from_start);
}

Screens DanGameScreen::on_screen_end(Screens next_screen) {
    dan_info_cache.reset();
    hori_name.reset();
    between.stop();
    exam_captions.clear();
    return GameScreen::on_screen_end(next_screen);
}

std::optional<Screens> DanGameScreen::update() {
    Screen::update();
    double current_ms = get_current_ms();
    allnet_indicator.update(current_ms);

    transition->update(current_ms);
    between.update(current_ms);
    ms_from_start = current_ms - start_ms;

    if (transition->is_finished() && between.song_may_start())
        start_song(ms_from_start);

    update_background(current_ms);

    resync_song(current_ms);

    if (!failed_out) {
        players[0]->update(ms_from_start, current_ms, background);
        song_max_combo = std::max(song_max_combo, players[0]->get_combo());
        song_info.update(current_ms);
        update_skip_dan();
    }
    result_transition.update(current_ms);

    if (!failed_out) {
        dan_info_cache = calculate_dan_info();
        check_exam_failures();

        push_dan_state();
    } else {
        constexpr double FAILOUT_RESULT_DELAY = 2800.0;
        if (current_ms >= failed_out_at + FAILOUT_RESULT_DELAY && !result_transition.is_started) {
            result_transition.start();
            audio.play_sound("dan_transition", VolumePreset::VOICE);
        }
    }

    if (result_transition.is_finished && !audio.is_sound_playing("dan_transition"))
        return on_screen_end(Screens::DAN_RESULT);

    SessionData& sd = global_data.session_data[(int)global_data.player_num];

    if (!failed_out && ms_from_start >= players[0]->end_time) {
        check_exam_failures(false, true);
        {
            bool boundary_failed = std::any_of(exam_failed.begin(), exam_failed.end(),
                                               [](bool f) { return f; });
            bool is_final_song = song_index >= (int)sd.selected_dan.size() - 1;
            if (boundary_failed && !is_final_song) {
                spdlog::info("Dan course fails out at the END of song {}/{} "
                             "(state 6): song was played to its natural end at {} ms",
                             song_index + 1, (int)sd.selected_dan.size(), ms_from_start);
                trigger_fail_out(current_ms);
                return std::nullopt;
            }
        }

        // Save per-song result if not yet saved for this song
        if ((int)sd.dan_result_data.songs.size() <= song_index) {
            DanResultSong song_res;
            song_res.song_title         = current_song_title;
            song_res.genre_index        = sd.selected_dan[song_index].genre_index;
            song_res.selected_difficulty= sd.selected_dan[song_index].difficulty;
            song_res.diff_level         = sd.selected_dan[song_index].level;
            song_res.good    = players[0]->get_good()           - prev_good;
            song_res.ok      = players[0]->get_ok()             - prev_ok;
            song_res.bad     = players[0]->get_bad()            - prev_bad;
            song_res.drumroll= players[0]->get_total_drumroll() - prev_drumroll;
            sd.dan_result_data.songs.push_back(song_res);
        }

        bool any_failed = std::any_of(exam_failed.begin(), exam_failed.end(),
                                      [](bool failed) { return failed; });
        bool is_last = (song_index == (int)sd.selected_dan.size() - 1) || any_failed;
        if (is_last) {
            if (ms_from_start >= players[0]->end_time + 1000 && !score_saved) {
                check_exam_failures(true, true);
                save_result_data(false);
                players[0]->spawn_ending_anim();
                score_saved = true;
            }
            constexpr double SKIP_RESULT_DELAY = 2800.0;
            double transition_delay = players[0]->was_skipped() ? SKIP_RESULT_DELAY : 8533.34;
            if (ms_from_start >= players[0]->end_time + transition_delay && !result_transition.is_started) {
                result_transition.start();
                audio.play_sound("dan_transition", VolumePreset::VOICE);
            }
        } else {
            SongStats st;
            st.good     = players[0]->get_good()           - prev_good;
            st.ok       = players[0]->get_ok()             - prev_ok;
            st.bad      = players[0]->get_bad()            - prev_bad;
            st.drumroll = players[0]->get_total_drumroll() - prev_drumroll;
            st.score    = players[0]->get_score()          - prev_score;
            st.max_combo = song_max_combo;
            if ((int)song_stats.size() <= song_index) song_stats.push_back(st);
            song_max_combo = players[0]->get_combo();

            prev_good     = players[0]->get_good();
            prev_ok       = players[0]->get_ok();
            prev_bad      = players[0]->get_bad();
            prev_drumroll = players[0]->get_total_drumroll();
            prev_score    = players[0]->get_score();

            song_index++;
            song_started = false;
            change_song();
        }
    }

    // Global keys (back / restart)
    if (check_key_pressed(global_data.config->keys.back_key)) {
        if (song_music.has_value()) audio.stop_sound(song_music.value());
        return on_screen_end(Screens::DAN_SELECT);
    }
    if (check_key_pressed(global_data.config->keys.restart_key)) {
        if (song_music.has_value()) { audio.stop_sound(song_music.value()); song_music.reset(); }
        song_index = 0;
        prev_good = prev_ok = prev_bad = prev_drumroll = prev_score = 0;
        song_stats.clear();
        sd.dan_result_data = DanResultData();
        // The flag still says the music is on from before the restart, and
        // with it set the first song never gets its playback started.
        song_started = false;
        score_saved  = false;
        init_dan();
        audio.play_sound("restart", VolumePreset::SOUND);
    }

    return std::nullopt;
}

void DanGameScreen::push_dan_state() {
    if (!background.has_value() || !background->wants_dan()) return;
    if (!dan_info_cache.has_value()) return;

    sol::state& lua = *script_manager.lua;
    sol::table st   = lua.create_table();
    const SessionData& sd = global_data.session_data[(int)global_data.player_num];

    st["dan_color"]       = dan_color;
    st["dan_title"]       = sd.song_title;
    st["song_index"]      = song_index;
    st["song_count"]      = (int)sd.selected_dan.size();
    st["total_notes"]     = total_notes;
    st["remaining_notes"] = dan_info_cache->remaining_notes;
    st["gauge"]           = dan_gauge.gauge_length;
    st["gauge_max"]       = dan_gauge.gauge_max;

    sol::table rows = lua.create_table();
    const auto& exams = sd.selected_dan_exam;
    for (int i = 0; i < (int)dan_info_cache->exam_data.size(); i++) {
        const DanExamInfo& info = dan_info_cache->exam_data[i];
        sol::table row = lua.create_table();
        row["type"]     = info.exam_type;
        row["range"]    = info.exam_range;
        row["red"]      = info.red_value;
        row["gold"]     = (i < (int)exams.size()) ? exams[i].gold : 0;
        row["value"]    = info.counter_value;
        row["progress"] = info.progress;
        row["bar"]      = info.bar_texture;
        row["failed"]   = (i < (int)exam_failed.size()) ? exam_failed[i] : false;
        rows[i + 1]     = row;
    }
    st["exams"] = rows;

    background->handle_dan(global_data.player_num, st);
}

void DanGameScreen::draw_digit_counter(const std::string& digits, float margin_x, TexID tex_id, int index, float y, float x_offset) {
    for (int j = 0; j < (int)digits.size(); j++) {
        float x = -(float)(digits.size() - j) * margin_x + x_offset;
        tex.draw_texture(tex_id, {.frame=digits[j]-'0', .x=x, .y=y, .index=index});
    }
}

void DanGameScreen::draw_exam_row(const DanExamInfo& info, const Exam& exam, int index, float y) {
    const SkinInfo& dei = dan_exam_info();
    const SkinInfo* bm = tex.skin_entry("dan_exam_border_margin");
    float border_margin = bm ? bm->x : tex.skin_config[SC::DAN_SCORE_BOX_MARGIN].x;
    const SkinInfo* vm = tex.skin_entry("dan_value_counter_margin");
    float value_margin = vm ? vm->x : border_margin;

    tex.draw_texture(DAN_INFO::EXAM_BG, {.y = y});
    tex.draw_texture(DAN_INFO::EXAM_BADGE,
                     {.frame = info.gothrough ? 0 : 1 + std::min(song_index, 2), .y = y});

    const bool all = info.gothrough;
    tex.draw_texture(all ? DAN_INFO::EXAM_FRAME_BACK_ALL : DAN_INFO::EXAM_OVERLAY_1, {.y = y});

    const SkinInfo* wa = tex.skin_entry("dan_exam_bar_all");
    float bar_full = all && wa ? wa->width : dei.width;

    auto have = [&](TexID id) {
        return tex.textures.find((uint32_t)id) != tex.textures.end();
    };
    auto fill = [&](const std::string& state, float w, float fy, bool sub) {
        if (w <= 0 || state == "empty") return;
        TexID rb         = sub ? DAN_INFO::EXAM_SUB_RAINBOW : DAN_INFO::EXAM_RAINBOW;
        if (!sub && all && have(DAN_INFO::EXAM_RAINBOW_ALL))
            rb = DAN_INFO::EXAM_RAINBOW_ALL;
        const TexID d80  = sub ? DAN_INFO::EXAM_SUB_DOWN80  : DAN_INFO::EXAM_DOWN80;
        const TexID up50 = sub ? DAN_INFO::EXAM_SUB_RED     : DAN_INFO::EXAM_RED;
        const TexID up80 = sub ? DAN_INFO::EXAM_SUB_GOLD    : DAN_INFO::EXAM_GOLD;
        const TexID p100 = sub ? DAN_INFO::EXAM_SUB_MAX     : DAN_INFO::EXAM_MAX;
        if (state == "max" && have(rb)) {
            auto it = tex.textures.find((uint32_t)rb);
            const float th   = (float)it->second->height;
            const float tw   = (float)it->second->width;
            const float perd = tw * 0.5f;
            const double phase_ms = get_frame_ms();
            const float ph   = perd - (float)std::fmod(phase_ms / 1000.0
                                                       * (perd * 60.0 / 80.0), (double)perd);
            const float avail = tw - ph;
            if (w <= avail) {
                tex.draw_texture(rb, {.y = fy, .x2 = w,
                                      .src = ray::Rectangle{ph, 0.0f, w, th}});
            } else {
                tex.draw_texture(rb, {.y = fy, .x2 = avail,
                                      .src = ray::Rectangle{ph, 0.0f, avail, th}});
                tex.draw_texture(rb, {.x = avail, .y = fy, .x2 = w - avail,
                                      .src = ray::Rectangle{0.0f, 0.0f, w - avail, th}});
            }
            return;
        }
        TexID id = p100;
        if (state == "up_50")           id = up50;
        else if (state == "up_80")      id = up80;
        else if (state == "max_soon")   id = p100;
        else if (state == "max_soon2")  id = p100;
        else if (state == "down_80")    id = have(d80) ? d80 : up80;
        tex.draw_texture(id, {.y = fy, .x2 = w});
    };
    if (exam_failed[index]) {
        tex.draw_texture(DAN_INFO::EXAM_FAIL, {.y = y, .x2 = bar_full});
    } else {
        fill(info.bar_state, bar_full * info.progress, y, false);
    }

    tex.draw_texture(all ? DAN_INFO::EXAM_FRAME_FRONT_ALL : DAN_INFO::EXAM_OVERLAY_2, {.y = y});

    static const std::unordered_map<std::string, TexID> exam_ids = {
        {"gauge",        DAN_INFO::EXAM_GAUGE},
        {"combo",        DAN_INFO::EXAM_COMBO},
        {"hit",          DAN_INFO::EXAM_HIT},
        {"judgebad",     DAN_INFO::EXAM_JUDGEBAD},
        {"judgegood",    DAN_INFO::EXAM_JUDGEGOOD},
        {"judgeperfect", DAN_INFO::EXAM_JUDGEPERFECT},
        {"score",        DAN_INFO::EXAM_SCORE},
        {"renda",        DAN_INFO::EXAM_ROLL},
    };
    auto icon_it = exam_ids.find(info.exam_type);
    if (icon_it != exam_ids.end())
        tex.draw_texture(icon_it->second, {.y = y});

    const SkinInfo* bt = tex.skin_entry("dan_game_exam_border_text");
    OutlinedText* cap = nullptr;
    float bt_ol = 4.0f / tex.screen_scale;
    if (bt) {
        if (bt->outline >= 0) bt_ol = bt->outline;
        cap = exam_captions.get(
            exam_threshold_text(tex, info.exam_type, info.exam_range,
                                info.red_value, global_data.config->general.language),
            bt->font_size > 0 ? bt->font_size : 36, bt_ol);
    }
    if (cap) {
        const float pad = ExamCaptionCache::pad_for(bt_ol, tex.screen_scale);
        cap->draw({.x = bt->x - cap->width + pad, .y = bt->y - pad + y});
    } else {
        if (info.exam_range == "less")      tex.draw_texture(DAN_INFO::EXAM_LESS, {.y = y});
        else if (info.exam_range == "more") tex.draw_texture(DAN_INFO::EXAM_MORE, {.y = y});

        float gauge_shift = 0.0f;
        if (info.exam_type == "gauge") {
            tex.draw_texture(DAN_INFO::EXAM_PERCENT, {.y = y, .index = 0});
            const SkinInfo* gs = tex.skin_entry("dan_exam_gauge_shift");
            gauge_shift = -(gs ? gs->x : border_margin);
        }
        draw_digit_counter(std::to_string(info.red_value), border_margin,
                           DAN_INFO::EXAM_BORDER_COUNTER, 0, y, gauge_shift);
    }

    if (exam_failed[index]) {
        tex.draw_texture(DAN_INFO::EXAM_FAILED, {.y = y});
    } else {
        const SkinInfo* vl = tex.skin_entry("dan_exam_value_left");
        const std::string live = std::to_string(info.counter_value);
        if (vl) {
            auto it = tex.textures.find((uint32_t)DAN_INFO::VALUE_COUNTER);
            const float json_x = (it != tex.textures.end() && !it->second->x.empty())
                               ? it->second->x[0] : 920.0f;
            for (int j = 0; j < (int)live.size(); j++)
                tex.draw_texture(DAN_INFO::VALUE_COUNTER,
                                 {.frame = live[j] - '0',
                                  .x = vl->x + j * value_margin - json_x, .y = y});
            if (info.exam_type == "gauge") {
                auto pit = tex.textures.find((uint32_t)DAN_INFO::EXAM_PERCENT);
                const float pjson = (pit != tex.textures.end() && pit->second->x.size() > 1)
                                  ? pit->second->x[1] : 944.0f;
                tex.draw_texture(DAN_INFO::EXAM_PERCENT,
                                 {.x = vl->x + live.size() * value_margin - pjson,
                                  .y = y, .index = 1});
            }
        } else {
            draw_digit_counter(live, value_margin, DAN_INFO::VALUE_COUNTER, 0, y);
            if (info.exam_type == "gauge")
                tex.draw_texture(DAN_INFO::EXAM_PERCENT, {.y = y, .index = 1});
        }
    }

    if (!all) {
        const SkinInfo* sp = tex.skin_entry("dan_exam_sub_row");
        float sub_pitch  = sp ? sp->y : 50.0f;
        float sub_margin = sp ? sp->x : 20.0f;
        for (int j = 0; j < 2; j++) {
            float sy = y + j * sub_pitch;
            tex.draw_texture(DAN_INFO::EXAM_SUB_BG,    {.y = sy});
            if (j >= info.song_count) continue;   // not reached yet: stub only
            tex.draw_texture(DAN_INFO::EXAM_SUB_TRACK, {.y = sy});
            const SkinInfo* sb = tex.skin_entry("dan_exam_sub_bar");
            float sub_full = sb ? sb->width : 234.0f;
            fill(info.song_state[j], sub_full * info.song_progress[j], sy, true);
            tex.draw_texture(DAN_INFO::EXAM_SUB_FRONT, {.y = sy});
            tex.draw_texture(DAN_INFO::EXAM_SUB_CHIP,  {.frame = j, .y = sy});
            draw_digit_counter(std::to_string(info.song_value[j]), sub_margin,
                               DAN_INFO::EXAM_SUB_COUNTER, 0, sy);
        }
    }
}

void DanGameScreen::draw_dan_info() {
    if (!dan_info_cache.has_value()) return;
    const DanInfoCache& cache = *dan_info_cache;
    const SessionData& sd = global_data.session_data[(int)global_data.player_num];

    tex.draw_texture(DAN_INFO::TOTAL_NOTES, {});

    float offset_y = dan_exam_info().y;
    const auto& exams = sd.selected_dan_exam;
    int slot = 0;
    for (int i = 0; i < (int)cache.exam_data.size(); i++) {
        if (i >= (int)exams.size()) break;
        if (exams[i].type == "gauge") continue;
        if (slot >= 3) break;
        draw_exam_row(cache.exam_data[i], exams[i], i, slot * offset_y);
        slot++;
    }

    if (sd.dan_rank >= 0 && tex.options[SCO::DAN_GAME_RANK_PLATE]) {
        tex.draw_texture(DAN_INFO::RANK_PLATE, {.frame = sd.dan_rank});
    } else {
        tex.draw_texture(DAN_INFO::FRAME, {.frame = dan_color});
        if (hori_name) {
            SkinInfo hn = tex.skin_config[SC::DAN_GAME_HORI_NAME];
            hori_name->draw({
                .x = hn.x - hori_name->width / 2.0f,
                .y = hn.y,
                .x2 = std::min(hori_name->width, hn.width) - hori_name->width
            });
        }
    }
}

void DanGameScreen::draw() {
    if (background.has_value()) background->draw_back();
    draw_dan_info();
    dan_gauge.draw();
    if (background.has_value()) background->draw_gauge(PlayerNum::P1);
    if (players.size() == 1)
        players[0]->draw(ms_from_start, 0, 184 * tex.screen_scale, mask_shader);
    between.draw(184 * tex.screen_scale);
    if (background.has_value()) background->draw_fore();
    draw_overlay();
}

void DanGameScreen::update_skip_dan() {
    if (skip_lane == PlayerNum::ALL) return;
    if (!skipped && !paused) poll_skip_dan();
    push_skip_state();
}

void DanGameScreen::poll_skip_dan() {
    size_t play_hits = 0;
    for (const auto& player : players)
        if (player) play_hits += player->input_log.size();
    if (play_hits != skip_play_hits) {
        skip_play_hits = play_hits;
        if (skip_count > 0 && skip_count < SKIP_HITS) {
            skip_count = 0;
            skip_last  = -1;
            skip_text.reset();
        }
    }

    int value = -1;
    if      (is_l_kat_pressed(skip_lane)) value = 1;
    else if (is_r_kat_pressed(skip_lane)) value = 0;
    while (is_l_kat_pressed(skip_lane) || is_r_kat_pressed(skip_lane)) {}
    if (value < 0) return;

    if (skip_count == 0) {
        skip_count = 1;
        skip_last  = value;
    } else if (value != skip_last) {
        skip_count++;
        skip_last = value;
    } else {
        return;
    }

    const bool skin_draws = background.has_value() && background->wants_skip();
    const SkinInfo* pos = skin_draws ? nullptr : tex.skin_entry("skip_counter");
    if (pos) {
        skip_text = std::make_unique<OutlinedText>(
            std::to_string(skip_count) + "/" + std::to_string(SKIP_HITS),
            (int)(pos->font_size > 0 ? pos->font_size : 40),
            ray::WHITE, ray::BLACK, false, 4);
    }

    if (skip_count >= SKIP_HITS) do_skip_dan();
}

void DanGameScreen::do_skip_dan() {
    skipped = true;
    const bool skin_draws = background.has_value() && background->wants_skip();
    const SkinInfo* used = skin_draws ? nullptr : tex.skin_entry("skip_used_text");
    if (used) {
        skip_text = std::make_unique<OutlinedText>(
            tex.skin_text("skip_used", global_data.config->general.language, ""),
            (int)(used->font_size > 0 ? used->font_size : 40),
            ray::WHITE, ray::BLACK, false, 4);
    } else {
        skip_text.reset();
    }
    spdlog::info("Dan enso skipped on song {} of {} at {} ms -- ending the whole course",
                 song_index + 1,
                 global_data.session_data[(int)global_data.player_num].selected_dan.size(),
                 ms_from_start);
    if (players.size() == 1 && players[0])
        players[0]->cut_to_end(ms_from_start, prev_good, prev_ok, prev_bad);
    trigger_fail_out(get_current_ms());
}
