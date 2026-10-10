#include "../libs/localized_text.h"
#include "game_2p.h"
#include "../libs/animation.h"
#include "../libs/input.h"
#include <algorithm>

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
    std::string title = localized_text(titles, lang, LocalizedTextFallback::ENGLISH_OR_FIRST);
    std::string subtitle = localized_text(subtitles, lang, LocalizedTextFallback::NONE);

    global_data.session_data[(int)PlayerNum::P1].song_title = title;
    global_data.session_data[(int)PlayerNum::P1].song_subtitle = subtitle;
    global_data.session_data[(int)PlayerNum::P1].song_subtitle_full_display = parser->metadata.subtitle_full_display;
    global_data.session_data[(int)PlayerNum::P2].song_title = title;
    global_data.session_data[(int)PlayerNum::P2].song_subtitle = subtitle;
    global_data.session_data[(int)PlayerNum::P2].song_subtitle_full_display = parser->metadata.subtitle_full_display;

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

    players[0]->kusudama_partner = players[1].get();
    players[1]->kusudama_partner = players[0].get();
    set_players_audio_end();

    start_ms = get_current_ms() - parser->metadata.offset * 1000 - (double)global_data.config->general.audio_offset;
}

std::optional<Screens> Game2PScreen::update() {
    if (auto init = Screen::update()) {
        return init;
    }

    double current_time = get_current_ms();
    transition->update(current_time);
    if (!paused) {
        ms_from_start = current_time - start_ms;
    }
    update_gameplay(current_time);
    result_transition.update(current_time);

    if (result_transition.is_finished && !audio.is_sound_playing("result_transition")) {
        return on_screen_end(Screens::RESULT_2P);
    }
    else if (ms_from_start >= std::max(players[0]->end_time, players[1]->end_time)) {
        const double end_time = std::max(players[0]->end_time, players[1]->end_time);
        if (ms_from_start >= end_time + 1000 && !score_saved) {
            global_data.session_data[(int)PlayerNum::P1].result_data = players[0]->get_result_score();
            global_data.session_data[(int)PlayerNum::P2].result_data = players[1]->get_result_score();
            save_score(get_player_id(PlayerNum::P1), PlayerNum::P1);
            save_score(get_player_id(PlayerNum::P2), PlayerNum::P2);
            for (int i = 0; i < 2; i++) {
                players[i]->spawn_ending_anim(background.has_value() ? &*background : nullptr);
            }
            global_data.songs_played += 1;
            score_saved = true;
        }
        const double transition_delay = global_data.config->general.fast_transitions
            ? kFastResultTransitionDelayMs : 8533.34;
        if (ms_from_start >= end_time + transition_delay) {
            if (!result_transition.is_started) {
                result_transition.start();
                audio.play_sound("result_transition", VolumePreset::VOICE);
            }
        }
    }

    if (ray::IsKeyPressed(global_data.config->keys.restart_key)) {
        if (song_music.has_value()) {
            audio.stop_sound(song_music.value());
            song_music.reset();
        }
        players.clear();
        parser_2p.reset();
        init_tja(global_data.session_data[(int)PlayerNum::P1].selected_song);
        audio.play_sound("restart", VolumePreset::SOUND);
        song_started = false;
        score_saved = false;
        paused = false;
        pause_time = 0;
        last_resync_ms = 0;
        result_transition = ResultTransition(global_data.player_num);
        start_ms = get_current_ms() - parser->metadata.offset*1000 - (double)global_data.config->general.audio_offset;
        ms_from_start = get_current_ms() - start_ms;
    }
    if (check_key_pressed(global_data.config->keys.back_key)) {
        if (song_music.has_value()) audio.stop_sound(song_music.value());
        return on_screen_end(Screens::SONG_SELECT_2P);
    }
    if (check_key_pressed(global_data.config->keys.pause_key)) {
        pause_song();
    }

    return std::nullopt;
}
