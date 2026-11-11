#include "result.h"

#include "../libs/file/script.h"
#include "../libs/input/input.h"
#include "../libs/network/network.h"
#include "total_result.h"

void ResultScreen::on_screen_start() {
    Screen::on_screen_start();
    credit_next.reset();
    SessionData& session_data = global_data.session_data[static_cast<int>(global_data.player_num)];
    song_title = session_data.song_title;
    song_subtitle = (session_data.song_subtitle_full_display && !session_data.song_subtitle.empty())
                        ? session_data.song_subtitle
                        : std::string();
    song_number = global_data.songs_played; // the game already counted this song
    audio.play_sound("bgm", VolumePreset::MUSIC);
    fade_out = dynamic_cast<FadeAnimation*>(tex.get_animation(0));
    retry_message.reset();
    retrying = false;
    quitting = false;
    fade_in.emplace(global_data.player_num);
    skipped_time = 0;
    skip_enabled_ms = 0;
    fs::path loading_graphic_path = session_data.selected_song.parent_path() / "Loading.png";
    background.reset();
    if (exists(loading_graphic_path)) {
        Texture loaded = load_texture(loading_graphic_path.string().c_str());
        if (loaded.id != 0) {
            loading_graphic.emplace(loaded);
        } else {
            spdlog::error("Failed to load result loading graphic: {}", loading_graphic_path.string());
            background.emplace(global_data.player_num, tex.screen_width);
        }
    } else {
        background.emplace(global_data.player_num, tex.screen_width);
    }
    player_1.emplace(global_data.player_num, false, false);
}

Screens ResultScreen::on_screen_end(Screens next_screen) {
    if (loading_graphic.has_value()) {
        unload_texture(loading_graphic.value());
        loading_graphic.reset();
    }
    // the retry mini-game restarts the song from this session
    if (next_screen != Screens::RETRY_GAME && next_screen != Screens::WAIWAI_RETRY_GAME) {
        reset_session();
    }
    return Screen::on_screen_end(next_screen);
}

double ResultScreen::reveal_end_ms() {
    return player_1.has_value() ? player_1->reveal_end_ms() : 0.0;
}

bool ResultScreen::run_failed() const {
    return !global_data.session_data[static_cast<int>(global_data.player_num)].result_data.cleared;
}

bool ResultScreen::retry_pending() const {
    const bool limit_reached =
        global_data.config->limits.song_limit > 0 && global_data.config->limits.song_limit == global_data.songs_played;
    return allow_retry && !tex.options[SCO::DISABLE_RETRY_GAME] && run_failed() && !limit_reached &&
           script_manager.has_lua_script("retry_game");
}

bool ResultScreen::exiting() const {
    return retrying || (fade_out && fade_out->is_started);
}

void ResultScreen::begin_exit() {
    retrying = retry_pending() && fade_in.has_value();
    if (retrying) {
        audio.play_sound("retry_transition", VolumePreset::SOUND);
        fade_in->start_reverse();
    } else {
        start_fade_out();
    }
}

void ResultScreen::start_fade_out() {
    const bool credit_end = quitting || (global_data.config->limits.song_limit > 0 &&
                                         global_data.config->limits.song_limit == global_data.songs_played);
    if (credit_end && credit_end_screen() == Screens::GAME_OVER && game_over_overlay()) {
        game_over.start();
        if (audio.has_sound("bgm")) {
            audio.stop_sound("bgm");
        }
        return;
    }
    fade_out->start();
}

void ResultScreen::handle_input(double current_ms) {
    if (!fade_out) {
        return;
    }
    if (!menu_confirm_pressed()) {
        return;
    }

    double reveal_end = reveal_end_ms();
    if (reveal_end == 0) {
        if (skipped_time == 0) {
            skipped_time = current_ms;
            audio.play_sound("don", VolumePreset::HITSOUND);
        }
        return;
    }
    const double advance_delay =
        global_data.config->general.fast_transitions ? kFastInputDelayMs : kWaitEffectEndMs + kWaitNextSceneMs;
    if (current_ms >= reveal_end + advance_delay && !exiting()) {
        begin_exit();
        audio.play_sound("don", VolumePreset::HITSOUND);
    }
}

void ResultScreen::update_input_and_timeout(double current_ms) {
    if (!fade_out || exiting()) {
        return;
    }

    if (skip_enabled_ms == 0 && fade_in.has_value() && fade_in->is_finished()) {
        skip_enabled_ms =
            current_ms + (global_data.config->general.fast_transitions ? kFastInputDelayMs : kEnableSkipMs);
    }

    if (skip_enabled_ms > 0 && current_ms >= skip_enabled_ms) {
        if (check_key_pressed(global_data.config->keys.back_key) && reveal_end_ms() > 0) {
            quitting = true;
            start_fade_out();
            audio.play_sound("don", VolumePreset::HITSOUND);
            return;
        }
        handle_input(current_ms);
    }

    double reveal_end = reveal_end_ms();
    if (reveal_end > 0 && !exiting() && current_ms >= reveal_end + kWaitEffectEndMs + kAutoNextSceneMs) {
        begin_exit();
    }
}

Screens ResultScreen::credit_end_screen() {
    if (credit_next) {
        return *credit_next;
    }
    credit_next = begin_total_result();
    const Screens next = *credit_next;
    if (!(network.kinotake_pending() || network.kinotake_tally().any()) ||
        !script_manager.has_lua_script("kinotake_result")) {
        return next;
    }
    global_data.kinotake_next = static_cast<int>(next);
    credit_next = Screens::KINOTAKE_RESULT;
    return *credit_next;
}

std::optional<Screens> ResultScreen::update() {
    Screen::update();
    double current_time = get_current_ms();
    allnet_indicator.update(current_time);
    if (fade_in.has_value()) {
        fade_in->update(current_time);
    }
    if (player_1.has_value()) {
        player_1->update(current_time, fade_in.has_value() && fade_in->is_finished(), skipped_time > 0);
    }

    update_input_and_timeout(current_time);

    if (retrying) {
        if (fade_in->is_finished()) {
            if (!retry_message) {
                retry_message.emplace();
            }
            retry_message->update(current_time);
            if (retry_message->is_finished()) {
                return on_screen_end(Screens::RETRY_GAME);
            }
        }
    } else if (fade_out) {
        fade_out->update(current_time);
        if (fade_out->is_finished) {
            // credit_next is set by start_fade_out() when the credit ends; songs_played was reset by
            // begin_total_result() by then, so re-checking the limit here would send us back to song select
            if (quitting || credit_next.has_value()) {
                return on_screen_end(credit_end_screen());
            }
            global_data.returned_from_result = true;
            return on_screen_end(select_screen());
        }
    }
    return std::nullopt;
}

void ResultScreen::draw_overlay() {
    if (fade_in.has_value()) {
        fade_in->draw();
    }
    if (fade_out) {
        draw_rectangle(0, 0, tex.screen_width, tex.screen_height, fade(Color::BLACK, fade_out->attribute));
    }
    if (retry_message) {
        retry_message->draw();
    }
    coin_overlay.draw();
    allnet_indicator.draw();
}

void ResultScreen::draw_song_info() {
    if (player_1.has_value()) {
        player_1->draw_song_info(song_title, song_subtitle, song_number);
    }
}

void ResultScreen::draw() {
    if (background.has_value()) {
        background->draw();
    }
    if (loading_graphic.has_value()) {
        Rect src = {0, 0, static_cast<float>(loading_graphic.value().width),
                    static_cast<float>(loading_graphic.value().height)};
        Rect dst = {0, 0, static_cast<float>(tex.screen_width), static_cast<float>(tex.screen_height)};
        draw_texture_pro(loading_graphic.value(), src, dst, {0, 0}, 0, Color::WHITE);
    }
    draw_song_info();
    if (player_1.has_value()) {
        player_1->draw();
    }
    draw_overlay();
}
