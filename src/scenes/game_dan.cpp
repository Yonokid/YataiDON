#include "game_dan.h"
#include <spdlog/spdlog.h>

void DanGameScreen::on_screen_start() {
    // Call the Screen base (loads textures/sounds) but NOT GameScreen::on_screen_start
    Screen::on_screen_start();
    mask_shader   = ray::LoadShader("shader/dummy.vs", "shader/mask.fs");
    current_ms    = 0;
    end_ms        = 0;
    start_ms      = 0;
    start_delay   = 4000.0;
    song_started  = false;
    paused        = false;
    score_saved   = false;
    pause_time    = 0;
    song_index    = 0;
    prev_good = prev_ok = prev_bad = prev_drumroll = 0;

    JudgePos::X = 414 * tex.screen_scale;
    JudgePos::Y = 256 * tex.screen_scale;

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
    transition->start();
    dan_transition = DanTransition();
    dan_transition.start();

    result_transition = ResultTransition(PlayerNum::DAN);
    allnet_indicator  = AllNetIcon();
}

void DanGameScreen::init_dan() {
    SessionData& sd = global_data.session_data[(int)global_data.player_num];

    // Count total notes across all songs / 2 (halved per original game logic)
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
    exam_failed.assign(sd.selected_dan_exam.size(), false);
    dan_info_cache.reset();

    // Set up gauge
    dan_gauge = DanGauge(global_data.player_num, total_notes);

    // Create player for first song
    const auto& first = sd.selected_dan[0];
    sd.selected_difficulty = first.difficulty;
    parser.emplace(first.song_path, (int)start_delay);
    if (fs::exists(parser->metadata.wave))
        song_music = audio->load_sound(parser->metadata.wave, "song");

    players.clear();
    players.push_back(std::make_unique<Player>(
        parser, global_data.player_num, first.difficulty, false,
        global_data.modifiers[(int)global_data.player_num]));
    players[0]->set_is_dan(true);
    players[0]->gauge.reset();
    players[0]->dan_gauge = &dan_gauge;

    bpm        = parser->metadata.bpm;
    scene_preset = parser->metadata.scene_preset;
    background.emplace(global_data.player_num, bpm, "DAN");

    const std::string& lang = global_data.config->general.language;
    std::string title = sd.song_title;
    hori_name = std::make_unique<OutlinedText>(title, tex.skin_config[SC::DAN_TITLE].font_size, ray::WHITE, ray::BLACK, false);

    current_song_title = parser->metadata.title.count(lang) ? parser->metadata.title.at(lang) : parser->metadata.title.at("en");
    song_info = SongInfo(current_song_title, first.genre_index);

    start_ms = get_current_ms() - parser->metadata.offset * 1000;
}

void DanGameScreen::change_song() {
    SessionData& sd = global_data.session_data[(int)global_data.player_num];
    const auto& entry = sd.selected_dan[song_index];
    sd.selected_difficulty = entry.difficulty;

    if (song_music.has_value()) {
        audio->stop_sound(song_music.value());
        song_music.reset();
    }

    parser.emplace(entry.song_path, (int)start_delay);
    if (fs::exists(parser->metadata.wave))
        song_music = audio->load_sound(parser->metadata.wave, "song");

    song_started = false;

    players[0]->reload_for_dan(parser, entry.difficulty);
    players[0]->dan_gauge = &dan_gauge;

    const std::string& lang = global_data.config->general.language;
    current_song_title = parser->metadata.title.count(lang) ? parser->metadata.title.at(lang) : parser->metadata.title.at("en");
    song_info = SongInfo(current_song_title, entry.genre_index);

    dan_transition.start();
    start_ms = get_current_ms() - parser->metadata.offset * 1000;
}

int DanGameScreen::get_exam_progress(const Exam& exam) {
    Player* p = players[0].get();
    float gauge_pct = (dan_gauge.gauge_max > 0)
        ? (dan_gauge.gauge_length / dan_gauge.gauge_max) * 100.0f : 0.0f;

    static const std::unordered_map<std::string, std::function<int()>> map = {
        {"gauge",        [&]{ return (int)gauge_pct; }},
        {"judgeperfect", [&]{ return p->get_good(); }},
        {"judgegood",    [&]{ return p->get_ok() + p->get_bad(); }},
        {"judgebad",     [&]{ return p->get_bad(); }},
        {"hit",          [&]{ return p->get_good() + p->get_ok() + p->get_total_drumroll(); }},
        {"score",        [&]{ return p->get_score(); }},
        {"combo",        [&]{ return p->get_max_combo(); }},
    };
    auto it = map.find(exam.type);
    return it != map.end() ? it->second() : 0;
}

DanInfoCache DanGameScreen::calculate_dan_info() {
    DanInfoCache cache;
    Player* p = players[0].get();
    int used = p->get_good() + p->get_ok() + p->get_bad();
    cache.remaining_notes = std::max(0, total_notes - used);

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

        float bar_full_w = tex.skin_config[SC::DAN_EXAM_INFO].width;
        info.bar_width = bar_full_w * progress;

        if (progress >= 1.0f)        info.bar_texture = "exam_max";
        else if (progress >= 0.5f)   info.bar_texture = "exam_gold";
        else                          info.bar_texture = "exam_red";

        cache.exam_data.push_back(info);
    }
    return cache;
}

void DanGameScreen::check_exam_failures() {
    if (!dan_info_cache.has_value()) return;
    const auto& exams = global_data.session_data[(int)global_data.player_num].selected_dan_exam;
    for (int i = 0; i < (int)exams.size(); i++) {
        if (exam_failed[i]) continue;
        const Exam& exam = exams[i];
        int val = get_exam_progress(exam);

        if (exam.range == "more" && end_ms != 0 && val < exam.red) {
            exam_failed[i] = true;
            audio->play_sound("exam_failed", "sound");
            spdlog::info("Dan exam {} ({}) failed: {} < {}", i, exam.type, val, exam.red);
        } else if (exam.range == "less") {
            int remaining = std::max(0, exam.red - val);
            if (remaining == 0) {
                exam_failed[i] = true;
                audio->play_sound("dan_failed", "sound");
                spdlog::info("Dan exam {} ({}) failed: limit reached", i, exam.type);
            }
        }
    }
}

Screens DanGameScreen::on_screen_end(Screens next_screen) {
    dan_info_cache.reset();
    hori_name.reset();
    return GameScreen::on_screen_end(next_screen);
}

std::optional<Screens> DanGameScreen::update() {
    Screen::update();
    double current_time = get_current_ms();

    transition->update(current_time);
    current_ms = current_time - start_ms;
    dan_transition.update(current_time);

    if (transition->is_finished() && dan_transition.is_finished()) {
        start_song(current_ms);
    } else {
        start_ms = current_time - parser->metadata.offset * 1000;
    }

    update_background(current_time);

    if (song_music.has_value() && audio->is_sound_playing(song_music.value())) {
        double audio_ms = audio->get_sound_time_played(song_music.value()) * 1000.0;
        double adj = audio_ms + (parser->metadata.offset * 1000 + start_delay
                                  - (double)global_data.config->general.audio_offset);
        if (std::abs(current_ms - adj) > Timing::GOOD) {
            current_ms = adj;
            start_ms   = current_time - current_ms;
        }
    }

    players[0]->update(current_ms, current_time, background);
    song_info.update(current_time);
    result_transition.update(current_time);

    dan_info_cache = calculate_dan_info();
    check_exam_failures();

    // Result transition finished → go to DAN_RESULT
    if (result_transition.is_finished && !audio->is_sound_playing("dan_transition")) {
        spdlog::info("DanGameScreen: moving to DAN_RESULT");
        return on_screen_end(Screens::DAN_RESULT);
    }

    SessionData& sd = global_data.session_data[(int)global_data.player_num];

    if (current_ms >= players[0]->end_time + 1000) {
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

        bool is_last = (song_index == (int)sd.selected_dan.size() - 1);
        if (is_last) {
            if (end_ms != 0) {
                if (current_time >= end_ms + 1000 && !score_saved) {
                    sd.dan_result_data.dan_color   = dan_color;
                    sd.dan_result_data.dan_title   = sd.song_title;
                    sd.dan_result_data.score       = players[0]->get_score();
                    sd.dan_result_data.gauge_length= dan_gauge.gauge_length;
                    sd.dan_result_data.max_combo   = players[0]->get_max_combo();
                    sd.dan_result_data.exams       = sd.selected_dan_exam;
                    sd.dan_result_data.exam_data.clear();
                    if (dan_info_cache.has_value()) {
                        for (int i = 0; i < (int)dan_info_cache->exam_data.size(); i++) {
                            DanResultExam re;
                            re.progress      = dan_info_cache->exam_data[i].progress;
                            re.counter_value = dan_info_cache->exam_data[i].counter_value;
                            re.bar_texture   = dan_info_cache->exam_data[i].bar_texture;
                            re.failed        = exam_failed[i];
                            sd.dan_result_data.exam_data.push_back(re);
                        }
                    }
                    players[0]->spawn_ending_anim();
                    score_saved = true;
                }
                if (current_time >= end_ms + 8533.34 && !result_transition.is_started) {
                    result_transition.start();
                    audio->play_sound("dan_transition", "voice");
                    spdlog::info("DanGameScreen: result transition started");
                }
            } else {
                end_ms = current_time;
            }
        } else {
            // Advance to next song
            prev_good     = players[0]->get_good();
            prev_ok       = players[0]->get_ok();
            prev_bad      = players[0]->get_bad();
            prev_drumroll = players[0]->get_total_drumroll();

            song_index++;
            end_ms       = 0;
            song_started = false;
            change_song();
        }
    }

    // Global keys (back / restart)
    if (check_key_pressed(global_data.config->keys.back_key)) {
        if (song_music.has_value()) audio->stop_sound(song_music.value());
        return on_screen_end(Screens::DAN_SELECT);
    }
    if (check_key_pressed(global_data.config->keys.restart_key)) {
        if (song_music.has_value()) { audio->stop_sound(song_music.value()); song_music.reset(); }
        song_index = 0;
        prev_good = prev_ok = prev_bad = prev_drumroll = 0;
        sd.dan_result_data = DanResultData();
        init_dan();
        audio->play_sound("restart", "sound");
    }

    return std::nullopt;
}

void DanGameScreen::draw_digit_counter(const std::string& digits, float margin_x, TexID tex_id, int index, float y, float x_offset) {
    for (int j = 0; j < (int)digits.size(); j++) {
        float x = -(float)(digits.size() - j) * margin_x + x_offset;
        tex.draw_texture(tex_id, {.frame=digits[j]-'0', .x=x, .y=y, .index=index});
    }
}

void DanGameScreen::draw_dan_info() {
    if (!dan_info_cache.has_value()) return;
    const DanInfoCache& cache = *dan_info_cache;

    // Total remaining notes
    tex.draw_texture(DAN_INFO::TOTAL_NOTES, {});
    std::string rem = std::to_string(cache.remaining_notes);
    draw_digit_counter(rem, tex.skin_config[SC::DAN_TOTAL_NOTES_MARGIN].x,
                       DAN_INFO::TOTAL_NOTES_COUNTER, 0, 0);

    // Per-exam bars
    float offset_y = tex.skin_config[SC::DAN_EXAM_INFO].y;
    float score_margin = tex.skin_config[SC::DAN_SCORE_BOX_MARGIN].x;

    for (int i = 0; i < (int)cache.exam_data.size(); i++) {
        const DanExamInfo& info = cache.exam_data[i];
        float y = i * offset_y;

        tex.draw_texture(DAN_INFO::EXAM_BG,       {.y=y});
        tex.draw_texture(DAN_INFO::EXAM_OVERLAY_1,{.y=y});

        // Bar texture selection
        static const std::unordered_map<std::string, TexID> bar_ids = {
            {"exam_red",  DAN_INFO::EXAM_RED},
            {"exam_gold", DAN_INFO::EXAM_GOLD},
            {"exam_max",  DAN_INFO::EXAM_MAX},
        };
        auto bar_it = bar_ids.find(info.bar_texture);
        if (bar_it != bar_ids.end())
            tex.draw_texture(bar_it->second, {.y=y, .x2=info.bar_width});

        // Exam type icon
        static const std::unordered_map<std::string, TexID> exam_ids = {
            {"gauge",        DAN_INFO::EXAM_GAUGE},
            {"combo",        DAN_INFO::EXAM_COMBO},
            {"hit",          DAN_INFO::EXAM_HIT},
            {"judgebad",     DAN_INFO::EXAM_JUDGEBAD},
            {"judgegood",    DAN_INFO::EXAM_JUDGEGOOD},
            {"judgeperfect", DAN_INFO::EXAM_JUDGEPERFECT},
            {"score",        DAN_INFO::EXAM_SCORE},
        };
        std::string red_str = std::to_string(info.red_value);
        float type_x = -(float)red_str.size() * 20.0f * tex.screen_scale;
        auto icon_it = exam_ids.find(info.exam_type);
        if (icon_it != exam_ids.end())
            tex.draw_texture(icon_it->second, {.x=type_x, .y=y});

        float gauge_shift = (info.exam_type == "gauge") ? -score_margin : 0.0f;
        draw_digit_counter(red_str, score_margin, DAN_INFO::VALUE_COUNTER, 0, y, gauge_shift);

        if (info.exam_range == "less")
            tex.draw_texture(DAN_INFO::EXAM_LESS, {.y=y});
        else if (info.exam_range == "more")
            tex.draw_texture(DAN_INFO::EXAM_MORE, {.y=y});

        tex.draw_texture(DAN_INFO::EXAM_OVERLAY_2, {.y=y});
        std::string cur_str = std::to_string(info.counter_value);
        draw_digit_counter(cur_str, score_margin, DAN_INFO::VALUE_COUNTER, 1, y);

        if (info.exam_type == "gauge") {
            tex.draw_texture(DAN_INFO::EXAM_PERCENT, {.y=y, .index=0});
            tex.draw_texture(DAN_INFO::EXAM_PERCENT, {.y=y, .index=1});
        }

        if (exam_failed[i]) {
            tex.draw_texture(DAN_INFO::EXAM_BG,     {.y=y, .fade=0.5});
            tex.draw_texture(DAN_INFO::EXAM_FAILED, {.y=y});
        }
    }

    // Frame + title
    tex.draw_texture(DAN_INFO::FRAME, {.frame=dan_color});
    if (hori_name) {
        SkinInfo hn = tex.skin_config[SC::DAN_GAME_HORI_NAME];
        hori_name->draw({
            .x = hn.x - hori_name->width / 2.0f,
            .y = hn.y,
            .x2 = std::min(hori_name->width, hn.width) - hori_name->width
        });
    }
}

void DanGameScreen::draw() {
    if (background.has_value()) background->draw_back();
    draw_dan_info();
    dan_gauge.draw();
    if (players.size() == 1)
        players[0]->draw(current_ms, 0, 184 * tex.screen_scale, mask_shader);
    dan_transition.draw();
    if (background.has_value()) background->draw_fore();
    draw_overlay();
}
