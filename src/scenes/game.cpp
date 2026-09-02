#include "game.h"
#include "../libs/scores.h"
#include "../libs/input.h"
#include "../libs/network.h"
#include "../libs/script.h"
#include "../libs/filesystem.h"
#include <cmath>


void GameScreen::on_screen_start() {
    Screen::on_screen_start();
    mask_shader = load_shader("shader/dummy.vs", "shader/mask.fs");
    ms_from_start = 0;
    start_ms = 0;
    start_delay = 1000.0f;
    last_resync_ms = 0;
    JudgePos::X = tex.skin_config[SC::JUDGE_POS].x;
    JudgePos::Y = tex.skin_config[SC::JUDGE_POS].y;
    song_started = false;
    paused = false;
    score_saved = false;
    pause_time = 0;
    global_data.live_combo = global_data.live_score = global_data.live_drumroll = 0;
    global_data.live_gogo = false;
    if (global_data.config->general.nijiiro_notes) {
        tex.load_folder("game", "notes_nijiiro");
        for (auto [src, dst] : std::initializer_list<std::pair<uint32_t, uint32_t>>{
            {NOTES_NIJIIRO::_0,                  NOTES::_0},
            {NOTES_NIJIIRO::_1,                  NOTES::_1},
            {NOTES_NIJIIRO::_2,                  NOTES::_2},
            {NOTES_NIJIIRO::_3,                  NOTES::_3},
            {NOTES_NIJIIRO::_4,                  NOTES::_4},
            {NOTES_NIJIIRO::_5,                  NOTES::_5},
            {NOTES_NIJIIRO::_6,                  NOTES::_6},
            {NOTES_NIJIIRO::_7,                  NOTES::_7},
            {NOTES_NIJIIRO::_8,                  NOTES::_8},
            {NOTES_NIJIIRO::_9,                  NOTES::_9},
            {NOTES_NIJIIRO::_10,                 NOTES::_10},
            {NOTES_NIJIIRO::DRUMROLL_BIG_TAIL,   NOTES::DRUMROLL_BIG_TAIL},
            {NOTES_NIJIIRO::DRUMROLL_TAIL,       NOTES::DRUMROLL_TAIL},
            {NOTES_NIJIIRO::MOJI,                NOTES::MOJI},
            {NOTES_NIJIIRO::MOJI_DRUMROLL_MID,   NOTES::MOJI_DRUMROLL_MID},
            {NOTES_NIJIIRO::MOJI_DRUMROLL_MID_BIG, NOTES::MOJI_DRUMROLL_MID_BIG},
        }) {
            auto it = tex.textures.find(src);
            if (it != tex.textures.end()) tex.textures[dst] = it->second;
        }
    }
    auto rainbow_mask = std::dynamic_pointer_cast<SingleTexture>(tex.textures[BALLOON::RAINBOW_MASK]);
    auto rainbow = std::dynamic_pointer_cast<SingleTexture>(tex.textures[BALLOON::RAINBOW]);
    if (rainbow_mask && rainbow) {
        SetShaderValueTexture(mask_shader, GetShaderLocation(mask_shader, "texture0"), rainbow_mask->texture);
        SetShaderValueTexture(mask_shader, GetShaderLocation(mask_shader, "texture1"), rainbow->texture);
    }
    SessionData& session_data = global_data.session_data[(int)global_data.player_num];
    init_tja(session_data.selected_song);
    spdlog::info("TJA initialized for song: {}", session_data.selected_song.string());
    load_hitsounds();
    song_info = SongInfo(session_data.song_title, session_data.song_subtitle, parser->metadata.subtitle_full_display, session_data.genre_index, global_data.songs_played + 1);
    result_transition = ResultTransition(global_data.player_num);
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

    start_ms = get_current_ms() - parser->metadata.offset*1000 - (double)global_data.config->general.audio_offset;

    init_skip();

    std::optional<Note> first_note = players.back()->get_first_note();
    if (first_note.has_value()) {
        double travel_time = first_note->hit_ms - first_note->load_ms;
        double initial_ms = parser->metadata.offset * 1000 + (double)global_data.config->general.audio_offset;
        double extra_delay = initial_ms - (double)start_delay + travel_time;
        if (extra_delay > 0.0) {
            start_ms += extra_delay;
        }
    }
}

Screens GameScreen::on_screen_end(Screens next_screen) {
    song_started = false;
    ray::UnloadShader(mask_shader);

    if (movie.has_value()) {
        movie->stop();
        spdlog::info("Movie stopped");
    }
    movie.reset();
    if (background.has_value()) {
        background.reset();
        spdlog::info("Background unloaded");
    }
    transition.reset();
    if (pending_song_load.valid()) pending_song_load.wait();
    pending_song_load = {};
    song_music.reset();
    parser.reset();
    players.clear();

    return Screen::on_screen_end(next_screen);
}

Modifiers GameScreen::get_player_modifiers(PlayerNum pn) {
    Modifiers m = player_data_to_modifiers(scores_manager.get_player_data(get_player_id(pn)).value_or(PlayerData{}));
    if (global_data.force_auto_play) m.auto_play = true;   // `--auto`
    return m;
}

void GameScreen::load_hitsounds() {
    int neiro_p1 = scores_manager.get_player_data(get_player_id(PlayerNum::P1)).value_or(PlayerData{}).neiro_index;
    int neiro_p2 = scores_manager.get_player_data(get_player_id(PlayerNum::P2)).value_or(PlayerData{}).neiro_index;
    audio.load_sound(resolve_skin_path(fs::path("Sounds/hit_sounds") / std::to_string(neiro_p1) / "don.ogg"), "hitsound_don_1p");
    audio.load_sound(resolve_skin_path(fs::path("Sounds/hit_sounds") / std::to_string(neiro_p1) / "ka.ogg"),  "hitsound_kat_1p");
    audio.load_sound(resolve_skin_path(fs::path("Sounds/hit_sounds") / std::to_string(neiro_p2) / "don.ogg"), "hitsound_don_2p");
    audio.load_sound(resolve_skin_path(fs::path("Sounds/hit_sounds") / std::to_string(neiro_p2) / "ka.ogg"),  "hitsound_kat_2p");
    spdlog::info("Loaded ogg hit sounds for 1P and 2P");
}

void GameScreen::init_tja(fs::path song) {
    start_delay = (song.extension() == ".osu") ? 0 : start_delay;
    parser = SongParser(song, start_delay);
    if (fs::exists(parser->metadata.bgmovie)) {
        movie.emplace(parser->metadata.bgmovie);
    }
    auto& titles = parser->metadata.title;
    auto& subtitles = parser->metadata.subtitle;
    const std::string& lang = global_data.config->general.language;

    global_data.session_data[(int)global_data.player_num].song_title = titles.count(lang) ? titles.at(lang) : titles.count("en") ? titles.at("en") : titles.empty() ? "" : titles.begin()->second;
    global_data.session_data[(int)global_data.player_num].song_subtitle = subtitles.count(lang) ? subtitles.at(lang) : "";
    global_data.session_data[(int)global_data.player_num].song_subtitle_full_display = parser->metadata.subtitle_full_display;

    if (fs::exists(parser->metadata.wave) && !song_music.has_value() && !pending_song_load.valid()) {
        fs::path wave = parser->metadata.wave;
        pending_song_load = std::async(std::launch::async, [wave] {
            return audio.load_sound(wave, "song");
        });
    }

    players.push_back(std::make_unique<Player>(parser, global_data.player_num, global_data.session_data[(int)global_data.player_num].selected_difficulty, false, get_player_modifiers(global_data.player_num)));
}

void GameScreen::poll_pending_song() {
    if (!pending_song_load.valid() ||
        pending_song_load.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    std::string name = pending_song_load.get();
    if (name.empty()) return;
    song_music = name;

    if (song_started && !paused) {
        audio.play_sound(*song_music, VolumePreset::MUSIC);
        double audio_ms = ms_from_start
                        - (parser->metadata.offset * 1000 + start_delay
                           - (double)global_data.config->general.audio_offset);
        audio.seek_sound(*song_music, (float)std::max(0.0, audio_ms / 1000.0));
    }
}

void GameScreen::start_song(double ms_from_start) {
    if (ms_from_start >= parser->metadata.offset*1000 + start_delay - (double)global_data.config->general.audio_offset && !song_started) {
        if (song_music.has_value()) {
            audio.play_sound(song_music.value(), VolumePreset::MUSIC);
            spdlog::info("Song started at {}", ms_from_start);
        }
        if (movie.has_value()) {
            movie->start(get_current_ms());
            movie->set_volume(0.0);
        }
        song_started = true;
    }
}

void GameScreen::pause_song() {
    paused = !paused;
    double audio_time;
    if (paused) {
        if (song_music.has_value()) {
            audio_time = audio.get_sound_time_played(song_music.value());
            audio.stop_sound(song_music.value());
        }
        pause_time = get_current_ms() - start_ms;
    } else {
        if (song_music.has_value()) {
            audio.play_sound(song_music.value(), VolumePreset::MUSIC);
            audio.seek_sound(song_music.value(), audio_time);
        }
        start_ms = get_current_ms() - pause_time;
    }
}

void GameScreen::restart_song() {
    if (song_music.has_value()) {
        audio.stop_sound(song_music.value());
    }
    players.clear();
    init_tja(global_data.session_data[(int)global_data.player_num].selected_song);
    audio.play_sound("restart", VolumePreset::SOUND);
    init_skip();
    song_started = false;
    score_saved = false;
    paused = false;
    pause_time = 0;
    last_resync_ms = 0;
    start_ms = get_current_ms() - parser->metadata.offset*1000 - (double)global_data.config->general.audio_offset;
    ms_from_start = get_current_ms() - start_ms;
}

std::optional<Screens> GameScreen::global_keys() {
    if (check_key_pressed(global_data.config->keys.restart_key))
        restart_song();

    if (check_key_pressed(global_data.config->keys.back_key)) {
        if (song_music.has_value())
            audio.stop_sound(song_music.value());
        return on_screen_end(Screens::SONG_SELECT);
    }

    if (ray::IsKeyPressed(global_data.config->keys.pause_key))
        pause_song();

    return std::nullopt;
}

void GameScreen::update_background(double current_ms) {
    if (movie.has_value()) {
        movie->update(current_ms);
    } else {
        bpm = players[0]->bpm;
        if (background.has_value()) background->update(current_ms, bpm);
    }
}

void GameScreen::save_score(int player_id, PlayerNum player_num) {
    for (const auto& player : players)
        if (player && player->player_num == player_num && player->is_auto_play())
            return;

    Score score;
    SessionData& session_data = global_data.session_data[(int)player_num];
    std::string hash = session_data.song_hash;
    score.score = session_data.result_data.score;
    score.good = session_data.result_data.good;
    score.ok = session_data.result_data.ok;
    score.bad = session_data.result_data.bad;
    score.max_combo = session_data.result_data.max_combo;
    score.drumroll = session_data.result_data.total_drumroll;
    auto prev_score = scores_manager.get_score(hash, global_data.session_data[(int)player_num].selected_difficulty, player_id);
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
    } else if (players[0]->gauge->get_is_clear()) {
        score.crown = Crown::CLEAR;
    } else {
        score.crown = Crown::NONE;
    }
    if (run_skipped) {
        score.rank = Rank::_NONE;
    } else if (score.score >= 1000000) {
        score.rank = Rank::_RAINBOW;
    } else if (score.score >= 950000) {
        score.rank = Rank::_PURPLE;
    } else if (score.score >= 900000) {
        score.rank = Rank::_PINK;
    } else if (score.score >= 800000) {
        score.rank = Rank::_GOLD;
    } else if (score.score >= 700000) {
        score.rank = Rank::_SILVER;
    } else if (score.score >= 600000) {
        score.rank = Rank::_BRONZE;
    } else {
        score.rank = Rank::_WHITE;
    }
    int64_t played_at = unix_now();
    std::string modifiers_json = modifiers_to_json(players[0]->get_modifiers());
    scores_manager.save_score(hash, session_data.selected_difficulty, player_id, score, played_at, modifiers_json);
    PlayerData pd = scores_manager.get_player_data(player_id).value_or(PlayerData{});
    if (global_data.config->general.score_method != ScoreMethod::GEN3) {
        network.submit_score(hash, session_data.selected_difficulty, global_data.config->network.access_code, score, players[0]->input_log, played_at, modifiers_json, pd.chara_is_costume, pd.chara_cos_index);
    }
}

void GameScreen::resync_song(double current_ms) {
    if (!song_started) return;
    if (!song_music.has_value()) return;
    if (!audio.is_sound_playing(song_music.value())) return;

    double audio_ms = audio.get_sound_time_played(song_music.value()) * 1000.0f;
    double audio_ms_adjusted = audio_ms + (parser->metadata.offset * 1000 + start_delay - (double)global_data.config->general.audio_offset);

    double drift = audio_ms_adjusted - ms_from_start;
    if (std::abs(drift) > 100.0) {
        spdlog::debug("Hard resyncing chart from {} to {}", ms_from_start, audio_ms_adjusted);
        ms_from_start = audio_ms_adjusted;
    } else if (std::abs(drift) > 5.0) {
        double frame_delta = (last_resync_ms > 0.0) ? (current_ms - last_resync_ms) : 16.667;
        double correction_rate = std::min(frame_delta / 16.667, 4.0);
        ms_from_start += drift * 0.5 * correction_rate;
    }
    last_resync_ms = current_ms;
    start_ms = current_ms - ms_from_start;
}

void GameScreen::init_skip() {
    skip_lane = PlayerNum::ALL;
    skip_count = 0;
    skip_last = -1;
    skip_play_hits = 0;
    skipped = false;
    skip_text.reset();
    global_data.live_skip_count = -1;
    global_data.live_skip_used  = false;

    if (!tex.options[SCO::OPTION_SKIP_ROW]) return;

    if (global_data.player_num == PlayerNum::TWO_PLAYER || players.size() > 1) {
        return;
    }

    bool wanted = false;
    bool lane_used[3] = {false, false, false};
    for (const auto& player : players) {
        if (!player) continue;
        if (player->is_skip_enabled()) wanted = true;
        int pn = (int)player->player_num;
        if (pn == (int)PlayerNum::P1 || pn == (int)PlayerNum::P2) lane_used[pn] = true;
    }
    if (!wanted) return;

    if      (!lane_used[(int)PlayerNum::P2]) skip_lane = PlayerNum::P2;
    else if (!lane_used[(int)PlayerNum::P1]) skip_lane = PlayerNum::P1;
    if (skip_lane != PlayerNum::ALL) {
        global_data.live_skip_count = 0;
    }
}

void GameScreen::update_skip() {
    if (skip_lane == PlayerNum::ALL) return;
    if (!skipped && !paused) poll_skip();
    push_skip_state();
}

void GameScreen::push_skip_state() {
    global_data.live_skip_count = skip_count;
    global_data.live_skip_used  = skipped;
    if (!background.has_value() || !background->wants_skip()) return;

    sol::state& lua = *script_manager.lua;
    sol::table st   = lua.create_table();
    st["count"]     = skip_count;
    st["total"]     = SKIP_HITS;
    st["remaining"] = SKIP_HITS - skip_count;
    st["skipped"]   = skipped;
    background->handle_skip(global_data.player_num, st);
}

void GameScreen::poll_skip() {

    size_t play_hits = 0;
    for (const auto& player : players)
        if (player) play_hits += player->input_log.size();
    if (play_hits != skip_play_hits) {
        skip_play_hits = play_hits;
        if (skip_count > 0 && skip_count < SKIP_HITS) {
            skip_count = 0;
            skip_last = -1;
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

    if (skip_count >= SKIP_HITS) do_skip();
}

void GameScreen::do_skip() {
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
    spdlog::info("skipped at {} ms", ms_from_start);
    if (song_music.has_value()) audio.stop_sound(song_music.value());
    if (movie.has_value()) movie->stop();
    for (auto& player : players)
        if (player) player->cut_to_end(ms_from_start);
}

void GameScreen::draw_skip() {
    if (background.has_value() && background->wants_skip()) return;
    if (!skip_text || skip_count <= 0) return;
    const SkinInfo* pos = tex.skin_entry(skipped ? "skip_used_text" : "skip_counter");
    if (!pos) return;
    skip_text->draw({.x = pos->x - skip_text->width / 2.0f, .y = pos->y});
}

void GameScreen::end_song() {
    if (ms_from_start >= players[0]->end_time + 1000 && !score_saved) {
        global_data.session_data[(int)players[0]->player_num].result_data = players[0]->get_result_score();
        save_score(global_data.config->general.player_1_id, players[0]->player_num);
        for (auto& player : players) {
            player->spawn_ending_anim();
            if (background.has_value()) {
                int g = player->get_good(), o = player->get_ok(), b = player->get_bad();
                background->handle_song_end(player->player_num, g, o, b, g + o + b);
            }
        }
        global_data.songs_played += 1;
        score_saved = true;
    }
    constexpr double SKIP_RESULT_DELAY = 2800.0;
    double transition_delay = players[0]->was_skipped() ? SKIP_RESULT_DELAY : 8533.34;
    if (ms_from_start >= players[0]->end_time + transition_delay) {
        if (!result_transition.is_started) {
            result_transition.start();
            audio.play_sound("result_transition", VolumePreset::VOICE);
            spdlog::info("Result transition started and voice played");
        }
    }
}

std::optional<Screens> GameScreen::update() {
    Screen::update();

    double current_ms = get_frame_ms();
    allnet_indicator.update(current_ms);
    if (!paused)
        ms_from_start = current_ms - start_ms;

    transition->update(current_ms);
    poll_pending_song();
    if (transition->is_finished()) {
        start_song(ms_from_start);
        global_data.input_locked = 0;
    }
    resync_song(current_ms);
    update_background(current_ms);

    for (auto& player : players)
        player->update(ms_from_start, current_ms, background);
    song_info.update(current_ms);
    update_skip();
    result_transition.update(current_ms);

    if (result_transition.is_finished && !audio.is_sound_playing("result_transition")) {
        return on_screen_end(Screens::RESULT);
    } else if (ms_from_start >= players[0]->end_time) {
        end_song();
    }
    if (global_data.config->general.song_limit <= 0) return global_keys();

    return std::nullopt;
}

void GameScreen::draw_overlay() {
    song_info.draw();
    draw_skip();
    bool over_transition = false;
    if (!transition->is_finished()) {
        transition->draw();
        over_transition = true;
    }
    if (over_transition) global_data.in_transition = true;
    allnet_indicator.draw();
    if (over_transition) global_data.in_transition = false;
    if (result_transition.is_started) {
        result_transition.draw();
    }
}

void GameScreen::draw_players() {
    if (players.size() == 1) {
        players[0]->draw(ms_from_start, 0, 184 * tex.screen_scale, mask_shader);
    } else if (players.size() == 2) {
        players[0]->draw(ms_from_start, 0, 184 * tex.screen_scale, mask_shader);
        players[1]->draw(ms_from_start, 0, 360 * tex.screen_scale, mask_shader);
    } else {
        float gap = ((float)tex.screen_height - (players.size() * 176 * tex.screen_scale)) / (players.size() + 1);
        for (int i = 0; i < players.size(); i++) {
            float position = gap + i * ((176 * tex.screen_scale) + gap);
            players[i]->draw(ms_from_start, 0, position, mask_shader);
        }
    }
}

void GameScreen::draw() {
    if (movie.has_value()) {
        movie->draw();
    } else if (background.has_value()) {
        background->draw_back();
    } else {
        ray::ClearBackground(ray::BLACK);
    }
    draw_players();
    if (background.has_value()) background->draw_fore();
    draw_overlay();
}
