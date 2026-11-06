#include "game.h"

#include <algorithm>
#include <cmath>

#include "../libs/file/filesystem.h"
#include "../libs/file/scores.h"
#include "../libs/file/script.h"
#include "../libs/gfx/text.h"
#include "../libs/input/input.h"
#include "../libs/math/animation.h"
#include "../libs/network/network.h"

void GameScreen::on_screen_start() {
    Screen::on_screen_start();
    reset_playback();
    start_delay = 1000.0f;
    JudgePos::X = tex.get_skin_config(SC::GAME_JUDGE_POS)->x;
    JudgePos::Y = tex.get_skin_config(SC::GAME_JUDGE_POS)->y;
    TJAParser::jpos_field_width = (tex.screen_width - JudgePos::X) / tex.screen_scale;
    SessionData& session_data = global_data.session_data[static_cast<int>(global_data.player_num)];
    if (session_data.selected_song.empty() || !exists(session_data.selected_song)) {
        auto songs = get_song_files(global_data.config->paths.tja_path);
        if (songs.empty()) {
            throw std::runtime_error("No songs found in tja_path, cannot load fallback song");
        }
        session_data.selected_song = songs.front();
        session_data.selected_difficulty = static_cast<int>(Difficulty::EASY);
        try {
            SongParser probe(session_data.selected_song, 0);
            auto& course_data = probe.metadata.course_data;
            if (!course_data.empty()) {
                session_data.selected_difficulty = course_data.rbegin()->first;
            }
        } catch (const std::exception& e) {
            spdlog::warn("Failed to probe difficulty for fallback song {}: {}", session_data.selected_song.string(),
                         e.what());
        }
        spdlog::info("GameScreen auto-loaded fallback song: {}", session_data.selected_song.string());
    }
    initialize_chart(session_data.selected_song);
    spdlog::info("Chart initialized for song: {}", session_data.selected_song.string());
    load_hitsounds();
    song_info = SongInfo(session_data.song_title, session_data.song_subtitle, parser->metadata.subtitle_full_display,
                         session_data.genre_index, global_data.songs_played + 1, session_data.genre_label);
    result_transition = ResultTransition(scene_player_num());
    bpm = parser->metadata.bpm;
    scene_preset = parser->metadata.scene_preset;
    if (!movie.has_value()) {
        background.emplace(scene_player_num(), bpm, background_scene_preset());
        spdlog::info("Background initialized");
    } else {
        spdlog::info("Movie initialized");
    }
    transition.emplace(session_data.song_title, session_data.song_subtitle, true);
    if (exists(session_data.selected_song.parent_path() / "Loading.png")) {
        transition->add_loading_graphic((session_data.selected_song.parent_path() / "Loading.png").string());
    }
    transition->start();

    reset_song_clock();

    init_skip();

    std::optional<Note> first_note = players.back()->get_first_note();
    if (first_note.has_value()) {
        double travel_time = first_note->hit_ms - first_note->load_ms;
        double initial_ms = parser->metadata.offset * 1000 + global_data.config->general.audio_offset;
        double extra_delay = initial_ms - start_delay + travel_time;
        if (extra_delay > 0.0) {
            clock.seek(get_current_ms(), clock.position() - extra_delay);
        }
    }
}

Screens GameScreen::on_screen_end(Screens next_screen) {
    unload_song();
    background.reset();
    transition.reset();
    global_data.camera = CameraConfig(); // TJA camera commands must not leak out of the song

    return Screen::on_screen_end(next_screen);
}

Modifiers GameScreen::get_player_modifiers(PlayerNum pn) {
    Modifiers m = player_data_to_modifiers(scores_manager.get_player_data(get_player_id(pn)).value_or(PlayerData{}));
    if (global_data.force_auto_play) {
        m.auto_play = true; // `--auto`
    }
    return m;
}

void GameScreen::load_hitsounds() {
    int neiro_p1 = scores_manager.get_player_data(get_player_id(PlayerNum::P1)).value_or(PlayerData{}).neiro_index;
    int neiro_p2 = scores_manager.get_player_data(get_player_id(PlayerNum::P2)).value_or(PlayerData{}).neiro_index;
    audio.load_sound(resolve_skin_path(fs::path("Sounds/hit_sounds") / std::to_string(neiro_p1) / "don.ogg"),
                     "hitsound_don_1p");
    audio.load_sound(resolve_skin_path(fs::path("Sounds/hit_sounds") / std::to_string(neiro_p1) / "ka.ogg"),
                     "hitsound_kat_1p");
    audio.load_sound(resolve_skin_path(fs::path("Sounds/hit_sounds") / std::to_string(neiro_p2) / "don.ogg"),
                     "hitsound_don_2p");
    audio.load_sound(resolve_skin_path(fs::path("Sounds/hit_sounds") / std::to_string(neiro_p2) / "ka.ogg"),
                     "hitsound_kat_2p");
    spdlog::info("Loaded ogg hit sounds for 1P and 2P");
}

void GameScreen::initialize_chart(const fs::path& song) {
    start_delay = (song.extension() == ".osu") ? 0 : start_delay;
    parser = SongParser(song, start_delay);
    initialize_song_movie();
    set_session_song_info(global_data.player_num);

    load_song_audio(AudioLoadMode::Async);

    create_players();
}

void GameScreen::initialize_song_movie() {
    movie.reset();
    if (!parser->metadata.bgmovie.empty() && fs::is_regular_file(parser->metadata.bgmovie)) {
        movie.emplace(parser->metadata.bgmovie);
    }
}

void GameScreen::set_session_song_info(PlayerNum player_num) {
    auto& session = global_data.session_data[static_cast<int>(player_num)];
    const auto& metadata = parser->metadata;
    const std::string& lang = global_data.config->general.language;

    session.song_title = localized_text(metadata.title, lang, LocalizedTextFallback::ENGLISH_OR_FIRST);
    session.song_subtitle = localized_text(metadata.subtitle, lang, LocalizedTextFallback::NONE);
    session.song_subtitle_full_display = metadata.subtitle_full_display;
}

void GameScreen::create_players() {
    const auto player_index = static_cast<int>(global_data.player_num);
    players.push_back(std::make_unique<Player>(parser, global_data.player_num,
                                               global_data.session_data[player_index].selected_difficulty, false,
                                               get_player_modifiers(global_data.player_num)));
    players.back()->allow_papamama();

    SessionData& replay_session_data = global_data.session_data[player_index];
    if (replay_session_data.replay_input_log.has_value()) {
        std::map<double, InputLogType> typed_log;
        for (const auto& [ms, type] : *replay_session_data.replay_input_log) {
            typed_log.emplace(ms, static_cast<InputLogType>(type));
        }
        players.back()->load_replay(typed_log);

        PlayerData replay_pd;
        replay_pd.username = replay_session_data.replay_username;
        replay_pd.title = replay_session_data.replay_title;
        replay_pd.title_bg = replay_session_data.replay_title_bg;
        replay_pd.chara_color_1 = replay_session_data.replay_chara_color_1;
        replay_pd.chara_color_2 = replay_session_data.replay_chara_color_2;
        replay_pd.chara_color_3 = replay_session_data.replay_chara_color_3;
        replay_pd.chara_head_index = replay_session_data.replay_chara_head_index;
        replay_pd.chara_body_index = replay_session_data.replay_chara_body_index;
        replay_pd.chara_cos_index = replay_session_data.replay_chara_cos_index;
        replay_pd.chara_is_costume = replay_session_data.replay_chara_is_costume;
        players.back()->apply_replay_appearance(replay_pd);
    }
}

void GameScreen::load_song_audio(AudioLoadMode mode) {
    if (!fs::exists(parser->metadata.wave) || song_music || pending_song_load.valid()) {
        return;
    }

    fs::path wave = parser->metadata.wave;
    if (mode == AudioLoadMode::Async) {
        pending_song_load = std::async(std::launch::async, [wave] { return audio.load_sound(wave, "song"); });
    } else {
        song_music = audio.load_sound(wave, "song");
    }
}

void GameScreen::set_players_audio_end() {
    if (!song_music || song_music->empty()) {
        return;
    }
    double audio_end = audio.get_sound_time_length(*song_music) * 1000.0 + audio_start_ms();
    for (auto& player : players) {
        player->set_audio_end(audio_end);
    }
}

void GameScreen::poll_pending_song() {
    if (!pending_song_load.valid() ||
        pending_song_load.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }

    std::string name = pending_song_load.get();
    if (name.empty()) {
        return;
    }
    song_music = name;

    if (!song_started) {
        set_players_audio_end();
    }

    if (song_started && !clock.is_paused()) {
        play_song_audio((clock.position() - audio_start_ms()) / 1000.0);
    }
}

void GameScreen::start_song(double chart_ms) {
    if (chart_ms >= audio_start_ms() && !song_started && !clock.is_paused()) {
        if (song_music) {
            play_song_audio(0.0);
            spdlog::info("Song started at {}", chart_ms);
        }
        if (movie.has_value()) {
            movie->start(get_current_ms());
            movie->set_volume(0.0);
        }
        song_started = true;
    }
}

double GameScreen::audio_start_ms() const {
    return parser->metadata.offset * 1000.0 + start_delay - global_data.config->general.audio_offset;
}

void GameScreen::reset_playback() {
    clock.reset(get_current_ms());
    song_started = false;
    score_saved = false;
    paused_audio_time = 0.0f;
}

void GameScreen::reset_song_clock(double opening_delay_ms) {
    clock.reset(get_current_ms(),
                parser->metadata.offset * 1000.0 + global_data.config->general.audio_offset - opening_delay_ms);
}

void GameScreen::stop_song_audio() {
    if (pending_song_load.valid()) {
        try {
            song_music = pending_song_load.get();
        } catch (const std::exception& e) {
            spdlog::warn("Failed to finish song audio load: {}", e.what());
        }
    }

    if (song_music) {
        audio.stop_sound(*song_music);
    }

    song_music.reset();
    song_started = false;
}

void GameScreen::unload_song() {
    stop_song_audio();

    if (movie.has_value()) {
        movie->stop();
    }

    movie.reset();
    players.clear();
    release_chart_resources();
    parser.reset();
    reset_playback();
}

void GameScreen::play_song_audio(double position_seconds) {
    if (!song_music) {
        return;
    }

    audio.set_sound_pitch(*song_music, song_playback_rate());
    audio.play_sound(*song_music, VolumePreset::MUSIC);
    audio.seek_sound(*song_music, static_cast<float>(std::max(0.0, position_seconds)));
}

void GameScreen::pause_playback() {
    if (clock.is_paused()) {
        return;
    }

    clock.pause(get_current_ms());
    paused_audio_time = static_cast<float>(std::max(0.0, (clock.position() - audio_start_ms()) / 1000.0));

    if (song_music && song_started) {
        paused_audio_time = audio.get_sound_time_played(*song_music);
        audio.stop_sound(*song_music);
    }
}

void GameScreen::resume_playback(std::optional<double> chart_ms) {
    if (!clock.is_paused()) {
        return;
    }

    double current_ms = get_current_ms();

    if (chart_ms) {
        clock.seek(current_ms, *chart_ms);
        paused_audio_time = static_cast<float>(std::max(0.0, (*chart_ms - audio_start_ms()) / 1000.0));
        song_started = true;
    }

    clock.resume(current_ms);

    if (song_started) {
        play_song_audio(paused_audio_time);
    }
}

void GameScreen::toggle_pause() {
    if (clock.is_paused()) {
        resume_playback();
    } else {
        pause_playback();
    }
}

void GameScreen::restart_song() {
    unload_song();
    initialize_chart(global_data.session_data[static_cast<int>(restart_player_num())].selected_song);

    if (background.has_value()) {
        for (const auto& player : players) {
            background->handle_miss_streak(player->player_num, false);
        }
    }

    result_transition = ResultTransition(scene_player_num());
    audio.play_sound("restart", VolumePreset::SOUND);
    init_skip();
    reset_song_clock();
}

std::optional<Screens> GameScreen::global_keys() {
    if (check_key_pressed(global_data.config->keys.restart_key)) {
        restart_song();
    }

    if (check_key_pressed(global_data.config->keys.back_key)) {
        return on_screen_end(Screens::SONG_SELECT);
    }

    return std::nullopt;
}

void GameScreen::update_background(double current_ms) {
    if (movie.has_value()) {
        movie->update(current_ms);
    } else {
        bpm = players[0]->bpm;
        if (background.has_value()) {
            background->update(current_ms, bpm);
        }
    }
}

void GameScreen::save_score(const std::string& player_id, PlayerNum player_num) {
    Player* target = nullptr;
    for (const auto& player : players) {
        if (player && player->player_num == player_num) {
            if (player->is_auto_play() || player->is_replay()) {
                return;
            }
            target = player.get();
        }
    }
    if (!target) {
        return;
    }
    if (target->get_modifiers().subdiff != 0) {
        ResultData& result = global_data.session_data[static_cast<int>(player_num)].result_data;
        result.prev_score = result.score;
        return;
    }

    try {
        Score score;
        SessionData& session_data = global_data.session_data[static_cast<int>(player_num)];
        std::string hash = session_data.song_hash;
        score.score = session_data.result_data.score;
        score.good = session_data.result_data.good;
        score.ok = session_data.result_data.ok;
        score.bad = session_data.result_data.bad;
        score.max_combo = session_data.result_data.max_combo;
        score.drumroll = session_data.result_data.total_drumroll;
        auto prev_score = scores_manager.get_score(hash, session_data.selected_difficulty, player_id);
        if (prev_score.has_value()) {
            session_data.result_data.prev_score = prev_score->score;
        }
        const bool run_skipped = skipped;
        if (run_skipped) {
            score.crown = Crown::NONE;
        } else if (score.ok == 0 && score.bad == 0) {
            score.crown = Crown::DFC;
        } else if (score.bad == 0) {
            score.crown = Crown::FC;
        } else if (target->gauge->get_is_clear()) {
            score.crown = Crown::CLEAR;
        } else {
            score.crown = Crown::NONE;
        }
        if (run_skipped) {
            score.rank = Rank::_NONE;
        } else {
            score.rank = rank_for_score(score.score);
        }
        int64_t played_at = unix_now();
        std::string modifiers_json = modifiers_to_json(target->get_modifiers());
        scores_manager.save_score(hash, session_data.selected_difficulty, player_id, score, played_at, modifiers_json);
        PlayerData pd = scores_manager.get_player_data(player_id).value_or(PlayerData{});
        if (global_data.config->general.score_method != ScoreMethod::GEN3) {
            std::string access_code;
            if (player_id == scores_manager.player_1 && !global_data.config->network.access_code_1.empty()) {
                access_code = global_data.config->network.access_code_1;
            } else if (player_id == scores_manager.player_2 && !global_data.config->network.access_code_2.empty()) {
                access_code = global_data.config->network.access_code_2;
            }
            if (!access_code.empty()) {
                network.submit_score(hash, session_data.selected_difficulty, access_code, score, target->input_log,
                                     played_at, modifiers_json, pd.chara_is_costume, pd.chara_cos_index);
                session_data.result_data.submitted = true;
                const int seat = static_cast<int>(player_num) - 1;
                if (!run_skipped && (seat == 0 || seat == 1)) {
                    global_data.credit_gauges[seat].push_back(static_cast<int>(target->gauge->get_length()));
                    global_data.credit_titles[seat].push_back(session_data.song_title);
                    global_data.medals_due[seat] = true;
                }
                if (!run_skipped && (seat == 0 || seat == 1) && global_data.kinotake_team[seat] >= 0) {
                    const bool duet = std::ranges::count_if(players, [](const auto& p) {
                                          return p && !p->is_auto_play() && !p->is_replay();
                                      }) == 2;
                    const bool kinotake_costume =
                        pd.chara_is_costume && (pd.chara_cos_index == KINOTAKE_COSTUME_KINOKO ||
                                                pd.chara_cos_index == KINOTAKE_COSTUME_TAKENOKO);
                    network.submit_kinotake(seat, access_code, global_data.kinotake_team[seat], score.score, duet,
                                            kinotake_costume);
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("GameScreen::save_score failed for player {}: {}", player_id, e.what());
    } catch (...) {
        spdlog::error("GameScreen::save_score failed for player {}: unknown exception", player_id);
    }
}

void GameScreen::resync_song(double current_ms) {
    if (!song_started || clock.is_paused()) {
        return;
    }
    if (!song_music) {
        return;
    }
    if (!audio.is_sound_playing(*song_music)) {
        return;
    }

    double audio_ms = audio.get_sound_time_played(*song_music) * 1000.0f;
    double audio_ms_adjusted = audio_ms + audio_start_ms();

    clock.synchronize(current_ms, audio_ms_adjusted, [] { return get_monitor_refresh_rate(); });
}

void GameScreen::init_skip() {
    skip_lane = PlayerNum::ALL;
    skip_count = 0;
    skip_last = -1;
    skip_play_hits = 0;
    skipped = false;
    skip_text.reset();
    global_data.live_skip_count = -1;
    global_data.live_skip_used = false;

    if (!tex.options[SCO::OPTION_SKIP_ROW]) {
        return;
    }

    if (global_data.player_num == PlayerNum::TWO_PLAYER || players.size() > 1) {
        return;
    }

    bool wanted = false;
    bool lane_used[3] = {false, false, false};
    const auto p1_index = static_cast<int>(PlayerNum::P1);
    const auto p2_index = static_cast<int>(PlayerNum::P2);
    for (const auto& player : players) {
        if (!player) {
            continue;
        }
        if (player->is_skip_enabled()) {
            wanted = true;
        }
        const auto pn = static_cast<int>(player->player_num);
        if (pn == p1_index || pn == p2_index) {
            lane_used[pn] = true;
        }
    }
    if (!wanted) {
        return;
    }

    if (!lane_used[p2_index]) {
        skip_lane = PlayerNum::P2;
    } else if (!lane_used[p1_index]) {
        skip_lane = PlayerNum::P1;
    }
    if (skip_lane != PlayerNum::ALL) {
        global_data.live_skip_count = 0;
    }
}

void GameScreen::update_skip() {
    if (skip_lane == PlayerNum::ALL) {
        return;
    }
    if (!skipped && !clock.is_paused()) {
        poll_skip();
    }
    push_skip_state();
}

void GameScreen::push_skip_state() {
    global_data.live_skip_count = skip_count;
    global_data.live_skip_used = skipped;
    if (!background.has_value() || !background->wants_skip()) {
        return;
    }

    sol::state& lua = *script_manager.lua;
    sol::table st = lua.create_table();
    st["count"] = skip_count;
    st["total"] = SKIP_HITS;
    st["remaining"] = SKIP_HITS - skip_count;
    st["skipped"] = skipped;
    background->handle_skip(global_data.player_num, st);
}

void GameScreen::poll_skip() {

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
        do_skip();
    }
}

void GameScreen::do_skip() {
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
    spdlog::info("skipped at {} ms", clock.position());
    if (song_music) {
        audio.stop_sound(*song_music);
    }
    if (movie.has_value()) {
        movie->stop();
    }
    for (auto& player : players) {
        if (player) {
            player->cut_to_end(clock.position());
        }
    }
}

void GameScreen::draw_skip() {
    if (background.has_value() && background->wants_skip()) {
        return;
    }
    if (!skip_text || skip_count <= 0) {
        return;
    }
    const SkinInfo* pos = tex.get_skin_config(skipped ? SC::SKIP_USED_TEXT : SC::SKIP_COUNTER);
    if (!pos) {
        return;
    }
    skip_text->draw({.x = pos->x - skip_text->width / 2.0f, .y = pos->y});
}

void GameScreen::end_song(double end_time, double transition_delay, BackgroundNotification notification) {
    if (clock.position() >= end_time + 1000 && !score_saved) {
        for (const auto& player : players) {
            const auto player_index = static_cast<int>(player->player_num);
            global_data.session_data[player_index].result_data = player->get_result_score();
        }

        for (const auto& player : players) {
            save_score(get_player_id(player->player_num), player->player_num);
        }

        for (const auto& player : players) {
            player->spawn_ending_anim(background ? &*background : nullptr);

            if (notification == BackgroundNotification::Enabled && background) {
                int g = player->get_good(), o = player->get_ok(), b = player->get_bad();
                background->handle_song_end(player->player_num, g, o, b, g + o + b);
            }
        }

        global_data.songs_played += 1;
        score_saved = true;
    }

    if (clock.position() >= end_time + transition_delay && !result_transition.is_started) {
        result_transition.start();
        audio.play_sound("result_transition", VolumePreset::VOICE);
        spdlog::info("Result transition started and voice played");
    }
}

void GameScreen::update_gameplay(double current_ms) {
    poll_pending_song();
    if (transition->is_finished()) {
        start_song(clock.position());
        reset_input_lock();
    }

    resync_song(current_ms);
    update_background(current_ms);

    for (auto& player : players) {
        player->update(clock.position(), current_ms, background);
    }

    song_info.update(current_ms);
}

std::optional<Screens> GameScreen::update() {
    if (auto init = Screen::update()) {
        return init;
    }

    double current_ms = get_frame_ms();
    allnet_indicator.update(current_ms);
    clock.update(current_ms);

    transition->update(current_ms);
    update_gameplay(current_ms);
    update_skip();
    result_transition.update(current_ms);

    if (result_transition.is_finished && !audio.is_sound_playing("result_transition")) {
        return on_screen_end(Screens::RESULT);
    }

    if (clock.position() >= players[0]->end_time) {
        const double transition_delay = global_data.config->general.fast_transitions
                                            ? kFastResultTransitionDelayMs
                                            : (players[0]->was_skipped() ? 2800.0 : 8533.34);

        end_song(players[0]->end_time, transition_delay, BackgroundNotification::Enabled);
    }
    if (global_data.config->limits.song_limit <= 0) {
        return global_keys();
    }

    return std::nullopt;
}

void GameScreen::draw_overlay(bool with_song_info) {
    if (with_song_info) {
        song_info.draw();
    }
    draw_skip();
    bool over_transition = false;
    if (!transition->is_finished()) {
        transition->draw();
        over_transition = true;
    }
    if (over_transition) {
        global_data.in_transition = true;
    }
    allnet_indicator.draw();
    if (over_transition) {
        global_data.in_transition = false;
    }
    if (result_transition.is_started) {
        result_transition.draw();
    }
}

void GameScreen::draw_players() {
    if (players.size() == 1) {
        players[0]->draw(clock.position(), 0, 184 * tex.screen_scale);
    } else if (players.size() == 2) {
        players[0]->draw(clock.position(), 0, 184 * tex.screen_scale);
        players[1]->draw(clock.position(), 0, tex.get_skin_config(SC::GAME_P2_Y)->y);
    } else {
        float gap = (tex.screen_height - (players.size() * 176 * tex.screen_scale)) / (players.size() + 1);
        for (int i = 0; i < players.size(); i++) {
            float position = gap + i * ((176 * tex.screen_scale) + gap);
            players[i]->draw(clock.position(), 0, position);
        }
    }
}

void GameScreen::draw() {
    if (movie.has_value()) {
        movie->draw();
    } else if (background.has_value()) {
        background->draw_back();
    } else {
        clear_background(Color::BLACK);
    }
    draw_players();
    if (background.has_value()) {
        background->draw_fore();
    }
    draw_overlay();
}
