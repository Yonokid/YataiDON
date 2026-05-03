#include "song_select.h"
#include "../libs/input.h"

void SongSelectScreen::on_screen_start() {
    Screen::on_screen_start();
    audio->set_sound_volume("ura_switch", 0.25f);
    audio->set_sound_volume("add_favorite", 3.0f);
    audio->play_sound("bgm", "music");
    audio->play_sound("voice_enter", "voice");

    diff_fade_out          = (FadeAnimation*)tex.get_animation(2);
    text_fade_in           = (FadeAnimation*)tex.get_animation(4);
    blue_arrow_fade        = (FadeAnimation*)tex.get_animation(29);
    blue_arrow_move        = (MoveAnimation*)tex.get_animation(30);
    blue_arrow_fade->start();
    blue_arrow_move->start();
    text_fade_in->start();

    shader = ray::LoadShader("shader/dummy.vs", "shader/colortransform.fs");

    state = SongSelectState::BROWSING;

    game_transition.reset();
    dan_transition.reset();

    navigator.init(global_data.config->paths.tja_path);
    cached_stats = navigator.get_statistics(global_data.config->paths.tja_path[0]);
    navigator.refresh_scores();

    player = std::make_unique<SongSelectPlayer>(global_data.player_num);

    indicator = std::make_unique<Indicator>(Indicator::State::SELECT);
    song_num = std::make_unique<SongNum>(global_data.songs_played);
    select_timer = std::make_unique<Timer>(100, get_current_ms(), [this]() { player->select_song(); });
    diff_select_timer = nullptr;
}

void SongSelectScreen::select_song(SongBox* song) {
    navigator.add_to_recent(song);
    SessionData& session_data = global_data.session_data[(int)global_data.player_num];
    session_data.selected_song = song->path;
    session_data.selected_difficulty = (int)player->selected_difficulty;
    session_data.song_hash = song->hashes[session_data.selected_difficulty];
    session_data.genre_index = (int)song->genre_index - 1;
    game_transition.emplace(song->text_name, song->text_subtitle, false);
    if (exists(session_data.selected_song.parent_path() / "Loading.png")) {
        game_transition->add_loading_graphic((session_data.selected_song.parent_path() / "Loading.png").string());
    }
    game_transition->start();
}

void SongSelectScreen::handle_input_browsing(double current_ms) {
    state = player->handle_input_browsing(current_ms);
}

void SongSelectScreen::handle_input_selecting() {
    player->handle_input_selecting();
}

void SongSelectScreen::handle_input_diff_sorting() {
    if (!diff_sort_selector) return;
    auto result = player->handle_input_diff_sort(&diff_sort_selector.value());
    if (result) {
        diff_sort_selector.reset();
        state = SongSelectState::BROWSING;
        if (result->first == -1) {
            navigator.cancel_diff_sort();
        } else {
            last_diff_sort = *result;
            navigator.apply_diff_sort(result->first, result->second);
        }
    }
}

void SongSelectScreen::handle_input_search() {
    if (!search_box) return;
    auto result = player->handle_input_search();
    search_box->current_search = player->search_string;
    if (result) {
        navigator.current_search = *result;
        search_box.reset();
        state = SongSelectState::BROWSING;
        navigator.load_current_directory(navigator.get_current_item()->path);
    }
}

void SongSelectScreen::handle_input(double current_ms) {
    if (navigator.is_processing) {
        clear_input_buffers();
        return;
    }
    if (state == SongSelectState::BROWSING) {
        handle_input_browsing(current_ms);
    } else if (state == SongSelectState::SONG_SELECTED) {
        handle_input_selecting();
    } else if (state == SongSelectState::DIFF_SORTING) {
        handle_input_diff_sorting();
    } else if (state == SongSelectState::SEARCHING) {
        handle_input_search();
    }
}

std::optional<Screens> SongSelectScreen::update() {
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
    if (player->is_ready && !game_transition.has_value()) {
        if (player->selected_difficulty >= Difficulty::EASY) {
            BaseBox* item = navigator.get_current_item();
            select_song((SongBox*)item);
        } else if (player->selected_difficulty == Difficulty::BACK) {
            navigator.exit_diff_select();
            state = SongSelectState::BROWSING;
            player->is_ready = false;
            player->selected_song = false;
        }
    }

    if (screen_init) navigator.update(current_time);

    if (game_transition.has_value()) {
        game_transition->update(current_time);
        if (game_transition->is_finished()) {
            return on_screen_end(get_game_screen_target());
        }
    }

    if (dan_transition.has_value()) {
        dan_transition->update(current_time);
        if (dan_transition->is_finished()) {
            return on_screen_end(Screens::DAN_SELECT);
        }
    }

    if (ray::IsKeyPressed(global_data.config->keys.back_key)) {
        return on_screen_end(Screens::ENTRY);
    }

    if (state != prev_state) {
        text_fade_in->start();
        if (state == SongSelectState::SONG_SELECTED) {
            diff_select_timer = std::make_unique<Timer>(60, current_time, [this]() { select_song((SongBox*)navigator.get_current_item()); });
        } else if (state == SongSelectState::SEARCHING) {
            search_box.emplace();
        } else if (state == SongSelectState::DAN_SELECTED) {
            dan_transition.emplace();
            dan_transition->start();
        }
    }

    return std::nullopt;
}

Screens SongSelectScreen::on_screen_end(Screens next_screen) {
    ray::UnloadShader(shader);
    return Screen::on_screen_end(next_screen);
}

void SongSelectScreen::draw_overlays() {
    if (state == SongSelectState::BROWSING) {
        tex.draw_texture(GLOBAL::ARROW, {.x=-((float)blue_arrow_move->attribute * 2), .fade=blue_arrow_fade->attribute, .index=0});
        tex.draw_texture(GLOBAL::ARROW, {.mirror="horizontal", .x=  (float)blue_arrow_move->attribute * 2,  .fade=blue_arrow_fade->attribute, .index=1});
    }

    tex.draw_texture(GLOBAL::SONG_NUM_BG, {.x=-(song_num->width-127), .x2=(song_num->width-127), .fade=0.75});
    song_num->draw(tex.skin_config[SC::SONG_NUM].x-song_num->width, tex.skin_config[SC::SONG_NUM].y, 1.0);
    if (state == SongSelectState::SONG_SELECTED) {
        tex.draw_texture(GLOBAL::DIFFICULTY_SELECT, {.fade=text_fade_in->attribute});
        tex.draw_texture(GLOBAL::SONG_SELECT, {.fade=1 - text_fade_in->attribute});
        if (diff_select_timer) diff_select_timer->draw();
    } else {
        tex.draw_texture(GLOBAL::DIFFICULTY_SELECT, {.fade=1 - text_fade_in->attribute});
        tex.draw_texture(GLOBAL::SONG_SELECT, {.fade=text_fade_in->attribute});
        select_timer->draw();
    }
    allnet_indicator.draw();
    coin_overlay.draw();
    indicator->draw(tex.skin_config[SC::SONG_SELECT_INDICATOR].x, tex.skin_config[SC::SONG_SELECT_INDICATOR].y);
}

void SongSelectScreen::draw() {
    player->draw_background_diffs(state);
    if (screen_init) navigator.draw(player->is_ura);
    tex.draw_texture(GLOBAL::FOOTER, {});

    player->draw(state);

    draw_overlays();

    if (screen_init) navigator.draw_score_history();

    if (diff_sort_selector) diff_sort_selector->draw();
    if (search_box) search_box->draw();
    if (game_transition.has_value()) game_transition->draw();
    if (dan_transition.has_value()) dan_transition->draw();
}
