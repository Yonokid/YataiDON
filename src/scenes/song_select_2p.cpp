#include "song_select_2p.h"

void SongSelect2PScreen::on_screen_start() {
    SongSelectScreen::on_screen_start();
    player_2 = std::make_unique<SongSelectPlayer>(PlayerNum::P2);
}

static void init_player_diffs(SongSelectPlayer* p, SongBox* song) {
    p->selected_song = true;
    p->curr_diffs = song->get_diffs();
    p->selected_diff_bounce->start();
    p->selected_diff_fadein->start();
}

void SongSelect2PScreen::handle_input_browsing(double current_ms) {
    SongSelectState s1 = player->handle_input_browsing(current_ms);
    if (s1 == SongSelectState::SONG_SELECTED) {
        SongBox* song = static_cast<SongBox*>(navigator.get_current_item());
        init_player_diffs(player_2.get(), song);
        state = s1;
        return;
    }
    SongSelectState s2 = player_2->handle_input_browsing(current_ms);
    if (s2 == SongSelectState::SONG_SELECTED) {
        SongBox* song = static_cast<SongBox*>(navigator.get_current_item());
        init_player_diffs(player.get(), song);
        state = s2;
        return;
    }
    if (s1 != SongSelectState::BROWSING) state = s1;
    else if (s2 != SongSelectState::BROWSING) state = s2;
}

void SongSelect2PScreen::handle_input_selecting() {
    if (!player->is_ready) {
        player->handle_input_selecting();
    }
    if (!player_2->is_ready) {
        player_2->handle_input_selecting();
    }
}

void SongSelect2PScreen::select_song(SongBox* song) {
    navigator.add_to_recent(song);

    auto& sd1 = global_data.session_data[(int)PlayerNum::P1];
    sd1.selected_song = song->path;
    sd1.selected_difficulty = (int)player->selected_difficulty;
    sd1.song_hash = song->hashes[sd1.selected_difficulty];
    sd1.genre_index = (int)song->genre_index - 1;

    auto& sd2 = global_data.session_data[(int)PlayerNum::P2];
    sd2.selected_song = song->path;
    sd2.selected_difficulty = (int)player_2->selected_difficulty;
    sd2.song_hash = song->hashes[sd2.selected_difficulty];
    sd2.genre_index = (int)song->genre_index - 1;

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
    diff_fade_out->update(current_time);
    text_fade_in->update(current_time);
    blue_arrow_fade->update(current_time);
    blue_arrow_move->update(current_time);
    select_timer->update(current_time);
    if (diff_select_timer != nullptr) diff_select_timer->update(current_time);
    indicator->update(current_time);
    if (search_box) search_box->update(current_time);

    if (navigator.diff_sort_ready() && !diff_sort_selector) {
        diff_sort_selector.emplace(cached_stats, last_diff_sort.first, last_diff_sort.second);
    }
    if (diff_sort_selector) {
        state = SongSelectState::DIFF_SORTING;
        diff_sort_selector->update(current_time);
    }

    handle_input(current_time);

    player->update(current_time);
    player_2->update(current_time);

    if (player->is_ready && player_2->is_ready && !game_transition.has_value()) {
        if (player->selected_difficulty >= Difficulty::EASY && player_2->selected_difficulty >= Difficulty::EASY) {
            BaseBox* item = navigator.get_current_item();
            select_song((SongBox*)item);
        }
    } else if (player->is_ready && player->selected_difficulty == Difficulty::BACK) {
        navigator.exit_diff_select();
        state = SongSelectState::BROWSING;
        player->is_ready = false;
        player->selected_song = false;
        player_2->is_ready = false;
        player_2->selected_song = false;
    }

    if (screen_init) navigator.update(current_time);

    if (game_transition.has_value()) {
        game_transition->update(current_time);
        if (game_transition->is_finished()) {
            return on_screen_end(get_game_screen_target());
        }
    }

    if (ray::IsKeyPressed(global_data.config->keys.back_key)) {
        return on_screen_end(Screens::ENTRY);
    }

    if (state != prev_state) {
        text_fade_in->start();
        if (state == SongSelectState::SEARCHING) {
            search_box.emplace();
        }
    }

    return std::nullopt;
}

void SongSelect2PScreen::draw_overlays() {
    SongSelectScreen::draw_overlays();
}

void SongSelect2PScreen::draw() {
    player->draw_background_diffs(state);
    player_2->draw_background_diffs(state);
    if (screen_init) navigator.draw(player->is_ura);
    tex.draw_texture(GLOBAL::FOOTER, {});

    bool same_diff = (player->selected_difficulty == player_2->selected_difficulty);
    player->draw(state, same_diff);
    player_2->draw(state, same_diff);

    draw_overlays();

    if (screen_init) navigator.draw_score_history();

    if (diff_sort_selector) diff_sort_selector->draw();
    if (search_box) search_box->draw();
    if (game_transition.has_value()) game_transition->draw();
}
