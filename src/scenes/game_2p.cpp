#include "game_2p.h"

#include <algorithm>
#include <iterator>
#include <set>

#include "../libs/input/input.h"
#include "../libs/math/animation.h"

void Game2PScreen::release_chart_resources() {
    parser_2p.reset();
}

static std::set<long long> kusudama_times(SongParser& parser, int difficulty) {
    std::set<long long> times;
    auto add = [&](const NoteList& list) {
        for (const Note& note : list.notes) {
            if (note.type == NoteType::KUSUDAMA) {
                times.insert(std::llround(note.hit_ms));
            }
        }
    };
    auto [notes, branch_m, branch_e, branch_n] = parser.notes_to_position(difficulty);
    add(notes);
    for (const auto* branch : {&branch_m, &branch_e, &branch_n}) {
        for (const NoteList& section : *branch) {
            add(section);
        }
    }
    return times;
}

void Game2PScreen::initialize_chart(const fs::path& song) {
    start_delay = (song.extension() == ".osu") ? 0 : start_delay;
    int delay = static_cast<int>(start_delay);
    parser = SongParser(song, delay, PlayerNum::P1);
    parser_2p = SongParser(song, delay, PlayerNum::P2);

    initialize_song_movie();
    set_session_song_info(PlayerNum::P1);
    set_session_song_info(PlayerNum::P2);

    load_song_audio(AudioLoadMode::Sync);

    const auto p1_index = static_cast<int>(PlayerNum::P1);
    const auto p2_index = static_cast<int>(PlayerNum::P2);

    // a kusudama is shared, so one only the other lane lacks plays as a balloon
    const int diff_1 = global_data.session_data[p1_index].selected_difficulty;
    const int diff_2 = global_data.session_data[p2_index].selected_difficulty;
    std::set<long long> shared;
    std::ranges::set_intersection(kusudama_times(*parser, diff_1), kusudama_times(*parser_2p, diff_2),
                                  std::inserter(shared, shared.begin()));
    Modifiers mods_1 = get_player_modifiers(PlayerNum::P1);
    Modifiers mods_2 = get_player_modifiers(PlayerNum::P2);
    mods_1.shared_kusudamas = shared;
    mods_2.shared_kusudamas = shared;

    players.push_back(std::make_unique<Player>(parser, PlayerNum::P1, diff_1, false, mods_1));
    players.push_back(std::make_unique<Player>(parser_2p, PlayerNum::P2, diff_2, true, mods_2));

    players[0]->kusudama_partner = players[1].get();
    players[1]->kusudama_partner = players[0].get();
    set_players_audio_end();

    reset_song_clock();
}

std::optional<Screens> Game2PScreen::update() {
    if (auto init = Screen::update()) {
        return init;
    }

    double current_time = get_current_ms();
    clock.update(current_time);
    transition->update(current_time);
    update_gameplay(current_time);
    result_transition.update(current_time);

    const double end_time = std::max(players[0]->end_time, players[1]->end_time);

    if (result_transition.is_finished && !audio.is_sound_playing("result_transition")) {
        return on_screen_end(result_screen());
    }

    if (clock.position() >= end_time) {
        const double transition_delay =
            global_data.config->general.fast_transitions ? kFastResultTransitionDelayMs : 8533.34;

        end_song(end_time, transition_delay, BackgroundNotification::Disabled);
    }

    if (check_key_pressed(global_data.config->keys.restart_key)) {
        restart_song();
    }
    if (check_key_pressed(global_data.config->keys.back_key)) {
        return on_screen_end(select_screen());
    }
    if (check_key_pressed(global_data.config->keys.pause_key)) {
        toggle_pause();
    }

    return std::nullopt;
}
