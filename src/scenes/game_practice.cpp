#include "game_practice.h"
#include "../libs/animation.h"
#include "../libs/input.h"
#include <algorithm>
#include <cmath>

// Forward declaration for set_touch_drum_enabled from input.cpp
extern void set_touch_drum_enabled(bool enabled);

void PracticeGameScreen::init_practice_textures() {
    for (int t = 0; t <= 9; ++t) {
        std::string name = "notes/" + std::to_string(t);
        t_notes[t] = tex.has_texture(name) ? tex.get_texture(name) : nullptr;
    }
    t_notes_0 = tex.get_texture("notes/0");
    t_notes_8 = tex.get_texture("notes/8");
    t_notes_9 = tex.get_texture("notes/9");
    t_notes_10 = tex.get_texture("notes/10");
    t_drumroll_big_tail = tex.get_texture("notes/drumroll_big_tail");
    t_drumroll_tail = tex.get_texture("notes/drumroll_tail");
    t_moji = tex.get_texture("notes/moji");
    t_moji_drumroll_mid = tex.get_texture("notes/moji_drumroll_mid");

    t_large_drum = tex.get_texture("practice/large_drum");
    t_pause_don = tex.get_texture("practice/pause_don");
    t_pause_kat = tex.get_texture("practice/pause_kat");
    t_resume_don = tex.get_texture("practice/resume_don");
    t_skip_l_kat = tex.get_texture("practice/skip_l_kat");
    t_skip_r_kat = tex.get_texture("practice/skip_r_kat");
    t_menu_don = tex.get_texture("practice/menu_don");
    t_speed_r_kat = tex.get_texture("practice/speed_r_kat");
    t_speed_l_kat = tex.get_texture("practice/speed_l_kat");
    t_confirm = tex.get_texture("practice/confirm");
    t_delete = tex.get_texture("practice/delete");
    t_finish = tex.get_texture("practice/finish");
    t_jump_point_editing = tex.get_texture("practice/jump_point_editing");
    t_jump_point_arrow = tex.get_texture("practice/jump_point_arrow");
    t_playing = tex.get_texture("practice/playing");
    t_progress_bar_bg = tex.get_texture("practice/progress_bar_bg");
    t_progress_bar = tex.get_texture("practice/progress_bar");
    t_gogo_marker = tex.get_texture("practice/gogo_marker");
    t_jump_point_progress = tex.get_texture("practice/jump_point_progress");
    t_bar_count = tex.get_texture("practice/bar_count");
    t_bar_divider = tex.get_texture("practice/bar_divider");
    t_bar_count_bar = tex.get_texture("practice/bar_count_bar");
    t_song_tempo = tex.get_texture("practice/song_tempo");
    t_dot = tex.get_texture("practice/dot");
    t_multiplier = tex.get_texture("practice/multiplier");
    t_bar_label = tex.get_texture("practice/bar_label");
    t_paused = tex.get_texture("practice/paused");
}

void PracticeGameScreen::on_screen_start() {
    GameScreen::on_screen_start();
    init_practice_textures();
    menu.init_textures();
    pause_don_anim   = (TextureResizeAnimation*)tex.get_animation(67, true);
    pause_kat_anim   = (TextureResizeAnimation*)tex.get_animation(67, true);
    resume_don_anim  = (TextureResizeAnimation*)tex.get_animation(67, true);
    skip_l_kat_anim  = (TextureResizeAnimation*)tex.get_animation(67, true);
    skip_r_kat_anim  = (TextureResizeAnimation*)tex.get_animation(67, true);
    menu_don_anim    = (TextureResizeAnimation*)tex.get_animation(67, true);
    speed_l_kat_anim = (TextureResizeAnimation*)tex.get_animation(67, true);
    speed_r_kat_anim = (TextureResizeAnimation*)tex.get_animation(67, true);
    mark_action_anim = (TextureResizeAnimation*)tex.get_animation(67, true);
    mark_finish_anim = (TextureResizeAnimation*)tex.get_animation(67, true);
    ray::HideCursor();  // Hide cursor when entering practice mode
    set_touch_drum_enabled(true);      // Enable touch drum (input + drawing) when entering practice mode
    init_tja_practice(global_data.session_data[(int)global_data.player_num].selected_song);
}

Screens PracticeGameScreen::on_screen_end(Screens next_screen) {
    scrobble_index = 0;
    scrobble_time = 0;
    jump_bars.fill(-1);
    menu = PracticeMenu();
    scrobble_move = std::make_unique<MoveAnimation>(200.0, 0);
    bars.clear();
    scrobble_note_list.clear();
    markers.clear();
    ray::HideCursor();  // Hide cursor when leaving practice mode
    set_touch_drum_enabled(true);      // Re-enable touch drum (input + drawing) when leaving practice mode
    return GameScreen::on_screen_end(next_screen);
}

void PracticeGameScreen::init_tja(fs::path song) {
    GameScreen::init_tja(song);
    if (!players.empty()) {
        players.back() = std::make_unique<PracticePlayer>(
            parser,
            global_data.player_num,
            global_data.session_data[(int)global_data.player_num].selected_difficulty,
            false,
            get_player_modifiers(global_data.player_num));
        practice_player = static_cast<PracticePlayer*>(players.back().get());
    }
}

void PracticeGameScreen::init_tja_practice(const fs::path& song) {
    int difficulty = global_data.session_data[(int)global_data.player_num].selected_difficulty;
    auto [notes, bm, be, bn] = parser->notes_to_position(difficulty);
    if (auto* tja = std::get_if<TJAParser>(&parser->impl))
        tja->scroll_disabled = true;
    const Modifiers& practice_modifiers = get_player_modifiers(global_data.player_num);
    apply_modifiers(notes, practice_modifiers);

    base_chart = std::move(notes);
    branch_m_all.assign(bm.begin(), bm.end());
    branch_e_all.assign(be.begin(), be.end());
    branch_n_all.assign(bn.begin(), bn.end());
    for (auto* branch : {&branch_m_all, &branch_e_all, &branch_n_all})
        for (NoteList& section : *branch)
            apply_modifiers(section, practice_modifiers);
    branch_display_synced = (size_t)-1;   // force a full rebuild below

    sync_branch_display();

    if (!bars.empty()) {
        scrobble_index = 0;
        scrobble_time = bars[0].hit_ms;
    }
    scrobble_move = std::make_unique<MoveAnimation>(200.0, 0);
}

void PracticeGameScreen::sync_branch_display() {
    size_t resolved = practice_player ? practice_player->branch_history.size() : 0;
    if (branch_display_synced == resolved) return;

    auto ms_of = [&](int index) -> double {
        return (index >= 0 && index < (int)bars.size()) ? bars[index].hit_ms : -1.0;
    };
    double cursor_ms = ms_of(scrobble_index);
    double jump_arrow_ms = ms_of(jump_arrow_bar);
    std::array<double, PracticeMenu::MARK_SLOTS> mark_ms;
    for (size_t i = 0; i < jump_bars.size(); ++i) mark_ms[i] = ms_of(jump_bars[i]);

    scrobble_note_list.assign(base_chart.notes.begin(), base_chart.notes.end());
    bars.clear();
    markers.clear();

    size_t checkpoints = std::max({branch_m_all.size(), branch_e_all.size(), branch_n_all.size()});
    for (size_t i = 0; i < checkpoints; ++i) {
        BranchDifficulty chosen = (practice_player && i < practice_player->branch_history.size())
                                 ? practice_player->branch_history[i]
                                 : BranchDifficulty::MASTER;   // not decided yet: preview the master branch
        const NoteList* section = nullptr;
        if      (chosen == BranchDifficulty::EXPERT && i < branch_e_all.size()) section = &branch_e_all[i];
        else if (chosen == BranchDifficulty::MASTER && i < branch_m_all.size()) section = &branch_m_all[i];
        else if (chosen == BranchDifficulty::NORMAL && i < branch_n_all.size()) section = &branch_n_all[i];

        if (!section) {
            if      (i < branch_m_all.size()) section = &branch_m_all[i];
            else if (i < branch_n_all.size()) section = &branch_n_all[i];
            else if (i < branch_e_all.size()) section = &branch_e_all[i];
        }
        if (!section) continue;

        scrobble_note_list.insert(scrobble_note_list.end(), section->notes.begin(), section->notes.end());
        for (const auto& tl : section->timeline)
            if (tl.gogo_time.has_value() && tl.gogo_time.value()) markers.push_back(tl.start_time);
    }

    auto by_hit_ms = [](const Note& a, const Note& b) { return a.hit_ms < b.hit_ms; };
    std::sort(scrobble_note_list.begin(), scrobble_note_list.end(), by_hit_ms);
    std::sort(markers.begin(), markers.end());
    for (const Note& note : scrobble_note_list)
        if (note.type == NoteType::BARLINE) bars.push_back(note);

    auto reindex = [&](double ms) {
        if (ms < 0) return -1;
        for (int i = 0; i < (int)bars.size(); ++i)
            if (bars[i].hit_ms == ms) return i;
        return -1;
    };
    if (cursor_ms >= 0) scrobble_index = std::max(0, reindex(cursor_ms));
    jump_arrow_bar = reindex(jump_arrow_ms);
    for (size_t i = 0; i < jump_bars.size(); ++i) jump_bars[i] = reindex(mark_ms[i]);

    branch_display_synced = resolved;
}

void PracticeGameScreen::pause_song_practice() {
    paused = !paused;
    if (practice_player) practice_player->paused = paused;
    set_touch_drum_enabled(!paused);   // Enable/disable touch drum (input + drawing)

    // Show/hide mouse cursor when pausing/unpausing
    if (paused) ray::ShowCursor(); else ray::HideCursor();

    if (paused) {
        if (song_music.has_value()) {
            audio.stop_sound(song_music.value());
        }
        pause_time = (int)(get_current_ms() - start_ms);

        if (bars.empty()) return;
        double first_bar_time = bars[0].hit_ms;
        int nearest_bar_index = 0;
        double min_distance = std::numeric_limits<double>::infinity();
        for (int i = 0; i < (int)bars.size(); ++i) {
            double bar_relative_time = bars[i].hit_ms - first_bar_time;
            double distance = std::abs(bar_relative_time - ms_from_start);
            if (distance < min_distance) {
                min_distance = distance;
                nearest_bar_index = i;
            }
        }
        scrobble_index = std::max(0, nearest_bar_index - 1);
        scrobble_time = bars[scrobble_index].hit_ms;
    } else {
        if (bars.empty()) return;

        int resume_bar_index = std::max(0, scrobble_index);
        int previous_bar_index = std::max(0, scrobble_index - global_data.config->general.practice_mode_bar_delay);

        double first_bar_time = bars[0].hit_ms;
        double resume_time = bars[resume_bar_index].hit_ms - first_bar_time + start_delay;
        double start_time  = bars[previous_bar_index].hit_ms - first_bar_time + start_delay;

        if (practice_player) {
            practice_player->seek_to(resume_time);
            practice_player->reset_performance();
        }

        pause_time = (int)start_time;

        if (song_music.has_value()) {
            audio.play_sound(song_music.value(), VolumePreset::MUSIC);
            double seek_sec = (start_time - start_delay) / 1000.0 - parser->metadata.offset;
            audio.seek_sound(song_music.value(), std::max(0.0, seek_sec));
            audio.set_sound_pitch(song_music.value(), song_speed / 10.0f);
        }
        song_started = true;
        start_ms = get_current_ms() - pause_time;
        ms_from_start = start_time;
    }
}

void PracticeGameScreen::resume_practice() {
    pause_song_practice();
    resume_don_anim->start();

    if (practice_player) {
        practice_player->spawn_scrobble_effect(DrumType::DON, Side::LEFT, (int)global_data.player_num - 1);
    }
}

void PracticeGameScreen::open_practice_menu() {
    menu_don_anim->start();
    menu.open_menu();
}

void PracticeGameScreen::change_practice_speed(bool down, bool up) {
    if (down) {
        song_speed = std::max(1, song_speed - 1);
        speed_l_kat_anim->start();
    }

    if (up) {
        song_speed = std::min(99, song_speed + 1);
        speed_r_kat_anim->start();
    }

    if (song_music.has_value()) {
        audio.set_sound_pitch(song_music.value(), song_speed / 10.0f);
    }
}

void PracticeGameScreen::restart_practice() {
    if (song_music.has_value()) {
        audio.stop_sound(song_music.value());
    }
    players.clear();
    init_tja(global_data.session_data[(int)global_data.player_num].selected_song);
    init_tja_practice(global_data.session_data[(int)global_data.player_num].selected_song);
    audio.play_sound("restart", VolumePreset::SOUND);
    song_started = false;
    paused       = false;
    ray::HideCursor();  // Hide cursor on restart
    set_touch_drum_enabled(true);      // Re-enable touch drum (input + drawing) on restart
    menu.close();
    last_resync_ms = 0;
    start_ms = get_current_ms() - parser->metadata.offset * 1000
             - (double)global_data.config->general.audio_offset;
    ms_from_start = get_current_ms() - start_ms;
}

std::optional<Screens> PracticeGameScreen::handle_menu_action(PracticeMenu::Action action) {
    switch (action) {
        case PracticeMenu::Action::END_GAME:
            if (song_music.has_value()) audio.stop_sound(song_music.value());
            return on_screen_end(Screens::ENTRY);
        case PracticeMenu::Action::ANOTHER_SONG:
            if (song_music.has_value()) audio.stop_sound(song_music.value());
            return on_screen_end(Screens::PRACTICE_SELECT);
        case PracticeMenu::Action::RESTART:
            restart_practice();
            return std::nullopt;
        case PracticeMenu::Action::AUTO_ON:
            if (practice_player) practice_player->set_auto_play(true);
            return std::nullopt;
        case PracticeMenu::Action::AUTO_OFF:
            if (practice_player) practice_player->set_auto_play(false);
            return std::nullopt;
        case PracticeMenu::Action::JUMP_TO_MARK: {
            bool any_mark = false;
            for (int b : jump_bars) {
                if (b >= 0 && b < (int)bars.size()) { any_mark = true; break; }
            }
            if (any_mark) menu.open_jump_mode();
            return std::nullopt;
        }
        case PracticeMenu::Action::SET_MARK:
            menu.open_mark_edit();
            return std::nullopt;
        default:
            return std::nullopt;
    }
}

void PracticeGameScreen::animate_scrobble_to(int new_index) {
    if (bars.empty()) return;
    if (scrobble_move->is_started && !scrobble_move->is_finished) {
        scrobble_time = bars[scrobble_index].hit_ms;
    }
    double time_difference = bars[new_index].hit_ms - bars[scrobble_index].hit_ms;
    scrobble_index = new_index;
    scrobble_move = std::make_unique<MoveAnimation>(400.0, (int)time_difference, false, false, 0, 0.0,
                                                    std::nullopt, std::nullopt, EaseType::Quadratic);
    scrobble_move->start();
}

void PracticeGameScreen::scrobble_step_bar(bool right) {
    if (bars.empty()) return;
    audio.play_sound("kat", VolumePreset::SOUND);
    if (right) skip_r_kat_anim->start();
    else       skip_l_kat_anim->start();
    if (practice_player) {
        int player_idx = (int)global_data.player_num - 1;
        if (right) practice_player->spawn_scrobble_effect(DrumType::KAT, Side::RIGHT, player_idx);
        else       practice_player->spawn_scrobble_effect(DrumType::KAT, Side::LEFT,  player_idx);
    }
    int new_index = right ? (scrobble_index + 1) % (int)bars.size()
                          : ((scrobble_index > 0) ? scrobble_index - 1 : (int)bars.size() - 1);
    animate_scrobble_to(new_index);
}

// Check if a point is inside a rectangle
static bool point_in_rect(float x, float y, float rect_x, float rect_y, float rect_w, float rect_h) {
    return x >= rect_x && x <= rect_x + rect_w && y >= rect_y && y <= rect_y + rect_h;
}

// Get the screen-space rectangle for a practice mode UI element
static ray::Rectangle get_practice_button_rect(TextureObject* tex_obj, int index, float scale = 1.0f) {
    if (!tex_obj || index < 0 || static_cast<size_t>(index) >= tex_obj->x.size())
        return {0, 0, 0, 0};

    float x = tex_obj->x[index];
    float y = tex_obj->y[index];
    float w = tex_obj->x2[index] * scale;
    float h = tex_obj->y2[index] * scale;

    // Convert from virtual coordinates to screen coordinates
    int screen_x = virtual_to_screen_x(x);
    int screen_y = virtual_to_screen_y(y);
    int screen_w = static_cast<int>(w * std::min((float)ray::GetScreenWidth() / tex.screen_width,
                                                  (float)ray::GetScreenHeight() / tex.screen_height));
    int screen_h = static_cast<int>(h * std::min((float)ray::GetScreenWidth() / tex.screen_width,
                                                  (float)ray::GetScreenHeight() / tex.screen_height));

    return {static_cast<float>(screen_x), static_cast<float>(screen_y),
            static_cast<float>(screen_w), static_cast<float>(screen_h)};
}

// Handle mouse/touch input for practice mode UI buttons
std::optional<Screens> PracticeGameScreen::handle_mouse_input() {
    if (!paused) return std::nullopt;

    // Get mouse/touch position
    ray::Vector2 mouse_pos = ray::GetMousePosition();
    bool mouse_pressed = ray::IsMouseButtonPressed(ray::MOUSE_BUTTON_LEFT);
    ray::Camera2D camera = {0, 0, 0, 1.0f};

    int player_idx = (global_data.player_num == PlayerNum::P1) ? 0 : 1;
    int other_idx = (global_data.player_num == PlayerNum::P1) ? 1 : 0;

    // If menu is open, let the menu handle mouse input
    if (menu.open) {
        auto action = menu.handle_mouse_input(camera, mouse_pos, mouse_pressed, practice_player && practice_player->is_auto_play());
        auto next = handle_menu_action(action);
        if (next.has_value()) return next;
        return std::nullopt;
    }

    // Also check for touch input (raylib handles touch as mouse on mobile)
    // Note: touch input is already converted to virtual keys in input.cpp,
    // but we also want to allow direct clicking on the practice drums when paused

    // Check resume_don (index = player_idx)
    auto resume_rect = get_practice_button_rect(t_resume_don, player_idx,
        resume_don_anim ? (float)resume_don_anim->attribute : 1.0f);
    if (point_in_rect(mouse_pos.x, mouse_pos.y, resume_rect.x, resume_rect.y, resume_rect.width, resume_rect.height)) {
        if (mouse_pressed) {
            resume_practice();
            audio.play_sound("don", VolumePreset::SOUND);
        }
        return std::nullopt;
    }

    // Check skip_l_kat (index = player_idx * 2)
    auto skip_l_rect = get_practice_button_rect(t_skip_l_kat, player_idx * 2,
        skip_l_kat_anim ? (float)skip_l_kat_anim->attribute : 1.0f);
    if (point_in_rect(mouse_pos.x, mouse_pos.y, skip_l_rect.x, skip_l_rect.y, skip_l_rect.width, skip_l_rect.height)) {
        if (mouse_pressed && !bars.empty()) {
            scrobble_step_bar(false);
        }
        return std::nullopt;
    }

    // Check skip_r_kat (index = player_idx * 2 + 1)
    auto skip_r_rect = get_practice_button_rect(t_skip_r_kat, player_idx * 2 + 1,
        skip_r_kat_anim ? (float)skip_r_kat_anim->attribute : 1.0f);
    if (point_in_rect(mouse_pos.x, mouse_pos.y, skip_r_rect.x, skip_r_rect.y, skip_r_rect.width, skip_r_rect.height)) {
        if (mouse_pressed && !bars.empty()) {
            scrobble_step_bar(true);
        }
        return std::nullopt;
    }

    // Check menu_don (index = other_idx)
    auto menu_rect = get_practice_button_rect(t_menu_don, other_idx,
        menu_don_anim ? (float)menu_don_anim->attribute : 1.0f);
    if (point_in_rect(mouse_pos.x, mouse_pos.y, menu_rect.x, menu_rect.y, menu_rect.width, menu_rect.height)) {
        if (mouse_pressed) {
            open_practice_menu();
            audio.play_sound("don", VolumePreset::SOUND);
        }
        return std::nullopt;
    }

    // Check speed_l_kat (index = other_idx * 2 + 1)
    auto speed_l_rect = get_practice_button_rect(t_speed_l_kat, other_idx * 2 + 1,
        speed_l_kat_anim ? (float)speed_l_kat_anim->attribute : 1.0f);
    if (point_in_rect(mouse_pos.x, mouse_pos.y, speed_l_rect.x, speed_l_rect.y, speed_l_rect.width, speed_l_rect.height)) {
        if (mouse_pressed) {
            change_practice_speed(true, false);
            audio.play_sound("kat", VolumePreset::SOUND);
        }
        return std::nullopt;
    }

    // Check speed_r_kat (index = other_idx * 2)
    auto speed_r_rect = get_practice_button_rect(t_speed_r_kat, other_idx * 2,
        speed_r_kat_anim ? (float)speed_r_kat_anim->attribute : 1.0f);
    if (point_in_rect(mouse_pos.x, mouse_pos.y, speed_r_rect.x, speed_r_rect.y, speed_r_rect.width, speed_r_rect.height)) {
        if (mouse_pressed) {
            change_practice_speed(false, true);
            audio.play_sound("kat", VolumePreset::SOUND);
        }
        return std::nullopt;
    }

    // Also allow clicking on the large drums (index 0 and 1 for player)
    // Left drum (index = player_idx * 2) - DON
    auto drum_l_rect = get_practice_button_rect(t_large_drum, player_idx * 2);
    if (point_in_rect(mouse_pos.x, mouse_pos.y, drum_l_rect.x, drum_l_rect.y, drum_l_rect.width, drum_l_rect.height)) {
        if (mouse_pressed && practice_player) {
            practice_player->spawn_hit_effects(DrumType::DON, Side::LEFT);
            practice_player->spawn_scrobble_effect(DrumType::DON, Side::LEFT, player_idx);
            audio.play_sound("don", VolumePreset::SOUND);
        }
        return std::nullopt;
    }

    // Right drum (index = player_idx * 2 + 1) - DON
    auto drum_r_rect = get_practice_button_rect(t_large_drum, player_idx * 2 + 1);
    if (point_in_rect(mouse_pos.x, mouse_pos.y, drum_r_rect.x, drum_r_rect.y, drum_r_rect.width, drum_r_rect.height)) {
        if (mouse_pressed && practice_player) {
            practice_player->spawn_hit_effects(DrumType::DON, Side::RIGHT);
            practice_player->spawn_scrobble_effect(DrumType::DON, Side::RIGHT, player_idx);
            audio.play_sound("don", VolumePreset::SOUND);
        }
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<Screens> PracticeGameScreen::global_keys_practice() {
    if (check_key_pressed(global_data.config->keys.restart_key)) {
        restart_practice();
        return std::nullopt;
    }

    if (check_key_pressed(global_data.config->keys.back_key)) {
        if (song_music.has_value()) {
            audio.stop_sound(song_music.value());
        }
        return on_screen_end(Screens::PRACTICE_SELECT);
    }

    PlayerNum other_player = (global_data.player_num == PlayerNum::P1) ? PlayerNum::P2 : PlayerNum::P1;
    int other_idx  = (int)other_player - 1;
    int player_idx = (int)global_data.player_num - 1;

    if (!paused) {
        bool other_don   = is_l_don_pressed(other_player) || is_r_don_pressed(other_player);
        bool other_l_kat = is_l_kat_pressed(other_player);
        bool other_r_kat = is_r_kat_pressed(other_player);
        bool other_kat   = other_l_kat || other_r_kat;
        if (other_don || other_kat) {
            pause_song_practice();
            if (other_don) pause_don_anim->start();
            if (other_kat) pause_kat_anim->start();
            if (practice_player) practice_player->spawn_scrobble_effect(DrumType::DON, Side::LEFT, other_idx);
        }
    } else {
        if (menu.open) {
            if (menu.jumping_marks) {
                // Free-roam jump-point navigation: 1P cycles between marks
                // and confirms with don, returning to paused practice.
                bool step_l = is_l_kat_pressed(global_data.player_num);
                bool step_r = is_r_kat_pressed(global_data.player_num);
                bool p1_don = is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num);

                if (!bars.empty() && (step_l || step_r)) {
                    std::vector<int> marks;
                    for (int b : jump_bars) if (b >= 0 && b < (int)bars.size()) marks.push_back(b);
                    std::sort(marks.begin(), marks.end());

                    if (!marks.empty()) {
                        bool right = !step_l && step_r;
                        int new_index;
                        if (right) {
                            auto it = std::upper_bound(marks.begin(), marks.end(), scrobble_index);
                            new_index = (it != marks.end()) ? *it : marks.front();
                        } else {
                            auto it = std::lower_bound(marks.begin(), marks.end(), scrobble_index);
                            new_index = (it == marks.begin()) ? marks.back() : *(--it);
                        }
                        audio.play_sound("kat", VolumePreset::SOUND);
                        if (right) skip_r_kat_anim->start();
                        else       skip_l_kat_anim->start();
                        if (practice_player) {
                            if (right) practice_player->spawn_scrobble_effect(DrumType::KAT, Side::RIGHT, player_idx);
                            else       practice_player->spawn_scrobble_effect(DrumType::KAT, Side::LEFT,  player_idx);
                        }
                        animate_scrobble_to(new_index);
                    }
                }
                if (p1_don) {
                    audio.play_sound("don", VolumePreset::SOUND);
                    mark_finish_anim->start();
                    menu.close();
                }
                return std::nullopt;
            }

            if (menu.editing_marks) {
                // Jump-point editor: 1P moves the bar cursor and toggles a mark
                // at the current bar, 2P confirm exits.
                bool step_l = is_l_kat_pressed(global_data.player_num);
                bool step_r = is_r_kat_pressed(global_data.player_num);
                bool p1_don = is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num);
                bool p2_don = is_l_don_pressed(other_player) || is_r_don_pressed(other_player);

                if (step_l || step_r) {
                    scrobble_step_bar(!step_l && step_r);
                }
                if (p1_don) {
                    audio.play_sound("don", VolumePreset::SOUND);
                    mark_action_anim->start();
                    auto marked = std::find(jump_bars.begin(), jump_bars.end(), scrobble_index);
                    if (marked != jump_bars.end()) {
                        *marked = -1;   // already marked here: remove it
                    } else {
                        auto free_slot = std::find(jump_bars.begin(), jump_bars.end(), -1);
                        if (free_slot != jump_bars.end()) {
                            *free_slot = scrobble_index;   // append
                            jump_arrow_bar = scrobble_index;
                            jump_arrow_anim = std::make_unique<MoveAnimation>(350.0, (int)(184 * tex.screen_scale),
                                                                              false, false, 0, 0.0,
                                                                              std::nullopt, std::nullopt, EaseType::Quadratic);
                            jump_arrow_anim->start();
                        }
                    }
                }
                if (p2_don) {
                    audio.play_sound("don", VolumePreset::SOUND);
                    mark_finish_anim->start();
                    menu.close_mark_edit();
                }
                return std::nullopt;
            }

            // The menu swallows the drums: kat steps, don confirms.
            bool step_l = is_l_kat_pressed(global_data.player_num) || is_l_kat_pressed(other_player);
            bool step_r = is_r_kat_pressed(global_data.player_num) || is_r_kat_pressed(other_player);
            bool don    = is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num) ||
                          is_l_don_pressed(other_player) || is_r_don_pressed(other_player);

            if (step_l || step_r) {
                audio.play_sound("kat", VolumePreset::SOUND);
                menu.step(step_r);
            }
            if (don) {
                audio.play_sound("don", VolumePreset::SOUND);
                bool in_dialog = menu.dialog != PracticeMenu::Dialog::NONE;
                auto action = in_dialog ? menu.confirm()
                                        : menu.activate(practice_player && practice_player->is_auto_play());
                auto next = handle_menu_action(action);
                if (next.has_value()) return next;
            }
            return std::nullopt;
        }

        if (is_l_don_pressed(other_player) || is_r_don_pressed(other_player)) {
            audio.play_sound("don", VolumePreset::SOUND);
            open_practice_menu();
            return std::nullopt;
        }

        if (is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num)) {
            resume_practice();
        }

        bool speed_down = is_l_kat_pressed(other_player);
        bool speed_up   = is_r_kat_pressed(other_player);
        if (paused && (speed_down || speed_up)) {
            audio.play_sound("kat", VolumePreset::SOUND);
            change_practice_speed(speed_down, speed_up);
        }

        bool scrobble_left  = is_l_kat_pressed(global_data.player_num);
        bool scrobble_right = is_r_kat_pressed(global_data.player_num);

        if (!bars.empty() && (scrobble_left || scrobble_right)) {
            scrobble_step_bar(!scrobble_left && scrobble_right);
        }
    }

    return std::nullopt;
}

std::optional<Screens> PracticeGameScreen::update() {
    Screen::update();

    double current_ms = get_frame_ms();
    transition->update(current_ms);
    if (!paused) {
        if (song_started && song_music.has_value() && audio.is_sound_playing(song_music.value())) {
            double audio_ms = audio.get_sound_time_played(song_music.value()) * 1000.0;
            ms_from_start = audio_ms + parser->metadata.offset * 1000.0
                          + start_delay - global_data.config->general.audio_offset;
            start_ms = current_ms - ms_from_start;
        } else {
            ms_from_start = current_ms - start_ms;
        }
    }
    poll_pending_song();
    if (transition->is_finished()) {
        start_song(current_ms);
        reset_input_lock();
    }

    resync_song(current_ms);

    update_background(current_ms);

    // Process practice input before player update so events aren't consumed by handle_input
    auto next_screen = global_keys_practice();
    if (next_screen.has_value()) return next_screen;

    // Handle mouse/touch input for practice mode UI buttons
    next_screen = handle_mouse_input();
    if (next_screen.has_value()) return next_screen;

    for (auto& player : players)
        player->update(ms_from_start, current_ms, background);
    sync_branch_display();
    song_info.update(current_ms);

    scrobble_move->update(current_ms);
    if (scrobble_move->is_started && scrobble_move->is_finished) {
        if (scrobble_index >= 0 && scrobble_index < (int)bars.size())
            scrobble_time = bars[scrobble_index].hit_ms;
        scrobble_move = std::make_unique<MoveAnimation>(200.0, 0);
    }

    if (pause_don_anim)   pause_don_anim->update(current_ms);
    if (pause_kat_anim)   pause_kat_anim->update(current_ms);
    if (resume_don_anim)  resume_don_anim->update(current_ms);
    if (skip_l_kat_anim)  skip_l_kat_anim->update(current_ms);
    if (skip_r_kat_anim)  skip_r_kat_anim->update(current_ms);
    if (menu_don_anim)    menu_don_anim->update(current_ms);
    if (speed_l_kat_anim) speed_l_kat_anim->update(current_ms);
    if (speed_r_kat_anim) speed_r_kat_anim->update(current_ms);
    if (mark_action_anim) mark_action_anim->update(current_ms);
    if (mark_finish_anim) mark_finish_anim->update(current_ms);
    if (jump_arrow_anim) jump_arrow_anim->update(current_ms);

    return std::nullopt;
}

float PracticeGameScreen::get_scrobble_position_x(const Note& note, double current_ms) const {
    float speedx = note.bpm / 240000.0f * note.scroll_x * (tex.screen_width - JudgePos::X);
    float offset_px = 0.0f;
    if (!bars.empty() && scrobble_index < (int)bars.size()) {
        float bar_speedx = bars[scrobble_index].bpm / 240000.0f * bars[scrobble_index].scroll_x * (tex.screen_width - JudgePos::X);
        offset_px = (float)(scrobble_move->attribute * bar_speedx);
    }
    return JudgePos::X + (float)((note.hit_ms - current_ms) * speedx) - offset_px;
}

ray::Color PracticeGameScreen::moji_judgment_color(int note_index) const {
    if (!practice_player) return ray::WHITE;
    auto judgment = practice_player->get_note_judgment(note_index);
    if (!judgment.has_value()) return ray::WHITE;
    switch (judgment.value()) {
        case Judgments::GOOD: return ray::YELLOW;
        case Judgments::OK:   return ray::WHITE;
        case Judgments::BAD:  return ray::SKYBLUE;
    }
    return ray::WHITE;
}

void PracticeGameScreen::draw_bar_scrobble(const Note& bar, double current_ms) const {
    if (!bar.display) return;
    float x = get_scrobble_position_x(bar, current_ms);
    float y_off = (float)((bar.hit_ms - current_ms) * (bar.bpm / 240000.0f * bar.scroll_y * ((tex.screen_width - JudgePos::X) / tex.screen_width) * tex.screen_width));
    float angle = (y_off != 0) ? std::atan2(bar.scroll_y, bar.scroll_x) * 180.0f / PI : 0.0f;
    tex.draw_texture(t_notes_0, {.frame = bar.is_branch_start,
                                  .x = x + tex.skin_config[SC::MOJI_DRUMROLL].x - t_notes_9->width / 2.0f,
                                  .y = y_off + tex.skin_config[SC::MOJI_DRUMROLL].y + (184 * tex.screen_scale),
                                  .rotation = angle});
}

void PracticeGameScreen::draw_drumroll_scrobble(const Note& head, double current_ms) const {
    float start_pos = get_scrobble_position_x(head, current_ms);
    // Find corresponding TAIL
    const Note* tail = nullptr;
    for (const auto& n : scrobble_note_list) {
        // Match by hit_ms, not index: scrobble_note_list is built by
        // concatenating whichever branch section was chosen per checkpoint
        // (each with its own locally-numbered indices) and then re-sorting
        // by hit_ms, so index is not comparable across checkpoint/branch
        // boundaries. hit_ms ordering is what the list is actually sorted by.
        if (n.type == NoteType::TAIL && n.hit_ms > head.hit_ms) { tail = &n; break; }
    }
    if (!tail) return;
    float end_pos = get_scrobble_position_x(*tail, current_ms);
    float length = end_pos - start_pos;
    bool is_big = (head.type == NoteType::ROLL_HEAD_L);
    int color_val = head.color.value_or(255);
    ray::Color color = {255, (unsigned char)color_val, (unsigned char)color_val, 255};
    float y_pos = tex.skin_config[SC::NOTES].y + (184 * tex.screen_scale);
    float moji_y = tex.skin_config[SC::MOJI].y + (184 * tex.screen_scale);
    if (head.display) {
        tex.draw_texture(t_notes_8, {.color = color, .frame = is_big, .x = start_pos, .y = y_pos, .x2 = length + tex.skin_config[SC::DRUMROLL_WIDTH_OFFSET].width});
        if (is_big) tex.draw_texture(t_drumroll_big_tail, {.color = color, .x = end_pos, .y = y_pos});
        else        tex.draw_texture(t_drumroll_tail,     {.color = color, .x = end_pos, .y = y_pos});
        TextureObject* head_tex = t_notes[(int)head.type];
        tex.draw_texture(head_tex, {.color = color, .x = start_pos - t_notes_9->width / 2.0f, .y = y_pos});
    }
    tex.draw_texture(t_moji_drumroll_mid, {.x = start_pos, .y = moji_y, .x2 = length});
    tex.draw_texture(t_moji, {.frame = head.moji, .x = start_pos - t_moji->width / 2.0f, .y = moji_y});
    tex.draw_texture(t_moji, {.frame = tail->moji, .x = end_pos  - t_moji->width / 2.0f, .y = moji_y});
}

void PracticeGameScreen::draw_balloon_scrobble(const Note& head, double current_ms) const {
    float offset = tex.skin_config[SC::BALLOON_OFFSET].x;
    float start_pos = get_scrobble_position_x(head, current_ms);
    const Note* tail = nullptr;
    for (const auto& n : scrobble_note_list) {
        if (n.type == NoteType::TAIL && n.hit_ms > head.hit_ms) { tail = &n; break; }
    }
    if (!tail) return;
    float end_pos   = get_scrobble_position_x(*tail, current_ms);
    float pause_pos = JudgePos::X;
    float y_pos = tex.skin_config[SC::NOTES].y + (184 * tex.screen_scale);
    float position;
    if (current_ms >= tail->hit_ms)   position = end_pos;
    else if (current_ms >= head.hit_ms) position = pause_pos;
    else                                position = start_pos;
    if (head.display) {
        TextureObject* head_tex = t_notes[(int)head.type];
        tex.draw_texture(head_tex, {.x = position - offset - t_notes_9->width / 2.0f, .y = y_pos});
        tex.draw_texture(t_notes_10, {.x = position - offset + t_notes_10->width - t_notes_9->width / 2.0f, .y = y_pos});
    }
    float moji_y = tex.skin_config[SC::MOJI].y + (184 * tex.screen_scale);
    tex.draw_texture(t_moji, {.frame = head.moji, .x = position - t_moji->width / 2.0f, .y = moji_y});
}

void PracticeGameScreen::draw_notes_scrobble(double current_ms) const {
    int scissor_x = players[0]->get_scissor_x();
    int win_w = ray::GetRenderWidth();
    ray::BeginScissorMode(scissor_x, 0, win_w - scissor_x, ray::GetRenderHeight());

    // Draw bars
    for (auto it = bars.rbegin(); it != bars.rend(); ++it) {
        draw_bar_scrobble(*it, current_ms);
    }
    // Draw notes in reverse order
    float y = 184 * tex.screen_scale;
    for (auto it = scrobble_note_list.rbegin(); it != scrobble_note_list.rend(); ++it) {
        const Note& note = *it;
        if (note.type == NoteType::TAIL || note.type == NoteType::BARLINE) continue;

        if (note.color.has_value()) {
            draw_drumroll_scrobble(note, current_ms);
        } else if (note.type == NoteType::BALLOON_HEAD || note.type == NoteType::KUSUDAMA) {
            draw_balloon_scrobble(note, current_ms);
        } else {
            if (note.display) {
                float x = get_scrobble_position_x(note, current_ms);
                TextureObject* note_tex = t_notes[(int)note.type];
                tex.draw_texture(note_tex, {.center = true, .x = x - t_notes_9->width / 2.0f, .y = tex.skin_config[SC::NOTES].y + y});
            }
            float moji_x = get_scrobble_position_x(note, current_ms) - t_moji->width / 2.0f;
            tex.draw_texture(t_moji, {.color = moji_judgment_color(note.index), .frame = note.moji, .x = moji_x, .y = tex.skin_config[SC::MOJI].y + y});
        }
    }

    ray::EndScissorMode();

    if (!bars.empty() && scrobble_index < (int)bars.size()) {
        std::string bar_str = std::to_string(scrobble_index + 1);
        float dw = t_bar_count_bar->x2[0];
        float x  = get_scrobble_position_x(bars[scrobble_index], current_ms) + 4 * tex.screen_scale;
        float y  = tex.skin_config[SC::NOTES].y + 184 * tex.screen_scale
                   + t_bar_count_bar->y2[0] + 2 * tex.screen_scale;
        for (int i = 0; i < (int)bar_str.size(); i++)
            tex.draw_texture(t_bar_count_bar, {.frame = bar_str[i] - '0', .x = x + i * dw, .y = y});
    }
}

void PracticeGameScreen::draw() {
    ray::ClearBackground(ray::BLACK);
    if (movie.has_value()) {
        movie->draw();
    } else if (background.has_value()) {
        background->draw_back();
    }

    players[0]->draw_practice(ms_from_start, 0, 184 * tex.screen_scale, mask_shader, !paused);

    if (background.has_value()) background->draw_fore();

    // Scrobble note overlay when paused
    if (paused) {
        draw_notes_scrobble(scrobble_time);
        if (menu.editing_marks) {
            tex.draw_texture(t_jump_point_editing, {});
        }
        if (jump_arrow_anim && jump_arrow_anim->is_started &&
            jump_arrow_bar >= 0 && jump_arrow_bar < (int)bars.size()) {
            float ax = get_scrobble_position_x(bars[jump_arrow_bar], scrobble_time) - t_jump_point_arrow->width / 2.0f;
            float ay = (float)jump_arrow_anim->attribute - t_jump_point_arrow->height;
            tex.draw_texture(t_jump_point_arrow, {.x = ax, .y = ay});
        }
    }

    // Player::draw_practice already drew the overlays (drum, hit effects, combo) once, before
    // the background's draw_fore, as Player::draw does in normal play.

    tex.draw_texture(t_large_drum, {.index = 0});
    tex.draw_texture(t_large_drum, {.index = 1});

    int other_idx = (global_data.player_num == PlayerNum::P1) ? 1 : 0;
    int player_idx = (global_data.player_num == PlayerNum::P1) ? 0 : 1;
    if (!paused) {
        tex.draw_texture(t_pause_don, {.scale = pause_don_anim  ? (float)pause_don_anim->attribute  : 1.0f, .center = true, .index = other_idx});
        tex.draw_texture(t_pause_kat, {.scale = pause_kat_anim  ? (float)pause_kat_anim->attribute  : 1.0f, .center = true, .index = other_idx * 2});
        tex.draw_texture(t_pause_kat, {.scale = pause_kat_anim  ? (float)pause_kat_anim->attribute  : 1.0f, .center = true, .index = other_idx * 2 + 1});
    } else if (menu.editing_marks || menu.jumping_marks) {
        tex.draw_texture(t_skip_l_kat, {.scale = skip_l_kat_anim ? (float)skip_l_kat_anim->attribute : 1.0f, .center = true, .index = player_idx * 2});
        tex.draw_texture(t_skip_r_kat, {.scale = skip_r_kat_anim ? (float)skip_r_kat_anim->attribute : 1.0f, .center = true, .index = player_idx * 2 + 1});
        if (menu.editing_marks) {
            tex.draw_texture(t_finish, {.scale = mark_finish_anim ? (float)mark_finish_anim->attribute : 1.0f, .center = true, .index = other_idx});
            bool marked = std::find(jump_bars.begin(), jump_bars.end(), scrobble_index) != jump_bars.end();
            tex.draw_texture(marked ? t_delete : t_confirm,
                             {.scale = mark_action_anim ? (float)mark_action_anim->attribute : 1.0f, .center = true, .index = player_idx});
        } else {
            tex.draw_texture(t_finish, {.scale = mark_finish_anim ? (float)mark_finish_anim->attribute : 1.0f, .center = true, .index = player_idx});
        }
    } else {
        tex.draw_texture(t_resume_don,  {.scale = resume_don_anim  ? (float)resume_don_anim->attribute  : 1.0f, .center = true, .index = player_idx});
        tex.draw_texture(t_skip_l_kat,  {.scale = skip_l_kat_anim  ? (float)skip_l_kat_anim->attribute  : 1.0f, .center = true, .index = player_idx * 2});
        tex.draw_texture(t_skip_r_kat,  {.scale = skip_r_kat_anim  ? (float)skip_r_kat_anim->attribute  : 1.0f, .center = true, .index = player_idx * 2 + 1});
        tex.draw_texture(t_menu_don,    {.scale = menu_don_anim    ? (float)menu_don_anim->attribute    : 1.0f, .center = true, .index = other_idx});
        tex.draw_texture(t_speed_r_kat, {.scale = speed_r_kat_anim ? (float)speed_r_kat_anim->attribute : 1.0f, .center = true, .index = other_idx * 2});
        tex.draw_texture(t_speed_l_kat, {.scale = speed_l_kat_anim ? (float)speed_l_kat_anim->attribute : 1.0f, .center = true, .index = other_idx * 2 + 1});
    }

    if (!paused) {
        tex.draw_texture(t_playing, {.fade = 0.5, .index = (int)global_data.player_num - 1});
    }
    // lyric stays at its normal position but on top of the large drums
    if (!players.empty()) players[0]->draw_lyric(184 * tex.screen_scale);

    // Progress bar
    tex.draw_texture(t_progress_bar_bg, {});
    float progress;
    if (paused && !bars.empty()) {
        double raw = scrobble_time + scrobble_move->attribute - bars[0].hit_ms;
        progress = (players.empty() || players[0]->end_time <= 0)
                   ? 0.0f
                   : (float)std::min(raw / players[0]->end_time, 1.0);
    } else {
        progress = players.empty() ? 0.0f : (float)std::min(ms_from_start / players[0]->end_time, 1.0);
    }
    float bar_width = tex.skin_config[SC::PRACTICE_PROGRESS_BAR_WIDTH].width;
    tex.draw_texture(t_progress_bar, {.x2 = progress * bar_width});

    if (!bars.empty()) {
        double first_bar_time = bars[0].hit_ms;
        double total_time = players.empty() ? 1.0 : players[0]->end_time;
        for (double marker : markers) {
            float mx = (float)((marker - first_bar_time) / total_time) * bar_width;
            tex.draw_texture(t_gogo_marker, {.x = mx});
        }
        for (int b : jump_bars) {
            if (b < 0 || b >= (int)bars.size()) continue;
            float jx = (float)((bars[b].hit_ms - first_bar_time) / total_time) * bar_width;
            tex.draw_texture(t_jump_point_progress, {.x = jx});
        }
    }

    if (!bars.empty()) {
        int current_bar;
        if (paused) {
            current_bar = scrobble_index + 1;
        } else {
            current_bar = 0;
            double first_bar_time = bars[0].hit_ms;
            for (int i = 0; i < (int)bars.size(); i++) {
                if (bars[i].hit_ms - first_bar_time <= ms_from_start) current_bar = i + 1;
                else break;
            }
        }
        int total_bars = (int)bars.size();

        float dw   = t_bar_count->x2[0];
        float divw = t_bar_divider->x2[0];
        float dh   = t_bar_count->y2[0];
        float pb_y = t_progress_bar_bg->y[0];
        float pb_h = t_progress_bar_bg->y2[0];
        float digit_y = pb_y + (pb_h - dh) * 0.5f;

        std::string cur_str = std::to_string(current_bar);
        std::string tot_str = std::to_string(total_bars);
        int d1 = song_speed / 10;
        int d2 = song_speed % 10;

        tex.draw_texture(t_song_tempo, {});
        tex.draw_texture(t_dot, {});
        tex.draw_texture(t_multiplier, {});
        tex.draw_texture(t_bar_label, {});

        float st_right  = t_song_tempo->x[0] + t_song_tempo->x2[0];
        float dot_right = t_dot->x[0] + t_dot->x2[0];
        float lbl_right = t_bar_label->x[0] + t_bar_label->x2[0];

        tex.draw_texture(t_bar_count, {.frame = d1, .x = st_right - 4 * tex.screen_scale, .y = digit_y});
        tex.draw_texture(t_bar_count, {.frame = d2, .x = dot_right - 4 * tex.screen_scale, .y = digit_y});

        for (int i = 0; i < (int)cur_str.size(); i++)
            tex.draw_texture(t_bar_count, {.frame = cur_str[i] - '0', .x = lbl_right + i * dw, .y = digit_y});
        float div_x = lbl_right + (float)cur_str.size() * dw;
        tex.draw_texture(t_bar_divider, {.x = div_x, .y = digit_y});
        for (int i = 0; i < (int)tot_str.size(); i++)
            tex.draw_texture(t_bar_count, {.frame = tot_str[i] - '0', .x = div_x + divw + i * dw, .y = digit_y});
    }

    // The song title goes under the paused veil and the practice menu, not over them.
    song_info.draw();

    if (paused) {
        tex.draw_texture(t_paused, {.fade = 0.5});
        if (menu.open && !menu.editing_marks && !menu.jumping_marks) {
            if (menu.dialog != PracticeMenu::Dialog::NONE) menu.draw_dialog();
            else                                           menu.draw();
        }
    }

    draw_overlay(false);
}
