#include "game_2p.h"

void Game2PScreen::on_screen_start() {
    GameScreen::on_screen_start();
    if (!movie.has_value()) {
        background.emplace(PlayerNum::TWO_PLAYER, bpm, scene_preset);
    }
    result_transition = ResultTransition(PlayerNum::TWO_PLAYER);
}

void Game2PScreen::init_tja(fs::path song) {
    int delay = (song.extension() == ".osu") ? 0 : start_delay;
    parser = SongParser(song, delay, PlayerNum::P1);
    parser_2p = SongParser(song, delay, PlayerNum::P2);

    if (fs::exists(parser->metadata.bgmovie)) {
        movie.emplace(parser->metadata.bgmovie);
    }

    auto& titles = parser->metadata.title;
    auto& subtitles = parser->metadata.subtitle;
    const std::string& lang = global_data.config->general.language;
    std::string title = titles.count(lang) ? titles.at(lang) : titles.count("en") ? titles.at("en") : titles.empty() ? "" : titles.begin()->second;
    std::string subtitle = subtitles.count(lang) ? subtitles.at(lang) : "";

    global_data.session_data[(int)PlayerNum::P1].song_title = title;
    global_data.session_data[(int)PlayerNum::P1].song_subtitle = subtitle;
    global_data.session_data[(int)PlayerNum::P2].song_title = title;
    global_data.session_data[(int)PlayerNum::P2].song_subtitle = subtitle;

    if (fs::exists(parser->metadata.wave) && !song_music.has_value()) {
        song_music = audio.load_sound(parser->metadata.wave, "song");
    }

    players.push_back(std::make_unique<Player>(
        parser, PlayerNum::P1,
        global_data.session_data[(int)PlayerNum::P1].selected_difficulty, false,
        get_player_modifiers(PlayerNum::P1)));
    players.push_back(std::make_unique<Player>(
        parser_2p, PlayerNum::P2,
        global_data.session_data[(int)PlayerNum::P2].selected_difficulty, true,
        get_player_modifiers(PlayerNum::P2)));

    start_ms = get_current_ms() - parser->metadata.offset * 1000;
}

std::optional<Screens> Game2PScreen::update() {
    Screen::update();

    double current_time = get_current_ms();
    transition->update(current_time);
    if (!paused) {
        ms_from_start = current_time - start_ms;
    }
    if (transition->is_finished()) {
        start_song(ms_from_start);
        global_data.input_locked = 0;
    } else {
        start_ms = current_time - parser->metadata.offset * 1000;
    }

    resync_song(ms_from_start);

    update_background(current_time);

    for (auto& p : players) {
        p->update(ms_from_start, current_time, background);
    }
    song_info.update(current_time);
    result_transition.update(current_time);

    if (result_transition.is_finished && !audio.is_sound_playing("result_transition")) {
        return on_screen_end(Screens::RESULT_2P);
    }
    else if (ms_from_start >= players[0]->end_time) {
        if (ms_from_start >= players[0]->end_time + 1000 && !score_saved) {
            for (int i = 0; i < 2; i++) {
                global_data.session_data[i + 1].result_data = players[i]->get_result_score();
                save_score(i + 1);
                players[i]->spawn_ending_anim();
            }
            global_data.songs_played += 1;
            score_saved = true;
        }
        if (ms_from_start >= players[0]->end_time + 8533.34) {
            if (!result_transition.is_started) {
                result_transition.start();
                audio.play_sound("result_transition", VolumePreset::VOICE);
            }
        }
    }

    if (ray::IsKeyPressed(global_data.config->keys.restart_key)) {
        if (song_music.has_value()) audio.stop_sound(song_music.value());
        players.clear();
        parser_2p.reset();
        init_tja(global_data.session_data[(int)PlayerNum::P1].selected_song);
        audio.play_sound("restart", VolumePreset::SOUND);
        song_started = false;
    }
    if (ray::IsKeyPressed(global_data.config->keys.back_key)) {
        if (song_music.has_value()) audio.stop_sound(song_music.value());
        return on_screen_end(Screens::SONG_SELECT_2P);
    }
    if (ray::IsKeyPressed(global_data.config->keys.pause_key)) {
        pause_song();
    }

    return std::nullopt;
}
