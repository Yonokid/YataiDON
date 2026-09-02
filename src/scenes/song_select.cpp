#include "song_select.h"
#include "../libs/input.h"
#include "../libs/network.h"
#include <filesystem>

void SongSelectScreen::on_screen_start() {
    Screen::on_screen_start();
    audio.set_sound_volume("ura_switch", 0.25f);
    audio.set_sound_volume("add_favorite", 3.0f);
    SongBox::reset_bgm_slot();
    audio.play_sound("bgm", VolumePreset::MUSIC);
    audio.play_sound("voice_enter", VolumePreset::VOICE);

    diff_fade_out = (FadeAnimation*)tex.get_animation(2);
    script = std::make_unique<SongSelectScript>();
    navigator.script = script.get();

    shader = load_shader("shader/dummy.vs", "shader/colortransform.fs");

    state = SongSelectState::BROWSING;

    game_transition.reset();
    dan_transition.reset();

    navigator.hide_dan = hides_dan();
    navigator.is_2p = is_2p_screen();
    navigator.init(global_data.config->paths.tja_path);
#ifndef __EMSCRIPTEN__
    stats_future = std::async(std::launch::async, [this]() {
        return navigator.get_statistics(global_data.config->paths.tja_path[0]);
    });
#endif
    navigator.refresh_scores();

    player = std::make_unique<SongSelectPlayer>(global_data.player_num);
    player->script = script.get();

    indicator = std::make_unique<Indicator>(Indicator::State::SELECT);
    song_num = std::make_unique<SongNum>(global_data.songs_played + 1);
    select_timer = std::make_unique<Timer>(100, get_current_ms(), [this]() { player->select_song(); });
    diff_select_timer = nullptr;
    join_request_ms = -1.0;
}

void SongSelectScreen::select_song(SongBox* song) {
    navigator.add_to_recent(song);
    SessionData& session_data = global_data.session_data[(int)global_data.player_num];
    session_data.selected_song = song->path;
    session_data.selected_difficulty = (int)player->selected_difficulty;
    session_data.song_hash = song->hash_for(session_data.selected_difficulty);
    session_data.genre_index = (int)song->song_genre_index - 1;
    global_data.last_difficulty[(int)global_data.player_num] = session_data.selected_difficulty;
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

void SongSelectScreen::apply_sort_window_result() {
    if (!diff_sort_selector || !tex.options[SCO::ONE_MENU_SORT]) return;
    auto r = diff_sort_selector->take_result();
    if (!r) return;
    diff_sort_selector.reset();
    state = SongSelectState::BROWSING;
    last_diff_sort  = {(*r)[0], (*r)[1]};
    last_diff_order = (*r)[2];
    navigator.apply_diff_sort((*r)[0], (*r)[1], (*r)[2]);
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

void SongSelectScreen::poll_song_jump(double current_ms) {
    static constexpr double SONG_JUMP_POLL_INTERVAL_MS = 3000.0;
    const std::string& access_code = global_data.config->network.access_code;

    if (!access_code.empty() && state == SongSelectState::BROWSING &&
        current_ms - last_song_jump_poll_ms >= SONG_JUMP_POLL_INTERVAL_MS) {
        last_song_jump_poll_ms = current_ms;
        network.poll_song_jump(access_code);
    }

    if (auto hash = network.take_song_jump_result()) {
        navigator.jump_to_song(*hash);
    }
}

std::optional<Screens> SongSelectScreen::poll_second_player_join(double current_ms) {
    static constexpr double JOIN_WAIT_MS = 1.5 * 1000.0;
    if (join_request_ms >= 0.0) {
        if (current_ms - join_request_ms < JOIN_WAIT_MS) return std::nullopt;
        join_request_ms = -1.0;
        global_data.entry_join_pending = true;
        global_data.entry_joined_seat  = join_existing_seat;
        spdlog::info("[2P join] returning to ENTRY, {}P stays in", (int)join_existing_seat);
        return on_screen_end(Screens::ENTRY);
    }

    if (!allows_second_player_join()) return std::nullopt;
    if (!tex.options[SCO::SONGSELECT_2P_JOIN]) return std::nullopt;
    if (global_data.songs_played >= 2) return std::nullopt;
    if (game_transition.has_value() || dan_transition.has_value()) return std::nullopt;
    if (navigator.is_processing || navigator.inline_streaming) return std::nullopt;
    if (state != SongSelectState::BROWSING && state != SongSelectState::SONG_SELECTED)
        return std::nullopt;

    const PlayerNum in  = (global_data.player_num == PlayerNum::P2) ? PlayerNum::P2 : PlayerNum::P1;
    const PlayerNum out = (in == PlayerNum::P1) ? PlayerNum::P2 : PlayerNum::P1;
    if (!is_l_don_pressed(out) && !is_r_don_pressed(out)) return std::nullopt;
    while (is_l_don_pressed(out) || is_r_don_pressed(out)) {}

    join_request_ms   = current_ms;
    join_existing_seat = in;
    audio.play_sound("don", VolumePreset::SOUND);          // CheckEntry: don_l / don_r
    audio.play_sound("entry_2p_add", VolumePreset::SOUND); // SecondPlayerJoinToEntry
    spdlog::info("[2P join] {}P asked to join from song select; {}P holds", (int)out, (int)in);
    return std::nullopt;
}

void SongSelectScreen::handle_input(double current_ms) {
    if (navigator.is_processing || navigator.inline_streaming) {
        clear_input_buffers();
        return;
    }
    // Ignore input while the difficulty panel is still expanding, matching
    // the cursor which only appears once the animation is done.
    if (state == SongSelectState::SONG_SELECTED && navigator.get_diff_fade_in() < 1.0f) {
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
    allnet_indicator.update(current_time);
    diff_fade_out->update(current_time);
    script->update(current_time);
    if (join_request_ms < 0.0) {
        select_timer->update(current_time);
        if (diff_select_timer != nullptr) diff_select_timer->update(current_time);
    }
    indicator->update(current_time);
    if (search_box) search_box->update(current_time);

    if (navigator.diff_sort_ready() && !diff_sort_selector) {
        if (stats_future.valid()) {
            stats_future.wait();
            cached_stats = stats_future.get();
        }
        diff_sort_selector.emplace(cached_stats, last_diff_sort.first, last_diff_sort.second,
                                   script.get(), last_diff_order);
    }
    if (diff_sort_selector) {
        state = SongSelectState::DIFF_SORTING;
        diff_sort_selector->update(current_time);
        apply_sort_window_result();
    }

    poll_song_jump(current_time);
    if (auto join = poll_second_player_join(current_time)) return join;
    if (join_request_ms >= 0.0) {
        clear_input_buffers();
    } else {
        handle_input(current_time);
    }

    player->update(current_time);
    if (player->is_ready && !game_transition.has_value() && join_request_ms < 0.0) {
        if (player->selected_difficulty >= Difficulty::EASY) {
            BaseBox* item = navigator.get_current_item();
            select_song((SongBox*)item);
        } else if (player->selected_difficulty == Difficulty::BACK) {
            navigator.exit_diff_select();
            state = SongSelectState::BROWSING;
            player->reset_selection();
        }
    }

    if (screen_init) navigator.update(current_time);

    if (game_transition.has_value() && join_request_ms < 0.0) {
        game_transition->update(current_time);
        if (game_transition->is_finished()) {
            return on_screen_end(get_game_screen_target());
        }
    }

    if (dan_transition.has_value() && join_request_ms < 0.0) {
        dan_transition->update(current_time);
        if (dan_transition->is_finished()) {
            return on_screen_end(Screens::DAN_SELECT);
        }
    }

    if (check_key_pressed(global_data.config->keys.back_key) && global_data.config->general.song_limit <= 0) {
        return on_screen_end(Screens::ENTRY);
    }

    if (state != prev_state) {
        script->restart_text_fade();
        if (prev_state == SongSelectState::SEARCHING)
            android_set_keyboard_visible(false);
        if (state == SongSelectState::SONG_SELECTED) {
            diff_select_timer = std::make_unique<Timer>(60, current_time, [this]() { select_song((SongBox*)navigator.get_current_item()); });
        } else if (state == SongSelectState::SEARCHING) {
            search_box.emplace();
            android_set_keyboard_visible(true);
        } else if (state == SongSelectState::DAN_SELECTED) {
            dan_transition.emplace();
            dan_transition->start();
        }
    }

    return std::nullopt;
}

Screens SongSelectScreen::on_screen_end(Screens next_screen) {
    navigator.join_loader();
    ray::UnloadShader(shader);
    return Screen::on_screen_end(next_screen);
}

void SongSelectScreen::draw_overlays() {
    script->draw_overlays(state);

    tex.draw_texture(GLOBAL::SONG_NUM_BG, {.x=-(song_num->width-127), .x2=(song_num->width-127), .fade=0.75});
    song_num->draw(tex.skin_config[SC::SONG_NUM].x-song_num->width, tex.skin_config[SC::SONG_NUM].y, 1.0);
    if (state == SongSelectState::SONG_SELECTED) {
        if (diff_select_timer) diff_select_timer->draw();
    } else {
        select_timer->draw();
    }
    allnet_indicator.draw();
    coin_overlay.draw();
    indicator->draw(tex.skin_config[SC::SONG_SELECT_INDICATOR].x, tex.skin_config[SC::SONG_SELECT_INDICATOR].y);
}

void SongSelectScreen::draw() {
    navigator.draw_background();
    player->draw_background_diffs(state);
    bool in_diff_select = player->selected_song && state == SongSelectState::SONG_SELECTED;
    if (in_diff_select) {
        navigator.draw_diff_select_bg();
        player->try_lua_selector(false, navigator.get_diff_fade_in(), 0);
    }
    navigator.draw();
    script->draw_footer();

    player->draw(state, false, navigator.get_diff_fade_in());

    draw_overlays();

    if (screen_init) navigator.draw_score_history();

    if (diff_sort_selector) diff_sort_selector->draw();
    if (search_box) search_box->draw();
    if (game_transition.has_value()) {
        game_transition->draw();
        global_data.in_transition = true;
        coin_overlay.draw();
        global_data.in_transition = false;
    }
    script->draw_top(dan_transition.has_value() ? (float)dan_transition->progress() : -1.0f);

    if (dan_transition.has_value()) dan_transition->draw();
}
