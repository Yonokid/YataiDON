#include "game.h"
#include "../libs/scores.h"

void GameScreen::on_screen_start() {
    Screen::on_screen_start();
    mask_shader = ray::LoadShader("shader/dummy.vs", "shader/mask.fs");
    ms_from_start = 0;
    start_ms = 0;
    start_delay = 1000.0f;
    JudgePos::X = 414 * tex.screen_scale;
    JudgePos::Y = 256 * tex.screen_scale;
    song_started = false;
    paused = false;
    score_saved = false;
    pause_time = 0;
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
    song_info = SongInfo(session_data.song_title, session_data.genre_index);
    result_transition = ResultTransition(global_data.player_num);
    bpm = parser->metadata.bpm;
    scene_preset = parser->metadata.scene_preset;
    if (!movie.has_value()) {
        background.emplace(global_data.player_num, bpm, scene_preset);
        spdlog::info("Background initialized");
    } else {
        spdlog::info("Movie initialized");
    }
    transition.emplace(session_data.song_title, session_data.song_subtitle, true);
    if (exists(session_data.selected_song.parent_path() / "Loading.png")) {
        transition->add_loading_graphic((session_data.selected_song.parent_path() / "Loading.png").string());
    }
    transition->start();
}

Screens GameScreen::on_screen_end(Screens next_screen) {
    song_started = false;
    ray::UnloadShader(mask_shader);

    if (movie.has_value()) {
        movie->stop();
        spdlog::info("Movie stopped");
    }
    if (background.has_value()) {
        background.reset();
        spdlog::info("Background unloaded");
    }
    transition.reset();
    song_music.reset();
    parser.reset();
    players.clear();

    return Screen::on_screen_end(next_screen);
}

void GameScreen::load_hitsounds() {
    fs::path sounds_dir = audio.sounds_path;
    if (global_data.hit_sound[(int)global_data.player_num] == -1) {
        audio.load_sound("", "hitsound_don_1p");
        audio.load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P1]) / "ka.wav", "hitsound_kat_1p");
        audio.load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P1]) / "don.wav", "hitsound_don_1p");
        audio.load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P2]) / "ka.wav", "hitsound_kat_2p");
        audio.load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P2]) / "don.wav", "hitsound_don_2p");
        spdlog::info("Loaded wav hit sounds for 1P and 2P");
    } else {
        audio.load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P1]) / "don.ogg", "hitsound_don_1p");
        audio.load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P1]) / "ka.ogg", "hitsound_kat_1p");
        audio.load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P2]) / "don.ogg", "hitsound_don_2p");
        audio.load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P2]) / "ka.ogg", "hitsound_kat_2p");
        spdlog::info("Loaded ogg hit sounds for 1P and 2P");
    }
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

    if (fs::exists(parser->metadata.wave) && !song_music.has_value()) {
        song_music = audio.load_sound(parser->metadata.wave, "song");
    }

    players.push_back(std::make_unique<Player>(parser, global_data.player_num, global_data.session_data[(int)global_data.player_num].selected_difficulty, false, global_data.modifiers[(int)global_data.player_num]));
    start_ms = get_current_ms() - parser->metadata.offset*1000 - (double)global_data.config->general.audio_offset;

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
    song_started = false;
}

std::optional<Screens> GameScreen::global_keys() {
    if (ray::IsKeyPressed(global_data.config->keys.restart_key))
        restart_song();

    if (ray::IsKeyPressed(global_data.config->keys.back_key)) {
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

void GameScreen::save_score(int player_id) {
    Score score;
    SessionData& session_data = global_data.session_data[player_id];
    std::string hash = session_data.song_hash;
    score.score = session_data.result_data.score;
    score.good = session_data.result_data.good;
    score.ok = session_data.result_data.ok;
    score.bad = session_data.result_data.bad;
    score.max_combo = session_data.result_data.max_combo;
    score.drumroll = session_data.result_data.total_drumroll;
    auto prev_score = scores_manager.get_score(hash, global_data.session_data[player_id].selected_difficulty, player_id);
    if (prev_score.has_value()) {
        session_data.result_data.prev_score = prev_score->score;
    }
    if (score.ok == 0 && score.bad == 0) {
        score.crown = Crown::DFC;
    } else if (score.bad == 0) {
        score.crown = Crown::FC;
    } else {
        score.crown = Crown::CLEAR;
    }
    if (score.score >= 1000000) {
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
    scores_manager.save_score(hash, session_data.selected_difficulty, player_id, score);
    global_data.songs_played += 1;
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
        ms_from_start += drift * 0.5;
    }
    start_ms = current_ms - ms_from_start;
}

void GameScreen::end_song() {
    if (ms_from_start >= players[0]->end_time + 1000 && !score_saved) {
        global_data.session_data[(int)players[0]->player_num].result_data = players[0]->get_result_score();
        save_score((int)players[0]->player_num);
        for (auto& player : players) {
            player->spawn_ending_anim();
        }
        score_saved = true;
    }
    if (ms_from_start >= players[0]->end_time + 8533.34) {
        if (!result_transition.is_started) {
            result_transition.start();
            audio.play_sound("result_transition", VolumePreset::VOICE);
            spdlog::info("Result transition started and voice played");
        }
    }
}

std::optional<Screens> GameScreen::update() {
    Screen::update();

    double current_ms = get_current_ms();
    if (!paused)
        ms_from_start = current_ms - start_ms;

    transition->update(current_ms);
    if (transition->is_finished()) {
        start_song(ms_from_start);
        global_data.input_locked = 0;
    }
    resync_song(current_ms);
    update_background(current_ms);

    for (auto& player : players)
        player->update(ms_from_start, current_ms, background);
    song_info.update(current_ms);
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
    if (!transition->is_finished()) {
        transition->draw();
    }
    if (result_transition.is_started) {
        result_transition.draw();
    }
    allnet_indicator.draw();
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
