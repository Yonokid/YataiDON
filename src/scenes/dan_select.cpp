#include "dan_select.h"

#include <algorithm>
#include <filesystem>
#include <iterator>

#include "../libs/file/script.h"
#include "../libs/input/input.h"
#include "../objects/song_select/song_select_script.h"

static constexpr double DAN_INTRO_MS = 80.0 * 1000.0 / 60.0;
static constexpr double DAN_INTRO_FULL_MS = 257.0 * 1000.0 / 60.0;

// ─── DanSelectScreen ─────────────────────────────────────────────────────────

void DanSelectScreen::on_screen_start() {
    Screen::on_screen_start();
    scene.emplace();
    if (!scene->valid()) {
        scene.reset();
    }
    if (scene) {
        if (auto r = scene->ribbon()) {
            dan_navigator.set_ribbon((*r)[0], (*r)[1], (*r)[2], (*r)[3]);
        }
    }
    audio.play_sound("bgm", VolumePreset::MUSIC);
    audio.play_sound("dan_select", VolumePreset::VOICE);

    indicator = std::make_unique<Indicator>(Indicator::State::SELECT);
    confirm_fade = dynamic_cast<FadeAnimation*>(tex.get_animation(8));
    state = SongSelectState::BROWSING;
    dan_transition.reset();
    confirm_index = CONFIRM_NO;
    last_moved = 0;
    modifier_selector.reset();

    {
        auto pd = scores_manager.get_player_data(get_player_id(global_data.player_num));
        chara = make_chara_from_player_data(pd ? &*pd : nullptr);
        if (pd) {
            chara->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
            chara->apply_face(pd->chara_face_index);
        } else {
            chara->set_don_colors(chara_default_color_1(global_data.player_num),
                                  chara_default_color_2(global_data.player_num), {249, 240, 225, 255});
        }
        chara->set_anim(AnimIndex::DON_NORMAL);
        nameplate = Nameplate(pd ? pd->username : "", pd ? pd->title : "", global_data.player_num, pd ? pd->dan : -1,
                              pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    }

    wheel_locked = false;
    wheel_tick_epoch = get_current_ms();
    wheel_tick_seen = 0;

    tex.load_folder("song_select", "modifier");
    if (auto pd = scores_manager.get_player_data(get_player_id(global_data.player_num))) {
        dan_player_data = *pd;
    }

    const auto player_index = static_cast<int>(global_data.player_num);
    SessionData& sd_boot = global_data.session_data[player_index];
    if (sd_boot.selected_dan_folder.empty() && player_index < std::ssize(global_data.dan_folder)) {
        sd_boot.selected_dan_folder = global_data.dan_folder[player_index];
    }
    fs::path dan_folder = sd_boot.selected_dan_folder;

    select_timer.reset();
    timer_started = false;
    timer_fired = false;
    screen_start_ms = get_current_ms();
    scan_ready = false;
    scan_ready_ms = 0;
    scan_begin_ms = screen_start_ms;

    if (legacy_blocking) {
        dan_navigator.init({dan_folder});
        screen_start_ms = get_current_ms();
        scan_ready = true;
        scan_ready_ms = screen_start_ms;
    } else {
        dan_navigator.begin_init({dan_folder});
    }

    const bool from_entry = (global_data.previous_screen == "ENTRY");
    if (script_manager.lua) {
        (*script_manager.lua)["__hss_dan_intro"] = from_entry ? "full" : "open";
    }
    intro_ms = from_entry ? DAN_INTRO_FULL_MS : DAN_INTRO_MS;
    publish_scan_state(screen_start_ms);
}

void DanSelectScreen::publish_scan_state(double current_ms) {
    if (!scan_ready) {
        if (dan_navigator.poll_init()) {
            scan_ready = true;
            scan_ready_ms = current_ms;
        } else if (current_ms - scan_begin_ms > SCAN_TIMEOUT_MS) {
            scan_ready = true;
            scan_ready_ms = current_ms;
            spdlog::error("DanNavigator: course scan still running after {:.0f} ms; "
                          "opening the dojo anyway (the ribbon will fill in when it lands)",
                          SCAN_TIMEOUT_MS);
        }
    } else {
        dan_navigator.poll_init();
    }
    if (script_manager.lua) {
        (*script_manager.lua)["__hss_dan_ready"] = scan_ready;
    }
}

std::optional<Screens> DanSelectScreen::tick_timer(double current_ms) {
    if (!timer_started) {
        const double cover_ms = intro_ms - DAN_INTRO_MS;
        const double reveal_start = std::max(screen_start_ms + cover_ms, scan_ready ? scan_ready_ms : current_ms);
        if (!scan_ready || current_ms < reveal_start + DAN_INTRO_MS) {
            return std::nullopt;
        }
        timer_started = true;
        select_timer = std::make_unique<Timer>(100, current_ms, [this] { timer_fired = true; });
    }
    if (!select_timer) {
        return std::nullopt;
    }
    select_timer->update(current_ms);

    if (!timer_fired) {
        return std::nullopt;
    }
    timer_fired = false;
    if (state == SongSelectState::BROWSING) {
        if (dan_navigator.boxes.empty()) {
            return std::nullopt;
        }
        if (dan_navigator.get_current()->is_back) {
            return start_dan_transition();
        }
        audio.play_sound("don_big", VolumePreset::SOUND);
        open_confirm(current_ms);
        return std::nullopt;
    }
    if (modifier_selector.has_value()) {
        modifier_selector.reset();
    }
    confirm_index = CONFIRM_YES;
    select_timer.reset();
    audio.play_sound("don", VolumePreset::SOUND);
    return on_screen_end(Screens::GAME_DAN);
}

void DanSelectScreen::open_confirm(double current_ms) {
    audio.play_sound("confirm_box", VolumePreset::SOUND);
    audio.play_sound("dan_confirm", VolumePreset::VOICE);
    confirm_fade->start();
    state = SongSelectState::SONG_SELECTED;
    confirm_index = CONFIRM_NO;
    modifier_selector.reset();
    confirm_opened_at = current_ms;
    if (select_timer) {
        const int left = select_timer->time();
        if (left >= 0 && left < 30) {
            select_timer = std::make_unique<Timer>(30, current_ms, [this] { timer_fired = true; });
        }
    }
}

std::optional<Screens> DanSelectScreen::start_dan_transition() {
    // skins without a dan_transition folder go straight back
    if (!tex.has_texture("dan_transition/background")) {
        return on_screen_end(Screens::SONG_SELECT);
    }
    dan_transition.emplace(19);
    if (!dan_transition->start()) {
        dan_transition.reset();
        return on_screen_end(Screens::SONG_SELECT);
    }
    return std::nullopt;
}

Screens DanSelectScreen::on_screen_end(Screens next_screen) {
    dan_navigator.abort_init();
    scene.reset();
    DanBox* current = dan_navigator.get_current();
    if (current && next_screen == Screens::GAME_DAN) {
        SessionData& sd = global_data.session_data[static_cast<int>(global_data.player_num)];
        sd.selected_dan = current->songs;
        sd.selected_dan_exam = current->exams;
        sd.song_title = current->dan_title;
        sd.dan_color = current->dan_color;
        sd.dan_rank = current->dan_rank;
        sd.dan_index = current->dan_index;
        sd.dan_index_max = -1;
        for (const auto& b : dan_navigator.boxes) {
            sd.dan_index_max = std::max(sd.dan_index_max, b->dan_index);
        }
        sd.dan_gaiden = current->gaiden;
        if (!current->songs.empty()) {
            sd.selected_song = current->songs[0].song_path;
        }
    }
    return Screen::on_screen_end(next_screen);
}

void DanSelectScreen::handle_input_browsing(double current_ms) {
    if (dan_navigator.boxes.empty()) {
        return;
    }

    if (wheel_locked) {
        auto tick = static_cast<long long>((current_ms - wheel_tick_epoch) / 100.0);
        if (tick > wheel_tick_seen) {
            wheel_locked = false;
            wheel_tick_seen = tick;
        }
    }

    bool skip_left = check_key_pressed(SDL_SCANCODE_LCTRL);
    bool skip_right = check_key_pressed(SDL_SCANCODE_RCTRL);
    bool nav_left = is_l_kat_pressed(global_data.player_num);
    bool nav_right = is_r_kat_pressed(global_data.player_num);
    bool confirm = is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num);

    if (skip_left) {
        audio.play_sound("skip", VolumePreset::SOUND);
        dan_navigator.skip(-10);
        last_moved = current_ms;
    } else if (skip_right) {
        audio.play_sound("skip", VolumePreset::SOUND);
        dan_navigator.skip(10);
        last_moved = current_ms;
    } else if (!wheel_locked && nav_left) {
        audio.play_sound("kat", VolumePreset::SOUND);
        dan_navigator.move_left();
        last_moved = current_ms;
        wheel_locked = true;
    } else if (!wheel_locked && nav_right) {
        audio.play_sound("kat", VolumePreset::SOUND);
        dan_navigator.move_right();
        last_moved = current_ms;
        wheel_locked = true;
    } else if (!wheel_locked && confirm) {
        audio.play_sound("don", VolumePreset::SOUND);
        if (dan_navigator.get_current()->is_back) {
            back_requested = true;
        } else {
            open_confirm(current_ms);
        }
    }
}

std::optional<Screens> DanSelectScreen::handle_input_selected() {
    constexpr double CONFIRM_INPUT_LOCK_MS = 500.0;
    if (get_current_ms() < confirm_opened_at + CONFIRM_INPUT_LOCK_MS) {
        return std::nullopt;
    }

    const PlayerNum pn = global_data.player_num;
    const bool l_kat = is_l_kat_pressed(pn), r_kat = is_r_kat_pressed(pn);
    const bool don = is_l_don_pressed(pn) || is_r_don_pressed(pn);

    if (modifier_selector.has_value()) {
        if (l_kat) {
            audio.play_sound("kat", VolumePreset::SOUND);
            modifier_selector->left();
        }
        if (r_kat) {
            audio.play_sound("kat", VolumePreset::SOUND);
            modifier_selector->right();
        }
        if (don) {
            audio.play_sound("don", VolumePreset::SOUND);
            modifier_selector->confirm();
        }
        return std::nullopt;
    }

    // Entries run OPTION, YES, NO left to right; a skin that puts YES right of NO flips that order
    const bool yes_right = [] {
        if (!tex.has_texture("dan_select/confirm_box/yes") || !tex.has_texture("dan_select/confirm_box/no")) {
            return false;
        }
        TextureObject* yes = tex.get_texture("dan_select/confirm_box/yes");
        TextureObject* no = tex.get_texture("dan_select/confirm_box/no");
        return yes && no && !yes->x.empty() && !no->x.empty() && yes->x[0] > no->x[0];
    }();
    const int order[3] = {CONFIRM_OPTION, yes_right ? CONFIRM_NO : CONFIRM_YES, yes_right ? CONFIRM_YES : CONFIRM_NO};
    int slot = static_cast<int>(std::find(order, order + 3, confirm_index) - order);
    if (l_kat) {
        audio.play_sound("kat", VolumePreset::SOUND);
        slot = std::max(slot - 1, 0);
    }
    if (r_kat) {
        audio.play_sound("kat", VolumePreset::SOUND);
        slot = std::min(slot + 1, 2);
    }
    confirm_index = order[slot];

    if (!don) {
        return std::nullopt;
    }

    if (confirm_index == CONFIRM_OPTION) {
        audio.play_sound("don", VolumePreset::SOUND);
        modifier_selector.emplace(pn, &dan_player_data);
        return std::nullopt;
    }
    if (confirm_index == CONFIRM_YES) {
        audio.play_sound("don", VolumePreset::SOUND);
        return on_screen_end(Screens::GAME_DAN);
    }
    audio.play_sound("cancel", VolumePreset::SOUND);
    state = SongSelectState::BROWSING;
    return std::nullopt;
}

std::optional<Screens> DanSelectScreen::update() {
    Screen::update();
    double current_ms = get_current_ms();
    publish_scan_state(current_ms);
    allnet_indicator.update(current_ms);
    dan_navigator.update(current_ms);
    indicator->update(current_ms);
    confirm_fade->update(current_ms);
    nameplate.update(current_ms);
    if (chara) {
        chara->update(current_ms);
    }
    if (dan_transition.has_value()) {
        dan_transition->update(current_ms);
        if (dan_transition->is_finished()) {
            return on_screen_end(Screens::SONG_SELECT);
        }
        return std::nullopt;
    }
    if (auto next = tick_timer(current_ms)) {
        return next;
    }

    if (state == SongSelectState::BROWSING) {
        handle_input_browsing(current_ms);
        if (back_requested) {
            back_requested = false;
            return start_dan_transition();
        }
    } else if (state == SongSelectState::SONG_SELECTED) {
        if (modifier_selector.has_value()) {
            modifier_selector->update(current_ms);
            if (modifier_selector->is_finished) {
                scores_manager.save_player_data(dan_player_data);
                modifier_selector.reset();
            }
        }
        if (auto next = handle_input_selected()) {
            return next;
        }
    }

    // Back: left kat in browsing with no songs or specific back logic
    if (check_key_pressed(global_data.config->keys.back_key) && state == SongSelectState::BROWSING) {
        return start_dan_transition();
    }
    return std::nullopt;
}

void DanSelectScreen::draw() {
    if (scene) {
        scene->draw_back();
    }

    coin_overlay.draw();

    dan_navigator.draw();

    // Lumen DonModel rests in the same 200x200 box as song select, so share its slots
    const bool p1 = global_data.player_num == PlayerNum::P1;
    const auto chara_pos = tex.chara_xy(p1 ? "song_select_1p" : "song_select_2p");
    const auto plate_pos = tex.nameplate_xy(p1 ? "song_select_1p" : "song_select_2p");
    if (chara) {
        chara->draw(chara_pos.first, chara_pos.second, 1.0f);
    }
    nameplate.draw(plate_pos.first, plate_pos.second);

    if (state == SongSelectState::SONG_SELECTED) {
        if (scene && confirm_fade->attribute > 0) {
            scene->draw_confirm(confirm_fade->attribute, confirm_index);
        }
        if (modifier_selector.has_value()) {
            sol::protected_function fn_draw_modifier =
                cached_skin_module_fn("song_select/option_panel", "draw_modifier", "__option_panel_draw_modifier");
            if (fn_draw_modifier.valid()) {
                auto r = fn_draw_modifier(&modifier_selector.value());
                if (!r.valid()) {
                    sol::error err = r;
                    spdlog::error("Lua error in option_panel.draw_modifier: {}", err.what());
                }
            }
        }
    }

    indicator->draw_on("dan_select");
    if (scene) {
        scene->draw_title();
    }
    allnet_indicator.draw();

    if (select_timer) {
        select_timer->draw();
    }

    indicator->draw_top();

    if (dan_transition.has_value()) {
        dan_transition->draw();
    }
}
