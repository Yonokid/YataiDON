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
    std::string title = titles.count(lang) ? titles.at(lang) : titles.at("en");
    std::string subtitle = subtitles.count(lang) ? subtitles.at(lang) : "";

    global_data.session_data[(int)PlayerNum::P1].song_title = title;
    global_data.session_data[(int)PlayerNum::P1].song_subtitle = subtitle;
    global_data.session_data[(int)PlayerNum::P2].song_title = title;
    global_data.session_data[(int)PlayerNum::P2].song_subtitle = subtitle;

    if (fs::exists(parser->metadata.wave) && !song_music.has_value()) {
        song_music = audio->load_sound(parser->metadata.wave, "song");
    }

    players.push_back(std::make_unique<Player>(
        parser, PlayerNum::P1,
        global_data.session_data[(int)PlayerNum::P1].selected_difficulty, false,
        global_data.modifiers[(int)PlayerNum::P1]));
    players.push_back(std::make_unique<Player>(
        parser_2p, PlayerNum::P2,
        global_data.session_data[(int)PlayerNum::P2].selected_difficulty, true,
        global_data.modifiers[(int)PlayerNum::P2]));

    start_ms = get_current_ms() - parser->metadata.offset * 1000;
}

std::optional<Screens> Game2PScreen::update() {
    Screen::update();

    double current_time = get_current_ms();
    transition->update(current_time);
    if (!paused) {
        current_ms = current_time - start_ms;
    }
    if (transition->is_finished()) {
        start_song(current_ms);
        global_data.input_locked = 0;
    } else {
        start_ms = current_time - parser->metadata.offset * 1000;
    }

    if (song_started && song_music.has_value() && audio->is_sound_playing(song_music.value())) {
        double audio_ms = audio->get_sound_time_played(song_music.value()) * 1000.0f;
        double audio_ms_adjusted = audio_ms + (parser->metadata.offset * 1000 + start_delay - (double)global_data.config->general.audio_offset);
        if (std::abs(current_ms - audio_ms_adjusted) > Timing::GOOD) {
            current_ms = audio_ms_adjusted;
            start_ms = current_time - current_ms;
        }
    }

    update_background(current_time);

    for (auto& p : players) {
        p->update(current_ms, current_time, background);
    }
    song_info.update(current_time);
    result_transition.update(current_time);

    if (result_transition.is_finished && !audio->is_sound_playing("result_transition")) {
        return on_screen_end(Screens::RESULT_2P);
    }
    else if (current_ms >= players[0]->end_time) {
        global_data.session_data[(int)PlayerNum::P1].result_data = players[0]->get_result_score();
        global_data.session_data[(int)PlayerNum::P2].result_data = players[1]->get_result_score();

        if (end_ms != 0) {
            if (current_time >= end_ms + 1000 && !score_saved) {
                auto save_player_score = [&](int idx, PlayerNum pnum, int player_id) {
                    const auto& rd = global_data.session_data[(int)pnum].result_data;
                    Score score;
                    score.score = rd.score;
                    score.good = rd.good;
                    score.ok = rd.ok;
                    score.bad = rd.bad;
                    score.max_combo = rd.max_combo;
                    score.drumroll = rd.total_drumroll;
                    if (score.ok == 0 && score.bad == 0) score.crown = Crown::DFC;
                    else if (score.bad == 0) score.crown = Crown::FC;
                    else score.crown = Crown::CLEAR;
                    if (score.score >= 1000000) score.rank = Rank::_RAINBOW;
                    else if (score.score >= 950000) score.rank = Rank::_PURPLE;
                    else if (score.score >= 900000) score.rank = Rank::_PINK;
                    else if (score.score >= 800000) score.rank = Rank::_GOLD;
                    else if (score.score >= 700000) score.rank = Rank::_SILVER;
                    else if (score.score >= 600000) score.rank = Rank::_BRONZE;
                    else score.rank = Rank::_WHITE;
                    players[idx]->spawn_ending_anim();
                    scores_manager.save_score(
                        global_data.session_data[(int)pnum].song_hash,
                        global_data.session_data[(int)pnum].selected_difficulty,
                        player_id, score);
                };
                save_player_score(0, PlayerNum::P1, 1);
                save_player_score(1, PlayerNum::P2, 2);
                global_data.songs_played += 1;
                score_saved = true;
            }
            if (current_time >= end_ms + 8533.34) {
                if (!result_transition.is_started) {
                    result_transition.start();
                    audio->play_sound("result_transition", "voice");
                }
            }
        } else {
            end_ms = current_time;
        }
    }

    if (ray::IsKeyPressed(global_data.config->keys.restart_key)) {
        if (song_music.has_value()) audio->stop_sound(song_music.value());
        players.clear();
        parser_2p.reset();
        init_tja(global_data.session_data[(int)PlayerNum::P1].selected_song);
        audio->play_sound("restart", "sound");
        song_started = false;
    }
    if (ray::IsKeyPressed(global_data.config->keys.back_key)) {
        if (song_music.has_value()) audio->stop_sound(song_music.value());
        return on_screen_end(Screens::SONG_SELECT_2P);
    }
    if (ray::IsKeyPressed(global_data.config->keys.pause_key)) {
        pause_song();
    }

    return std::nullopt;
}
