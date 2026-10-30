#include "game_dan.h"

#include <algorithm>
#include <iterator>

#include "../libs/file/script.h"
#include "../libs/gfx/text.h"
#include "../libs/input/input.h"

void DanGameScreen::on_screen_start() {
    Screen::on_screen_start();
    reset_playback();
    start_delay = 4000.0;
    song_index = 0;
    prev_good = prev_ok = prev_bad = prev_drumroll = 0;

    JudgePos::X = tex.get_skin_config(SC::GAME_JUDGE_POS)->x;
    JudgePos::Y = tex.get_skin_config(SC::GAME_JUDGE_POS)->y;
    TJAParser::jpos_field_width = (tex.screen_width - JudgePos::X) / tex.screen_scale;

    init_dan();
    load_hitsounds();

    transition.emplace("", "", true);
    {
        const SessionData& _sd = global_data.session_data[static_cast<int>(global_data.player_num)];
        transition->set_dan(_sd.dan_color, _sd.song_title);
    }
    transition->start();

    result_transition = ResultTransition(PlayerNum::DAN);
    allnet_indicator = AllNetIcon();
}

void DanGameScreen::init_dan() {
    SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];

    if (sd.selected_dan.size() > 3) {
        spdlog::warn("DanGameScreen: dan course '{}' has {} songs, only the first 3 are supported; truncating",
                     sd.song_title, sd.selected_dan.size());
        sd.selected_dan.resize(3);
        if (sd.selected_dan_exam.size() > 3) {
            sd.selected_dan_exam.resize(3);
        }
    }

    total_notes = 0;
    song_note_counts.clear();
    song_stats.clear();
    prev_score = 0;
    for (const auto& entry : sd.selected_dan) {
        int count = 0;
        try {
            SongParser sp(entry.song_path);
            auto [notes, bm, be, bn] = sp.notes_to_position(entry.difficulty);
            for (const Note& n : notes.notes) {
                if (n.type >= NoteType::DON && n.type <= NoteType::KAT_L) {
                    count++;
                }
            }
            for (auto& sec : bm) {
                for (const Note& n : sec.notes) {
                    if (n.type >= NoteType::DON && n.type <= NoteType::KAT_L) {
                        count++;
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::warn("DanGameScreen::init_dan: failed to parse '{}': {}", entry.song_path.string(), e.what());
        }
        song_note_counts.push_back(count);
        total_notes += count;
    }
    if (total_notes == 0) {
        total_notes = 1;
    }

    dan_color = sd.dan_color;
    failed_out = false;
    failed_out_at = 0.0;
    between.stop();
    exam_failed.assign(sd.selected_dan_exam.size(), false);
    exam_song_failed.assign(sd.selected_dan_exam.size(), {false, false, false});
    dan_info_cache.reset();
    song_max_combo = 0;
    dan_gauge.emplace(Gauge::dan(sd.selected_dan, total_notes, global_data.player_num));

    // Create player for first song
    const auto& first = sd.selected_dan[0];
    sd.selected_difficulty = first.difficulty;
    parser.emplace(first.song_path, static_cast<int>(start_delay));
    load_song_audio(AudioLoadMode::Sync);

    players.clear();
    players.push_back(std::make_unique<Player>(parser, global_data.player_num, first.difficulty, false,
                                               get_player_modifiers(global_data.player_num),
                                               sd.dan_gen3 ? ScoreMethod::GEN3 : global_data.config->general.score_method));
    players[0]->set_is_dan(true);
    players[0]->gauge.reset();
    players[0]->dan_gauge = &dan_gauge.value();

    init_skip();

    bpm = parser->metadata.bpm;
    scene_preset = parser->metadata.scene_preset;
    background.emplace(global_data.player_num, bpm, "DAN");

    std::string title = sd.song_title;
    draw_seq.emplace(title);

    begin_song_presentation(first, 1);
}

void DanGameScreen::change_song() {
    SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];
    const auto& entry = sd.selected_dan[song_index];
    sd.selected_difficulty = entry.difficulty;

    stop_song_audio();

    parser.emplace(entry.song_path, static_cast<int>(start_delay));
    load_song_audio(AudioLoadMode::Sync);

    song_started = false;

    players[0]->reload_for_dan(parser, entry.difficulty);
    players[0]->dan_gauge = &dan_gauge.value();

    init_skip();

    begin_song_presentation(entry, song_index + 1);
}

void DanGameScreen::begin_song_presentation(const DanSongEntry& entry, int song_number) {
    const std::string& lang = global_data.config->general.language;
    current_song_title = localized_text(parser->metadata.title, lang, LocalizedTextFallback::REQUIRED_ENGLISH);
    std::string subtitle = localized_text(parser->metadata.subtitle, lang, LocalizedTextFallback::NONE);
    song_info = SongInfo(current_song_title, subtitle, parser->metadata.subtitle_full_display, entry.genre_index - 1,
                         song_number);

    between.start(get_current_ms(), current_song_title, subtitle, parser->metadata.subtitle_full_display);
    reset_song_clock(DanBetween::SONG_OPEN_MS);
}

void DanGameScreen::fill_unplayed_songs() {
    SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];
    const std::string& lang = global_data.config->general.language;

    for (size_t i = sd.dan_result_data.songs.size(); i < sd.selected_dan.size(); i++) {
        const DanSongEntry& entry = sd.selected_dan[i];
        DanResultSong res;
        res.genre_index = entry.genre_index;
        res.selected_difficulty = entry.difficulty;
        res.diff_level = entry.level;

        res.unreached = true;
        res.hidden = entry.hidden;
        try {
            SongParser sp(entry.song_path);
            const auto& titles = sp.metadata.title;
            res.song_title = localized_text(titles, lang, LocalizedTextFallback::ENGLISH_OR_FIRST);
        } catch (...) {
            spdlog::warn("Dan result: could not read {}", entry.song_path.string());
        }

        sd.dan_result_data.songs.push_back(res);
    }
}

const SkinInfo& DanGameScreen::dan_exam_info() {
    if (const SkinInfo* e = tex.get_skin_config(SC::GAME_DAN_EXAM_INFO)) {
        return *e;
    }
    return *tex.get_skin_config(SC::DAN_SELECT_EXAM_INFO);
}

int DanGameScreen::get_exam_progress(const Exam& exam) {
    Player* p = players[0].get();

    if (exam.type == "gauge") {
        return dan_gauge ? dan_gauge->get_percent() : 0;
    }
    if (exam.type == "judgeperfect") {
        return p->get_good();
    }
    if (exam.type == "judgegood") {
        return p->get_ok();
    }
    if (exam.type == "judgebad") {
        return p->get_bad();
    }
    if (exam.type == "hit") {
        return p->get_good() + p->get_ok() + p->get_total_drumroll();
    }
    if (exam.type == "score") {
        return p->get_score();
    }
    if (exam.type == "combo") {
        return p->get_max_combo();
    }
    if (exam.type == "renda") {
        return p->get_total_drumroll();
    }
    return 0;
}

int DanGameScreen::get_exam_progress_song(const Exam& exam, int song_idx) {
    Player* p = players[0].get();
    int good, ok, bad, drum, score;
    if (song_idx < std::ssize(song_stats)) {
        good = song_stats[song_idx].good;
        ok = song_stats[song_idx].ok;
        bad = song_stats[song_idx].bad;
        drum = song_stats[song_idx].drumroll;
        score = song_stats[song_idx].score;
    } else if (song_idx == song_index) {
        good = p->get_good() - prev_good;
        ok = p->get_ok() - prev_ok;
        bad = p->get_bad() - prev_bad;
        drum = p->get_total_drumroll() - prev_drumroll;
        score = p->get_score() - prev_score;
    } else {
        return 0; // not played yet
    }
    if (exam.type == "judgeperfect") {
        return good;
    }
    if (exam.type == "judgegood") {
        return ok;
    }
    if (exam.type == "judgebad") {
        return bad;
    }
    if (exam.type == "hit") {
        return good + ok + drum;
    }
    if (exam.type == "score") {
        return score;
    }
    if (exam.type == "renda") {
        return drum;
    }
    if (exam.type == "combo") {
        if (song_idx < std::ssize(song_stats)) {
            return song_stats[song_idx].max_combo;
        }
        if (song_idx == song_index) {
            return song_max_combo;
        }
        return 0;
    }
    return get_exam_progress(exam); // gauge (course-wide by nature)
}

Exam DanGameScreen::exam_for(const Exam& exam, int song_idx) const {
    Exam ex = song_idx >= 0 ? exam.for_song(song_idx) : exam;
    if (ex.gold != Exam::GOLD_FULL) {
        return ex;
    }
    if (ex.range == "less") {
        ex.gold = 1; // gold = none at all (value < 1)
    } else if (ex.type == "gauge") {
        ex.gold = 100;
    } else if (ex.type == "judgeperfect" || ex.type == "hit" || ex.type == "combo") {
        int notes = total_notes;
        if (song_idx >= 0 && song_idx < std::ssize(song_note_counts)) {
            notes = song_note_counts[song_idx];
        }
        ex.gold = std::max(ex.red, notes); // every note
    } else {
        ex.gold = ex.red; // score / renda: no perfect value, no gold tier
    }
    return ex;
}

DanInfoCache DanGameScreen::calculate_dan_info() {
    DanInfoCache cache;
    Player* p = players[0].get();
    int used = p->get_good() + p->get_ok() + p->get_bad();
    cache.remaining_notes = std::max(0, total_notes - used);
    const bool near_end = total_notes * 0.10f > cache.remaining_notes;
    const bool just_before_end = total_notes * 0.05f > cache.remaining_notes;

    const auto& exams = global_data.session_data[static_cast<int>(global_data.player_num)].selected_dan_exam;
    for (const auto& exam : exams) {
        DanExamInfo info;
        info.exam_type = exam.type;
        info.exam_range = exam.range;
        info.red_value = exam.gothrough ? exam.red : exam_for(exam, std::min(song_index, 2)).red;

        int val = get_exam_progress(exam);
        float progress = (exam.red > 0) ? static_cast<float>(val) / exam.red : 0.0f;

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

        if (progress >= 1.0f) {
            info.bar_texture = "exam_max";
        } else if (progress >= 0.5f) {
            info.bar_texture = "exam_gold";
        } else {
            info.bar_texture = "exam_red";
        }
        info.bar_state = dan_bar_state(exam_for(exam, -1), val, true, near_end, just_before_end);

        info.gothrough = exam.gothrough;
        info.song_count = std::min(song_index, 3);
        if (!exam.gothrough) {
            for (int j = 0; j < 3 && j <= song_index; j++) {
                const Exam ex = exam_for(exam, j);
                int sv = get_exam_progress_song(exam, j);
                float sp = (ex.red > 0) ? static_cast<float>(sv) / ex.red : 0.0f;
                if (ex.range == "less") {
                    sp = 1.0f - sp;
                    sv = std::max(0, ex.red - sv);
                }
                info.song_value[j] = std::max(0, sv);
                info.song_progress[j] = std::max(0.0f, std::min(1.0f, sp));
                const bool live_j = (j == song_index);
                info.song_state[j] =
                    dan_bar_state(ex, get_exam_progress_song(exam, j), live_j, near_end, just_before_end);
            }
            int cur = std::min(song_index, 2);
            info.counter_value = info.song_value[cur];
            info.progress = info.song_progress[cur];
            info.bar_width = bar_full_w * info.progress;
            if (info.progress >= 1.0f) {
                info.bar_texture = "exam_max";
            } else if (info.progress >= 0.5f) {
                info.bar_texture = "exam_gold";
            } else {
                info.bar_texture = "exam_red";
            }
            info.bar_state = info.song_state[cur];
        }

        cache.exam_data.push_back(info);
    }
    return cache;
}

void DanGameScreen::check_exam_failures(bool course_finished, bool song_finished) {
    if (!dan_info_cache.has_value()) {
        return;
    }
    const auto& exams = global_data.session_data[static_cast<int>(global_data.player_num)].selected_dan_exam;
    for (int i = 0; i < std::ssize(exams); i++) {
        if (exam_failed[i]) {
            continue;
        }
        const Exam& base = exams[i];
        const Exam exam = base.gothrough ? exam_for(base, -1) : exam_for(base, std::min(song_index, 2));
        int val = exam.gothrough ? get_exam_progress(exam) : get_exam_progress_song(exam, song_index);
        bool at_boundary = exam.gothrough ? course_finished : (song_finished || course_finished);

        if (exam.range == "more" && !at_boundary) {
            continue;
        }

        if (exam.range == "more" && val < exam.red) {
            exam_failed[i] = true;
            if (!exam.gothrough && i < std::ssize(exam_song_failed) && song_index < 3) {
                exam_song_failed[i][song_index] = true;
            }
            audio.play_sound("exam_failed", VolumePreset::SOUND);
            spdlog::info("Dan exam {} ({}) failed: {} < {} ({}) at {} ms (song ends {} ms)", i, exam.type, val,
                         exam.red, exam.gothrough ? "gothrough" : "per-song", clock.position(),
                         players.empty() || !players[0] ? -1.0 : players[0]->end_time);
        } else if (exam.range == "less") {
            int remaining = std::max(0, exam.red - val);
            if (remaining == 0) {
                exam_failed[i] = true;
                if (!exam.gothrough && i < std::ssize(exam_song_failed) && song_index < 3) {
                    exam_song_failed[i][song_index] = true;
                }
                audio.play_sound("dan_failed", VolumePreset::SOUND);
                spdlog::info("Dan exam {} ({}) failed: {} of {} used up ({}) at {} ms (song ends {} ms)", i, exam.type,
                             val, exam.red, exam.gothrough ? "gothrough" : "per-song", clock.position(),
                             players.empty() || !players[0] ? -1.0 : players[0]->end_time);
            }
        }
    }
}

int DanGameScreen::exam_tier(const Exam& exam, int value) {
    const bool has_gold = exam.gold > 0 && exam.gold != exam.red;
    if (exam.range == "less") {
        if (value >= exam.red) {
            return 0;
        }
        return has_gold && value < exam.gold ? 2 : 1;
    }
    if (value < exam.red) {
        return 0;
    }
    return has_gold && value >= exam.gold ? 2 : 1;
}

void DanGameScreen::save_song_score(DanResultSong& song_res) {
    if (players[0]->is_auto_play() || players[0]->is_replay()) {
        return;
    }
    const SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];
    const DanSongEntry& entry = sd.selected_dan[song_index];
    const std::string hash = dan_song_hash(entry);
    if (hash.empty()) {
        return;
    }

    Score score;
    score.score = players[0]->get_score() - prev_score;
    score.good = song_res.good;
    score.ok = song_res.ok;
    score.bad = song_res.bad;
    score.drumroll = song_res.drumroll;
    score.max_combo = song_max_combo;

    if (skipped) {
        score.crown = Crown::NONE;
    } else if (score.ok == 0 && score.bad == 0) {
        score.crown = Crown::DFC;
    } else if (score.bad == 0) {
        score.crown = Crown::FC;
    } else if (!failed_out) {
        score.crown = Crown::CLEAR;
    } else {
        score.crown = Crown::NONE;
    }
    score.rank = skipped ? Rank::_NONE : rank_for_score(score.score);
    const std::string player_id = get_player_id(global_data.player_num);
    const int64_t played_at = unix_now();
    const std::string modifiers_json = modifiers_to_json(players[0]->get_modifiers());
    scores_manager.save_score(hash, entry.difficulty, player_id, score, played_at, modifiers_json, &song_res.score_id);

    if (global_data.config->general.score_method != ScoreMethod::GEN3 && !sd.dan_gen3) {
        std::string access_code;
        if (player_id == scores_manager.player_1) {
            access_code = global_data.config->network.access_code_1;
        } else if (player_id == scores_manager.player_2) {
            access_code = global_data.config->network.access_code_2;
        }
        if (!access_code.empty()) {
            PlayerData pd = scores_manager.get_player_data(player_id).value_or(PlayerData{});
            network.submit_score(hash, entry.difficulty, access_code, score, players[0]->input_log, played_at,
                                 modifiers_json, pd.chara_is_costume, pd.chara_cos_index);
        }
    }
}

void DanGameScreen::save_result_data(bool all_failed) {
    SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];
    const int course_songs = std::min(static_cast<int>(sd.selected_dan.size()), 3);

    sd.dan_result_data.dan_color = dan_color;
    sd.dan_result_data.dan_rank = sd.dan_rank;
    sd.dan_result_data.dan_index = sd.dan_index;
    sd.dan_result_data.dan_index_max = sd.dan_index_max;
    sd.dan_result_data.is_gaiden = sd.dan_gaiden;
    sd.dan_result_data.dan_title = sd.song_title;
    sd.dan_result_data.score = players[0]->get_score();
    if (dan_gauge) {
        sd.dan_result_data.gauge_length = dan_gauge->get_length();
    }
    sd.dan_result_data.max_combo = players[0]->get_max_combo();
    sd.dan_result_data.exams = sd.selected_dan_exam;

    sd.dan_result_data.exam_data.clear();
    int check = 2; // min tier over every exam
    const auto& exams = sd.selected_dan_exam;
    if (dan_info_cache.has_value()) {
        for (int i = 0; i < std::ssize(dan_info_cache->exam_data); i++) {
            DanResultExam re;
            re.progress = dan_info_cache->exam_data[i].progress;
            re.counter_value = dan_info_cache->exam_data[i].counter_value;
            re.bar_texture = dan_info_cache->exam_data[i].bar_texture;
            re.bar_state = dan_info_cache->exam_data[i].bar_state;
            for (int j = 0; j < 3; j++) {
                re.song_state[j] = dan_info_cache->exam_data[i].song_state[j];
            }
            re.failed = i < std::ssize(exam_failed) && exam_failed[i];
            re.song_count = dan_info_cache->exam_data[i].song_count;
            for (int j = 0; j < 3; j++) {
                re.song_value[j] = dan_info_cache->exam_data[i].song_value[j];
                re.song_progress[j] = dan_info_cache->exam_data[i].song_progress[j];
            }
            if (i < std::ssize(exams)) {
                const Exam& exam = exams[i];
                if (all_failed) {
                    re.tier = 0;
                } else if (exam.gothrough) {
                    re.tier = exam_tier(exam_for(exam, -1), get_exam_progress(exam));
                } else {
                    re.tier = 2;
                    for (int j = 0; j < course_songs; j++) {
                        re.tier = std::min(re.tier, exam_tier(exam_for(exam, j), get_exam_progress_song(exam, j)));
                    }
                }
                if (re.tier == 0) {
                    re.failed = true;
                }
                re.bar_state = dan_bar_state(exam_for(exam, -1), get_exam_progress(exam));
                if (!exam.gothrough) {
                    for (int j = 0; j < course_songs; j++) {
                        re.song_state[j] = dan_bar_state(exam_for(exam, j), get_exam_progress_song(exam, j));
                    }
                }
            }
            check = std::min(check, re.tier);
            sd.dan_result_data.exam_data.push_back(re);
        }
    }
    if (exams.empty()) {
        check = 0; // a course with no exams cannot pass
    }

    fill_unplayed_songs();

    int total_ok = 0, total_bad = 0;
    for (const auto& s : sd.dan_result_data.songs) {
        total_ok += s.ok;
        total_bad += s.bad;
    }

    int result;
    if (check == 2) {
        result = total_bad == 0 ? (total_ok == 0 ? 6 : 5) : 4;
    } else if (check == 1) {
        result = total_bad == 0 ? (total_ok == 0 ? 3 : 2) : 1;
    } else {
        result = 0;
    }
    if (skipped) {
        result = 0;
    }
    sd.dan_result_data.odai_result = result;
    sd.dan_result_data.skipped = skipped;
    spdlog::info("Dan course verdict: check={} ok={} bad={} odai_result={}{}", check, total_ok, total_bad, result,
                 all_failed ? " (fail-out)" : "");
}

void DanGameScreen::trigger_fail_out(double current_ms) {
    failed_out = true;
    failed_out_at = current_ms;
    if (song_music) {
        audio.stop_sound(*song_music);
        song_music.reset();
    }
    if (movie.has_value()) {
        movie->stop();
    }
    between.stop();

    SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];
    if (std::ssize(sd.dan_result_data.songs) <= song_index) {
        DanResultSong song_res;
        song_res.song_title = current_song_title;
        song_res.genre_index = sd.selected_dan[song_index].genre_index;
        song_res.selected_difficulty = sd.selected_dan[song_index].difficulty;
        song_res.diff_level = sd.selected_dan[song_index].level;
        song_res.good = players[0]->get_good() - prev_good;
        song_res.ok = players[0]->get_ok() - prev_ok;
        song_res.bad = players[0]->get_bad() - prev_bad;
        song_res.drumroll = players[0]->get_total_drumroll() - prev_drumroll;
        sd.dan_result_data.songs.push_back(song_res);
    }
    const bool reached_every_song = song_index >= std::ssize(sd.selected_dan) - 1;
    save_result_data(!reached_every_song);
    score_saved = true;
    spdlog::info("Dan course ended early ({}): song {}/{} at {} ms", skipped ? "skip" : "fail-out", song_index + 1,
                 sd.selected_dan.size(), clock.position());
}

Screens DanGameScreen::on_screen_end(Screens next_screen) {
    dan_info_cache.reset();
    draw_seq.reset();
    between.stop();
    return GameScreen::on_screen_end(next_screen);
}

std::optional<Screens> DanGameScreen::update() {
    if (auto init = Screen::update()) {
        return init;
    }

    double current_ms = get_current_ms();
    allnet_indicator.update(current_ms);

    transition->update(current_ms);
    between.update(current_ms);
    clock.update(current_ms);

    if (transition->is_finished()) {
        reset_input_lock(); // same as GameScreen; locks left over from the previous screen
    }
    if (transition->is_finished() && between.song_may_start()) {
        start_song(clock.position());
    }

    update_background(current_ms);

    resync_song(current_ms);

    if (!failed_out) {
        players[0]->update(clock.position(), current_ms, background);
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

    if (result_transition.is_finished && !audio.is_sound_playing("dan_transition")) {
        return on_screen_end(Screens::DAN_RESULT);
    }

    SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];

    if (!failed_out && clock.position() >= players[0]->end_time) {
        check_exam_failures(false, true);
        {
            bool boundary_failed = std::ranges::any_of(exam_failed, [](bool f) { return f; });
            bool is_final_song = song_index >= std::ssize(sd.selected_dan) - 1;
            if (boundary_failed && !is_final_song) {
                spdlog::info("Dan course fails out at the END of song {}/{} "
                             "(state 6): song was played to its natural end at {} ms",
                             song_index + 1, sd.selected_dan.size(), clock.position());
                trigger_fail_out(current_ms);
                return std::nullopt;
            }
        }

        // Save per-song result if not yet saved for this song
        if (std::ssize(sd.dan_result_data.songs) <= song_index) {
            DanResultSong song_res;
            song_res.song_title = current_song_title;
            song_res.genre_index = sd.selected_dan[song_index].genre_index;
            song_res.selected_difficulty = sd.selected_dan[song_index].difficulty;
            song_res.diff_level = sd.selected_dan[song_index].level;
            song_res.good = players[0]->get_good() - prev_good;
            song_res.ok = players[0]->get_ok() - prev_ok;
            song_res.bad = players[0]->get_bad() - prev_bad;
            song_res.drumroll = players[0]->get_total_drumroll() - prev_drumroll;
            save_song_score(song_res);
            players[0]->input_log.clear(); // each song is submitted with only its own inputs
            sd.dan_result_data.songs.push_back(song_res);
        }

        bool any_failed = std::ranges::any_of(exam_failed, [](bool failed) { return failed; });
        bool is_last = (song_index == std::ssize(sd.selected_dan) - 1) || any_failed;
        if (is_last) {
            if (clock.position() >= players[0]->end_time + 1000 && !score_saved) {
                check_exam_failures(true, true);
                save_result_data(false);
                players[0]->spawn_ending_anim(background.has_value() ? &*background : nullptr);
                score_saved = true;
            }
            constexpr double SKIP_RESULT_DELAY = 2800.0;
            double transition_delay = players[0]->was_skipped() ? SKIP_RESULT_DELAY : 8533.34;
            if (clock.position() >= players[0]->end_time + transition_delay && !result_transition.is_started) {
                result_transition.start();
                audio.play_sound("dan_transition", VolumePreset::VOICE);
            }
        } else {
            SongStats st;
            st.good = players[0]->get_good() - prev_good;
            st.ok = players[0]->get_ok() - prev_ok;
            st.bad = players[0]->get_bad() - prev_bad;
            st.drumroll = players[0]->get_total_drumroll() - prev_drumroll;
            st.score = players[0]->get_score() - prev_score;
            st.max_combo = song_max_combo;
            if (std::ssize(song_stats) <= song_index) {
                song_stats.push_back(st);
            }

            // The cabinet lets the song's audio play out before the next one starts; the
            // chart's last note is only where judging ends. Wait for the music to stop
            // (capped, in case a file has a long silent tail).
            constexpr double MAX_TAIL_MS = 20000.0;
            const bool music_running = song_music && audio.is_sound_playing(*song_music);
            if (music_running && clock.position() < players[0]->end_time + MAX_TAIL_MS) {
                return std::nullopt;
            }
            song_max_combo = 0;

            prev_good = players[0]->get_good();
            prev_ok = players[0]->get_ok();
            prev_bad = players[0]->get_bad();
            prev_drumroll = players[0]->get_total_drumroll();
            prev_score = players[0]->get_score();

            song_index++;
            song_started = false;
            change_song();
        }
    }

    // Global keys (back / restart)
    if (check_key_pressed(global_data.config->keys.back_key)) {
        return on_screen_end(Screens::DAN_SELECT);
    }
    if (check_key_pressed(global_data.config->keys.restart_key)) {
        unload_song();
        song_index = 0;
        prev_good = prev_ok = prev_bad = prev_drumroll = prev_score = 0;
        song_stats.clear();
        sd.dan_result_data = DanResultData();
        result_transition = ResultTransition(PlayerNum::DAN);
        init_dan();
        audio.play_sound("restart", VolumePreset::SOUND);
    }

    return std::nullopt;
}

void DanGameScreen::push_dan_state() {
    if (!background.has_value() || !background->wants_dan()) {
        return;
    }
    if (!dan_info_cache.has_value()) {
        return;
    }

    sol::state& lua = *script_manager.lua;
    sol::table st = lua.create_table();
    const SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];

    st["dan_color"] = dan_color;
    st["dan_title"] = sd.song_title;
    st["song_index"] = song_index;
    st["song_count"] = static_cast<int>(sd.selected_dan.size());
    st["total_notes"] = total_notes;
    st["remaining_notes"] = dan_info_cache->remaining_notes;
    st["gauge"] = dan_gauge ? dan_gauge->get_length() : 0.0f; // 0-100%
    st["gauge_max"] = 100.0f;

    sol::table rows = lua.create_table();
    const auto& exams = sd.selected_dan_exam;
    for (int i = 0; i < std::ssize(dan_info_cache->exam_data); i++) {
        const DanExamInfo& info = dan_info_cache->exam_data[i];
        sol::table row = lua.create_table();
        row["type"] = info.exam_type;
        row["range"] = info.exam_range;
        row["red"] = info.red_value;
        row["gold"] =
            (i < std::ssize(exams)) ? exam_for(exams[i], exams[i].gothrough ? -1 : std::min(song_index, 2)).gold : 0;
        if (i < std::ssize(exams) && exams[i].per_song()) {
            sol::table sr = lua.create_table(), sg = lua.create_table();
            for (int j = 0; j < std::ssize(exams[i].song_red); j++) {
                sr[j + 1] = exams[i].song_red[j];
                sg[j + 1] = exam_for(exams[i], j).gold;
            }
            row["song_red"] = sr;
            row["song_gold"] = sg;
        }
        row["value"] = info.counter_value;
        row["progress"] = info.progress;
        row["bar"] = info.bar_texture;
        row["failed"] = (i < std::ssize(exam_failed)) ? exam_failed[i] : false;
        rows[i + 1] = row;
    }
    st["exams"] = rows;

    background->handle_dan(global_data.player_num, st);
}

void DanGameScreen::draw() {
    if (background.has_value()) {
        background->draw_back();
    }
    if (dan_info_cache.has_value() && draw_seq.has_value()) {
        const SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];
        draw_seq->draw(*dan_info_cache, exam_failed, song_index, dan_color, sd.dan_rank, get_frame_ms());
    }
    if (dan_gauge) {
        dan_gauge->draw();
    }
    if (background.has_value()) {
        background->draw_gauge(PlayerNum::P1);
    }
    if (players.size() == 1) {
        players[0]->draw(clock.position(), 0, 184 * tex.screen_scale);
    }
    between.draw(184 * tex.screen_scale);
    if (background.has_value()) {
        background->draw_fore();
    }
    draw_overlay();
}

void DanGameScreen::update_skip_dan() {
    if (skip_lane == PlayerNum::ALL) {
        return;
    }
    if (!skipped && !clock.is_paused()) {
        poll_skip_dan();
    }
    push_skip_state();
}

void DanGameScreen::poll_skip_dan() {
    size_t play_hits = 0;
    for (const auto& player : players) {
        if (player) {
            play_hits += player->input_log.size();
        }
    }
    if (play_hits != skip_play_hits) {
        skip_play_hits = play_hits;
        if (skip_count > 0 && skip_count < SKIP_HITS) {
            skip_count = 0;
            skip_last = -1;
            skip_text.reset();
        }
    }

    int value = -1;
    if (is_l_kat_pressed(skip_lane)) {
        value = 1;
    } else if (is_r_kat_pressed(skip_lane)) {
        value = 0;
    }
    while (is_l_kat_pressed(skip_lane) || is_r_kat_pressed(skip_lane)) {
    }
    if (value < 0) {
        return;
    }

    if (skip_count == 0) {
        skip_count = 1;
        skip_last = value;
    } else if (value != skip_last) {
        skip_count++;
        skip_last = value;
    } else {
        return;
    }

    const bool skin_draws = background.has_value() && background->wants_skip();
    const SkinInfo* pos = skin_draws ? nullptr : tex.get_skin_config(SC::SKIP_COUNTER);
    if (pos) {
        skip_text = std::make_unique<OutlinedText>(std::to_string(skip_count) + "/" + std::to_string(SKIP_HITS),
                                                   (pos->font_size > 0 ? pos->font_size : 40), Color::WHITE,
                                                   Color::BLACK, false, 4);
    }

    if (skip_count >= SKIP_HITS) {
        do_skip_dan();
    }
}

void DanGameScreen::do_skip_dan() {
    skipped = true;
    const bool skin_draws = background.has_value() && background->wants_skip();
    const SkinInfo* used = skin_draws ? nullptr : tex.get_skin_config(SC::SKIP_USED_TEXT);
    if (used) {
        skip_text = std::make_unique<OutlinedText>(tex.skin_text("skip_used", global_data.config->general.language, ""),
                                                   (used->font_size > 0 ? used->font_size : 40), Color::WHITE,
                                                   Color::BLACK, false, 4);
    } else {
        skip_text.reset();
    }
    spdlog::info("Dan enso skipped on song {} of {} at {} ms -- ending the whole course", song_index + 1,
                 global_data.session_data[static_cast<int>(global_data.player_num)].selected_dan.size(),
                 clock.position());
    if (players.size() == 1 && players[0]) {
        players[0]->cut_to_end(clock.position(), prev_good, prev_ok, prev_bad);
    }
    trigger_fail_out(get_current_ms());
}
