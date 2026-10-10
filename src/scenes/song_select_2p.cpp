#include "song_select_2p.h"
#include "../libs/input.h"
#include <algorithm>
#include <filesystem>

void SongSelect2PScreen::on_screen_start() {
    SongSelectScreen::on_screen_start();
    player_2 = std::make_unique<SongSelectPlayer>(PlayerNum::P2);
    player_2->script = script.get();
}

static void init_player_diffs(SongSelectPlayer* p, SongBox* song) {
    p->selected_song = true;
    p->curr_diffs = song->get_diffs();
    p->init_diff_cursor();
    p->selected_diff_bounce->start();
    p->selected_diff_fadein->start();
}

void SongSelect2PScreen::handle_input_browsing(double current_ms) {
    SongSelectState s1 = player->handle_input_browsing(current_ms);
    SongSelectState s2 = player_2->handle_input_browsing(current_ms);

    if (s1 == SongSelectState::SONG_SELECTED) {
        if (auto* song = dynamic_cast<SongBox*>(navigator.get_current_item())) {
            init_player_diffs(player_2.get(), song);
            state = s1;
        }
        return;
    }
    if (s2 == SongSelectState::SONG_SELECTED) {
        if (auto* song = dynamic_cast<SongBox*>(navigator.get_current_item())) {
            init_player_diffs(player.get(), song);
            state = s2;
        }
        return;
    }
    if (s1 != SongSelectState::BROWSING) state = s1;
    else if (s2 != SongSelectState::BROWSING) state = s2;
}

void SongSelect2PScreen::handle_input_selecting() {
    bool ura_1p = player->is_ura;
    bool ura_2p = player_2->is_ura;

    if (!player->is_ready) {
        player->handle_input_selecting();
    }
    if (!player_2->is_ready) {
        player_2->handle_input_selecting();
    }

    if (player->is_ura != ura_1p && !player_2->is_ready)   player_2->sync_ura(player->is_ura);
    if (player_2->is_ura != ura_2p && !player->is_ready)   player->sync_ura(player_2->is_ura);
}

void SongSelect2PScreen::select_song(SongBox* song) {
    navigator.add_to_recent(song);

    auto& sd1 = global_data.session_data[(int)PlayerNum::P1];
    sd1.selected_song = song->path;
    sd1.selected_difficulty = (int)player->selected_difficulty;
    sd1.song_hash = song->hash_for(sd1.selected_difficulty);
    sd1.genre_index = std::max(0, (int)song->song_genre_index - 1);
    sd1.genre_label = song->song_genre_label;

    auto& sd2 = global_data.session_data[(int)PlayerNum::P2];
    sd2.selected_song = song->path;
    sd2.selected_difficulty = (int)player_2->selected_difficulty;
    sd2.song_hash = song->hash_for(sd2.selected_difficulty);
    sd2.genre_index = std::max(0, (int)song->song_genre_index - 1);
    sd2.genre_label = song->song_genre_label;

    global_data.last_difficulty[(int)PlayerNum::P1] = sd1.selected_difficulty;
    global_data.last_difficulty[(int)PlayerNum::P2] = sd2.selected_difficulty;

    game_transition.emplace(song->text_name, song->text_subtitle, false);
    if (exists(sd1.selected_song.parent_path() / "Loading.png")) {
        game_transition->add_loading_graphic((sd1.selected_song.parent_path() / "Loading.png").string());
    }
    game_transition->start();
}

std::optional<Screens> SongSelect2PScreen::update() {
    Screen::update();
    SongSelectState prev_state = state;
    double current_time = get_current_ms();
    update_selection_ui(current_time, true);
    update_diff_sort(current_time);

    poll_song_jump(current_time);
    handle_input(current_time);

    player->update(current_time);
    player_2->update(current_time);

    if (player->is_ready && player_2->is_ready && !game_transition.has_value()) {
        if (player->selected_difficulty >= Difficulty::EASY && player_2->selected_difficulty >= Difficulty::EASY) {
            if (auto* item = dynamic_cast<SongBox*>(navigator.get_current_item())) {
                select_song(item);
            }
        }
    }
    if (!game_transition.has_value() &&
        ((player->is_ready && player->selected_difficulty == Difficulty::BACK) ||
         (player_2->is_ready && player_2->selected_difficulty == Difficulty::BACK))) {
        navigator.exit_diff_select();
        state = SongSelectState::BROWSING;
        player->reset_selection();
        player_2->reset_selection();
    }

    if (screen_init) navigator.update(current_time);

    if (game_transition.has_value()) {
        game_transition->update(current_time);
        if (game_transition->is_finished() && !player->is_voice_playing() && !player_2->is_voice_playing()) {
            return on_screen_end(get_game_screen_target());
        }
    }

    if (check_key_pressed(global_data.config->keys.back_key) && !game_transition.has_value()) {
        return on_screen_end(Screens::ENTRY);
    }

    if (state != prev_state) {
        script->restart_text_fade();
        if (state == SongSelectState::SEARCHING) {
            start_search();
        }
    }

    return std::nullopt;
}

void SongSelect2PScreen::draw_overlays() {
    SongSelectScreen::draw_overlays();
}

void SongSelect2PScreen::draw() {
    navigator.draw_background();
    player->draw_background_diffs(state);
    player_2->draw_background_diffs(state);
    bool same_diff = (player->selected_difficulty == player_2->selected_difficulty);
    if (state == SongSelectState::SONG_SELECTED) {
        if (script && script->has_box_bg()) navigator.draw_diff_select_bg();
        if (player->selected_song)   player->try_lua_selector(same_diff, navigator.get_diff_fade_in(), 0);
        if (player_2->selected_song) player_2->try_lua_selector(same_diff, navigator.get_diff_fade_in(), 0);
    }
    if (screen_init) navigator.draw();
    script->draw_footer();

    player->draw(state, same_diff, navigator.get_diff_fade_in());
    player_2->draw(state, same_diff, navigator.get_diff_fade_in());

    draw_overlays();

    const bool popup_open = (player->neiro_selector.has_value()   || player->modifier_selector.has_value()
                          || player_2->neiro_selector.has_value() || player_2->modifier_selector.has_value());
    if (screen_init && !popup_open) navigator.draw_score_history();

    if (diff_sort_selector) diff_sort_selector->draw();
    if (search_box) search_box->draw();
    if (game_transition.has_value()) {
        game_transition->draw();
        global_data.in_transition = true;
        coin_overlay.draw();
        global_data.in_transition = false;
    }
    script->draw_top(-1.0f);
}
