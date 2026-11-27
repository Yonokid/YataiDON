#include "song_select.h"

#include <cmath>
#include <filesystem>

#include "../libs/input/input.h"
#include "../libs/network/network.h"
#include "../libs/network/online_songs.h"
#include "../objects/global/practice_timer.h"

void SongSelectScreen::update_box_bgm() {
    const fs::path& dir = navigator.current_path;
    if (dir == bgm_dir) {
        return;
    }
    bgm_dir = dir;
    std::error_code ec;
    const fs::path box_bgm = dir / "box.ogg";
    const bool has_box_bgm = !dir.empty() && fs::exists(dir / "box.def", ec) && fs::exists(box_bgm, ec);
    if (!has_box_bgm && !bgm_is_box) {
        return;
    }
    fs::path file = box_bgm;
    if (!has_box_bgm) {
        file = bgm_override();
        file = resolve_skin_path(file.empty() ? fs::path("Sounds/song_select/bgm.ogg") : file);
    }
    audio.stop_sound("bgm");
    audio.load_sound(file, "bgm");
    audio.play_sound("bgm", VolumePreset::MUSIC);
    bgm_is_box = has_box_bgm;
}

void SongSelectScreen::on_screen_start() {
    bgm_dir.clear();
    bgm_is_box = false;
    Screen::on_screen_start();
    audio.set_sound_volume("ura_switch", 0.25f);
    audio.set_sound_volume("add_favorite", 3.0f);
    SongBox::reset_bgm_slot();
    if (const fs::path bgm = bgm_override(); !bgm.empty()) {
        audio.load_sound(resolve_skin_path(bgm), "bgm");
    }
    audio.play_sound("bgm", VolumePreset::MUSIC);
    audio.play_sound("voice_enter", VolumePreset::VOICE);
    drag_start_x.reset();
    fling_velocity = 0.0f;

    diff_fade_out = dynamic_cast<FadeAnimation*>(tex.get_animation(2));
    if (!diff_fade_out) {
        throw std::runtime_error("SongSelectScreen: animation 2 is not a FadeAnimation");
    }
    script = std::make_unique<SongSelectScript>();
    t_song_num_bg = tex.get_texture("global/song_num_bg");
    navigator.script = script.get();

    shader = load_shader("shader/dummy.vs", "shader/colortransform.fs");

    state = SongSelectState::BROWSING;

    game_transition.reset();
    dan_transition.reset();

    navigator.hide_dan = hides_dan();
    navigator.hide_collections = guests_only(is_2p_screen());
    navigator.ai_assigned = shows_ai_assigned();
    navigator.is_2p = is_2p_screen();
    navigator.waiwai = is_waiwai_screen();
    navigator.start_path = global_data.config->paths.start_path;
    navigator.init(global_data.config->paths.tja_path);
#ifndef __EMSCRIPTEN__
    stats_stop.request_stop();

    if (stats_future.valid()) {
        stats_future.wait();
    }

    stats_stop = std::stop_source{};

    if (!global_data.config->paths.tja_path.empty()) {
        stats_future =
            std::async(std::launch::async, [paths = global_data.config->paths.tja_path, stop = stats_stop.get_token()] {
                return navigator.get_statistics(paths, stop);
            });
    }
#endif
    navigator.refresh_scores();

    player = std::make_unique<SongSelectPlayer>(global_data.player_num);
    player->script = script.get();

    indicator = std::make_unique<Indicator>(Indicator::State::SELECT);
    song_num = std::make_unique<SongNum>(global_data.credit.songs_played + 1);
    select_timer = std::make_unique<Timer>(100, get_current_ms(), [this] { on_select_timeout(); });
    diff_select_timer = nullptr;
    join_request_ms = -1.0;
}

void SongSelectScreen::select_song(SongBox* song) {
    navigator.add_to_recent(song);
    SongSelectPlayer* p2 = second_player();
    int genre_index = static_cast<int>(song->song_genre_index) - 1;

    if (p2) {
        genre_index = std::max(0, genre_index);

        for (auto* p : {player.get(), p2}) {
            audio.play_sound(p->shared_start_voice, VolumePreset::VOICE);
        }
    }

    for (auto* p : {player.get(), p2}) {
        if (!p) {
            continue;
        }

        const auto player_index = static_cast<int>(p->player_num);
        auto& session_data = global_data.session_data[player_index];
        session_data.selected_song = song->path;
        session_data.selected_collection = song->collection;
        session_data.selected_difficulty = static_cast<int>(p->selected_difficulty);
        session_data.song_hash = song->hash_for(session_data.selected_difficulty);
        session_data.genre_index = genre_index;
        session_data.genre_label = song->song_genre_label;
        global_data.last_difficulty[player_index] = session_data.selected_difficulty;
    }

    start_song_download(song->path);
    game_transition.emplace(song->text_name, song->text_subtitle, false);

    if (exists(song->path.parent_path() / "Loading.png")) {
        game_transition->add_loading_graphic((song->path.parent_path() / "Loading.png").string());
    }

    game_transition->start();
}

void SongSelectScreen::start_song_download(const fs::path& song) {
    song_download = std::async(std::launch::async, online_songs::fetch_song, song);
}

bool SongSelectScreen::song_download_pending() const {
    return song_download.valid() && song_download.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
}

void SongSelectScreen::on_select_timeout() {
    if (state == SongSelectState::BROWSING) {
        state = player->select_song();
    }
}

void SongSelectScreen::on_diff_select_timeout() {
    auto* item = dynamic_cast<SongBox*>(navigator.get_current_item());
    if (item == nullptr || game_transition.has_value()) {
        return;
    }
    // the cursor may rest on back/options when time runs out
    for (SongSelectPlayer* p : {player.get(), second_player()}) {
        if (p != nullptr && p->selected_difficulty < Difficulty::EASY && !p->curr_diffs.empty()) {
            p->selected_difficulty = p->curr_diffs.front();
        }
    }
    select_song(item);
}

void SongSelectScreen::handle_input_browsing(double current_ms) {
    state = player->handle_input_browsing(current_ms);
}

void SongSelectScreen::handle_input_selecting() {
    player->handle_input_selecting();
}

void SongSelectScreen::handle_input_diff_sorting() {
    if (!diff_sort_selector) {
        return;
    }
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
    if (!diff_sort_selector || !tex.options[SCO::ONE_MENU_SORT]) {
        return;
    }
    auto r = diff_sort_selector->take_result();
    if (!r) {
        return;
    }
    diff_sort_selector.reset();
    state = SongSelectState::BROWSING;
    last_diff_sort = {(*r)[0], (*r)[1]};
    last_diff_order = (*r)[2];
    navigator.apply_diff_sort((*r)[0], (*r)[1], (*r)[2]);
}

void SongSelectScreen::handle_input_search() {
    if (!search_box) {
        return;
    }
    auto result = player->handle_input_search();
    search_box->current_search = player->search_string;
    if (result) {
        search_box.reset();
        set_keyboard_visible(false);
        state = SongSelectState::BROWSING;
        // Enter on an empty (or blank) query just closes the box: there is nothing to
        // search for, so do not open the search folder.
        const bool blank = result->find_first_not_of(" \t\r\n") == std::string::npos;
        if (blank) {
            return;
        }
        navigator.current_search = *result;
        navigator.load_current_directory(navigator.get_current_item()->path);
    }
}

void SongSelectScreen::poll_song_jump(double current_ms) {
    // Server drops jump requests after 15s (JUMP_TTL in hiroba), keep this below that
    static constexpr double JUMP_POLL_INTERVAL_MS = 10000.0;
    const std::string& access_code = global_data.config->network.access_code_1;
    if (!access_code.empty() && state == SongSelectState::BROWSING &&
        current_ms - last_jump_poll_ms >= JUMP_POLL_INTERVAL_MS) {
        last_jump_poll_ms = current_ms;
        network.poll_jump(access_code);
    }

    if (auto hash = network.take_song_jump_result()) {
        navigator.jump_to_song(*hash);
    }
}

std::optional<Screens> SongSelectScreen::take_replay_jump() {
    if (auto score_id = network.take_replay_jump_result()) {
        network.request_replay(*score_id);
    }
    if (auto replay = network.take_replay_result()) {
        if (auto song_path = replay->ok ? scores_manager.get_path_by_diff_hash(replay->hash) : std::nullopt) {
            start_song_download(*song_path);
        }
        pending_replay = std::make_shared<const ReplayData>(std::move(*replay));
    }
    if (pending_replay && !song_download_pending()) {
        const auto replay = std::move(pending_replay);
        return start_replay(*replay);
    }
    return std::nullopt;
}

std::optional<Screens> SongSelectScreen::start_replay(const ReplayData& replay) {
    if (!replay.ok) {
        spdlog::error("Replay: failed to fetch from server");
        return std::nullopt;
    }
    auto song_path = scores_manager.get_path_by_diff_hash(replay.hash);
    if (!song_path.has_value()) {
        spdlog::error("Replay: chart {} not found in local library", replay.hash);
        return std::nullopt;
    }

    SessionData& session_data = global_data.session_data[static_cast<int>(global_data.player_num)];
    session_data.selected_song = *song_path;
    session_data.selected_difficulty = replay.difficulty;
    session_data.song_hash = replay.hash;
    session_data.replay = std::make_shared<const ReplayData>(replay);
    return on_screen_end(Screens::GAME);
}

std::optional<Screens> SongSelectScreen::poll_second_player_join(double current_ms) {
    static constexpr double JOIN_WAIT_MS = 1.5 * 1000.0;
    if (join_request_ms >= 0.0) {
        if (current_ms - join_request_ms < JOIN_WAIT_MS) {
            return std::nullopt;
        }
        join_request_ms = -1.0;
        global_data.entry.join_pending = true;
        global_data.entry.joined_seat = join_existing_seat;
        spdlog::info("[2P join] returning to ENTRY, {}P stays in", static_cast<int>(join_existing_seat));
        return on_screen_end(Screens::ENTRY);
    }

    if (!allows_second_player_join()) {
        return std::nullopt;
    }
    if (global_data.credit.songs_played >= 2) {
        return std::nullopt;
    }
    if (game_transition.has_value() || dan_transition.has_value()) {
        return std::nullopt;
    }
    if (navigator.is_processing || navigator.inline_streaming) {
        return std::nullopt;
    }
    if (state != SongSelectState::BROWSING && state != SongSelectState::SONG_SELECTED) {
        return std::nullopt;
    }

    const PlayerNum in = (global_data.player_num == PlayerNum::P2) ? PlayerNum::P2 : PlayerNum::P1;
    const PlayerNum out = (in == PlayerNum::P1) ? PlayerNum::P2 : PlayerNum::P1;

    if (awaiting_join_don_release) {
        if (is_l_don_pressed(out) || is_r_don_pressed(out)) {
            return std::nullopt;
        }
        awaiting_join_don_release = false;
    } else {
        if (!is_l_don_pressed(out) && !is_r_don_pressed(out)) {
            return std::nullopt;
        }
        awaiting_join_don_release = true;
        return std::nullopt;
    }

    join_request_ms = current_ms;
    join_existing_seat = in;
    audio.play_sound("don", VolumePreset::HITSOUND);       // CheckEntry: don_l / don_r
    audio.play_sound("entry_2p_add", VolumePreset::SOUND); // SecondPlayerJoinToEntry
    spdlog::info("[2P join] {}P asked to join from song select; {}P holds", static_cast<int>(out),
                 static_cast<int>(in));
    return std::nullopt;
}

// A flick keeps the wheel stepping at the finger's speed, decaying to 1/e every FLING_DECAY_MS; holding still for
// FLING_HOLD_MS before lifting cancels it.
static constexpr double FLING_DECAY_MS = 325.0;
static constexpr double FLING_HOLD_MS = 80.0;
static constexpr float FLING_STOP_BASE = 0.15f; // px per ms
static constexpr float SWIPE_MIN_BASE = 80.0f;

void SongSelectScreen::step_wheel(float& travel) {
    const float pitch = navigator.box_pitch();
    for (; travel <= -pitch; travel += pitch) {
        audio.play_sound("kat", VolumePreset::HITSOUND);
        navigator.move_right();
    }
    for (; travel >= pitch; travel -= pitch) {
        audio.play_sound("kat", VolumePreset::HITSOUND);
        navigator.move_left();
    }
}

void SongSelectScreen::fling_wheel(double now) {
    if (fling_velocity == 0.0f || state != SongSelectState::BROWSING) {
        fling_velocity = 0.0f;
        return;
    }
    // a frame where input was held back (loading) must not land as one long jump
    const double dt = std::min(now - fling_last_ms, 50.0);
    fling_last_ms = now;
    fling_travel += static_cast<float>(fling_velocity * dt);
    step_wheel(fling_travel);
    fling_velocity *= static_cast<float>(std::exp(-dt / FLING_DECAY_MS));
    if (std::abs(fling_velocity) < FLING_STOP_BASE * tex.screen_scale) {
        fling_velocity = 0.0f;
    }
}

// Browsing: drag the wheel a box per pitch, tap a side box to move to it and the open box to open it. Difficulty
// select: tap a slot. Popups and the sort window still run on the drum, so taps there stand in for it.
void SongSelectScreen::handle_touch() {
    const double now = get_current_ms();
    const auto touch = take_touch_pointer_world();
    if (!touch) {
        drag_start_x.reset();
        fling_wheel(now);
        return;
    }
    fling_velocity = 0.0f;
    if (state == SongSelectState::BROWSING) {
        const float along = navigator.is_vertical() ? touch->pos.y : touch->pos.x;
        const float start = navigator.is_vertical() ? touch->start.y : touch->start.x;
        if (!drag_start_x) {
            drag_start_x = start;
            drag_last = start;
            drag_last_ms = now;
            drag_velocity = 0.0f;
        }
        if (along != drag_last) {
            const double dt = std::max(now - drag_last_ms, 1.0);
            drag_velocity = (0.6f * static_cast<float>((along - drag_last) / dt)) + (0.4f * drag_velocity);
            drag_last = along;
            drag_last_ms = now;
        }
        if (touch->down) {
            float travel = along - *drag_start_x;
            step_wheel(travel);
            *drag_start_x = along - travel;
        } else {
            drag_start_x.reset();
            if (!touch_is_tap(*touch) && now - drag_last_ms < FLING_HOLD_MS) {
                fling_velocity = drag_velocity;
                fling_travel = 0.0f;
                fling_last_ms = now;
            }
        }
        if (!touch_is_tap(*touch)) {
            return;
        }
        if (const auto offset = navigator.offset_at(touch->start); offset && *offset != 0) {
            audio.play_sound("kat", VolumePreset::HITSOUND);
            navigator.move_by(*offset);
        } else if (offset) {
            audio.play_sound("don", VolumePreset::HITSOUND);
            player->touch_select = true;
        }
        return;
    }
    if (state == SongSelectState::SONG_SELECTED && !popup_open() && !touch->down && !touch_is_tap(*touch)) {
        const float dx = touch->pos.x - touch->start.x;
        if (std::abs(dx) > SWIPE_MIN_BASE * tex.screen_scale &&
            std::abs(dx) > std::abs(touch->pos.y - touch->start.y)) {
            player->touch_ura_swipe = true;
        }
        return;
    }
    if (!touch_is_tap(*touch) || state == SongSelectState::SEARCHING) {
        return;
    }
    if (state == SongSelectState::SONG_SELECTED && !popup_open()) {
        player->touch_difficulty = player->difficulty_at(touch->start);
        return;
    }
    press_touch_drum(touch->start);
}

void SongSelectScreen::handle_input(double current_ms) {
    if (navigator.is_processing || navigator.inline_streaming) {
        clear_input_buffers();
        take_touch_pointer();
        return;
    }
    // Ignore input while the difficulty panel is still expanding, matching
    // the cursor which only appears once the animation is done.
    if (state == SongSelectState::SONG_SELECTED && navigator.get_diff_fade_in() < 1.0f) {
        clear_input_buffers();
        take_touch_pointer();
        return;
    }
    handle_touch();
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

void SongSelectScreen::update_selection_ui(double current_time, bool advance_timers) {
    allnet_indicator.update(current_time);
    diff_fade_out->update(current_time);
    script->update(current_time);
    if (uses_session_timer()) {
        if (auto* session = PracticeTimer::active()) {
            session->update(current_time);
        }
    } else if (advance_timers) {
        select_timer->update(current_time);
        if (diff_select_timer != nullptr) {
            diff_select_timer->update(current_time);
        }
    }
    indicator->update(current_time);
    if (search_box) {
        search_box->update(current_time);
    }
}

void SongSelectScreen::update_diff_sort(double current_time) {
    if (navigator.diff_sort_ready() && !diff_sort_selector) {
        if (stats_future.valid()) {
            stats_future.wait();
            cached_stats = stats_future.get();
        }
        const auto& scope = navigator.get_diff_sort_scope();
        diff_sort_selector.emplace(scope ? navigator.get_statistics({}, {}, &*scope) : cached_stats,
                                   last_diff_sort.first, last_diff_sort.second, script.get(), last_diff_order,
                                   is_2p_screen());
    }
    if (diff_sort_selector) {
        state = SongSelectState::DIFF_SORTING;
        diff_sort_selector->update(current_time);
        apply_sort_window_result();
    }
}

void SongSelectScreen::start_search() {
    search_box.emplace();
    // The don key that opened the search (F/J) is also a typed character still
    // queued in raylib's char buffer; drop it so it does not land in the query.
    while (char_pressed() > 0) {
    }
    set_keyboard_visible(true);
}

void SongSelectScreen::on_selection_state_changed(SongSelectState previous, double current_time) {
    script->restart_text_fade();

    if (previous == SongSelectState::SEARCHING) {
        set_keyboard_visible(false);
    }

    if (state == SongSelectState::SONG_SELECTED) {
        diff_select_timer = std::make_unique<Timer>(60, current_time, [this] { on_diff_select_timeout(); });
    } else if (state == SongSelectState::BROWSING) {
        diff_select_timer.reset();
    } else if (state == SongSelectState::SEARCHING) {
        start_search();
    }
}

std::optional<Screens> SongSelectScreen::update() {
    if (auto next = Screen::update()) {
        return next;
    }
    SongSelectState prev_state = state;
    double current_time = get_current_ms();
    if (current_time - last_lua_gc_ms >= 1000.0) {
        last_lua_gc_ms = current_time;
        script_manager.collect_garbage();
    }
    update_selection_ui(current_time, join_request_ms < 0.0);
    update_diff_sort(current_time);

    poll_song_jump(current_time);
    if (auto replay_screen = take_replay_jump()) {
        return replay_screen;
    }
    if (auto join = poll_second_player_join(current_time)) {
        return join;
    }
    if (join_request_ms >= 0.0) {
        clear_input_buffers();
    } else {
        handle_input(current_time);
    }

    player->update(current_time);
    if (player->is_ready && !game_transition.has_value() && join_request_ms < 0.0) {
        if (player->selected_difficulty >= Difficulty::EASY) {
            if (auto* item = dynamic_cast<SongBox*>(navigator.get_current_item())) {
                select_song(item);
            }
        } else if (player->selected_difficulty == Difficulty::BACK) {
            navigator.exit_diff_select();
            state = SongSelectState::BROWSING;
            player->reset_selection();
        }
    }

    if (screen_init) {
        navigator.update(current_time);
        update_box_bgm();
    }

    if (game_transition.has_value() && join_request_ms < 0.0) {
        game_transition->update(current_time);
        if (game_transition->is_finished() && !player->is_voice_playing() && !song_download_pending()) {
            return on_screen_end(get_game_screen_target());
        }
    }

    if (dan_transition.has_value() && join_request_ms < 0.0) {
        dan_transition->update(current_time);
        if (dan_transition->is_finished()) {
            return on_screen_end(Screens::DAN_SELECT);
        }
    }

    if (uses_session_timer()) {
        auto* session = PracticeTimer::active();
        if (session && session->is_finished()) {
            return on_screen_end(Screens::ENTRY);
        }
    }

    if (check_key_pressed(global_data.config->keys.back_key) && global_data.config->limits.song_limit <= 0) {
        return on_screen_end(Screens::ENTRY);
    }

    if (state != prev_state) {
        on_selection_state_changed(prev_state, current_time);

        if (state == SongSelectState::DAN_SELECTED) {
            dan_transition.emplace();
            dan_transition->start();
        } else if (state == SongSelectState::TUTORIAL_SELECTED) {
            return on_screen_end(Screens::TUTORIAL);
        }
    }

    return std::nullopt;
}

Screens SongSelectScreen::on_screen_end(Screens next_screen) {
    navigator.script = nullptr;
    navigator.release_text();
    release_players();
    song_num.reset();
    script.reset();
    script_manager.collect_garbage();
    diff_sort_selector.reset();
    if (search_box) {
        set_keyboard_visible(false);
    }
    stats_stop.request_stop();

    if (stats_future.valid()) {
        stats_future.wait();
    }

    navigator.cancel_loading();
    unload_shader(shader);
    return Screen::on_screen_end(next_screen);
}

void SongSelectScreen::draw_overlays() {
    script->draw_overlays(state);

    if (!uses_session_timer() && !script->draw_song_num()) {
        tex.draw_texture(t_song_num_bg, {.x = -(song_num->width - 127), .x2 = (song_num->width - 127), .fade = 0.75});
        song_num->draw(font_manager.get_text_info("song_num")->x - song_num->width,
                       font_manager.get_text_info("song_num")->y, 1.0);
    }
    if (uses_session_timer()) {
        if (auto* session = PracticeTimer::active()) {
            session->draw();
        }
    } else if (state == SongSelectState::SONG_SELECTED) {
        if (diff_select_timer) {
            diff_select_timer->draw();
        }
    } else {
        select_timer->draw();
    }
    allnet_indicator.draw();
    coin_overlay.draw();
    indicator->draw_on("song_select");
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

    // the personal-best / leaderboard panel gives way to the popups that share its corner
    if (screen_init && !popup_open()) {
        navigator.draw_score_history();
    }

    if (diff_sort_selector) {
        diff_sort_selector->draw();
    }
    if (search_box) {
        search_box->draw(script.get());
    }
    if (game_transition.has_value()) {
        game_transition->draw();
        global_data.in_transition = true;
        coin_overlay.draw();
        global_data.in_transition = false;
    }
    script->draw_top(dan_transition.has_value() ? dan_transition->progress() : -1.0f);

    if (dan_transition.has_value()) {
        dan_transition->draw();
    }
}
