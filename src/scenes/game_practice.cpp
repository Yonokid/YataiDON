#include "game_practice.h"
#include "../libs/input.h"

void PracticeGameScreen::on_screen_start() {
    GameScreen::on_screen_start();
    if (!movie.has_value()) {
        background.emplace(global_data.player_num, bpm, "PRACTICE");
    }
    pause_don_anim   = (TextureResizeAnimation*)tex.get_animation(67, true);
    pause_kat_anim   = (TextureResizeAnimation*)tex.get_animation(67, true);
    resume_don_anim  = (TextureResizeAnimation*)tex.get_animation(67, true);
    skip_l_kat_anim  = (TextureResizeAnimation*)tex.get_animation(67, true);
    skip_r_kat_anim  = (TextureResizeAnimation*)tex.get_animation(67, true);
    menu_don_anim    = (TextureResizeAnimation*)tex.get_animation(67, true);
    speed_l_kat_anim = (TextureResizeAnimation*)tex.get_animation(67, true);
    speed_r_kat_anim = (TextureResizeAnimation*)tex.get_animation(67, true);
    init_tja_practice(global_data.session_data[(int)global_data.player_num].selected_song);
}

Screens PracticeGameScreen::on_screen_end(Screens next_screen) {
    scrobble_index = 0;
    scrobble_time = 0;
    scrobble_move = std::make_unique<MoveAnimation>(200.0, 0);
    bars.clear();
    scrobble_note_list.clear();
    markers.clear();
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
    // Extract bars and note list for scrobbling from the already-parsed TJA
    int difficulty = global_data.session_data[(int)global_data.player_num].selected_difficulty;
    auto [notes, bm, be, bn] = parser->notes_to_position(difficulty);
    std::get<TJAParser>(parser->impl).scroll_disabled = true;
    apply_modifiers(notes, get_player_modifiers(global_data.player_num));

    bars.clear();
    scrobble_note_list.clear();
    markers.clear();

    for (const auto& note : notes.notes) {
        scrobble_note_list.push_back(note);
        if (note.type == NoteType::BARLINE) {
            bars.push_back(note);
        }
    }

    for (const auto& tl : notes.timeline) {
        if (tl.gogo_time.has_value() && tl.gogo_time.value()) {
            markers.push_back(tl.start_time);
        }
    }

    if (!bars.empty()) {
        scrobble_index = 0;
        scrobble_time = bars[0].hit_ms;
    }
    scrobble_move = std::make_unique<MoveAnimation>(200.0, 0);
}

void PracticeGameScreen::pause_song_practice() {
    paused = !paused;
    if (practice_player) practice_player->paused = paused;

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
    }
}

std::optional<Screens> PracticeGameScreen::global_keys_practice() {
    if (ray::IsKeyPressed(global_data.config->keys.restart_key)) {
        if (song_music.has_value()) {
            audio.stop_sound(song_music.value());
        }
        players.clear();
        init_tja(global_data.session_data[(int)global_data.player_num].selected_song);
        init_tja_practice(global_data.session_data[(int)global_data.player_num].selected_song);
        audio.play_sound("restart", VolumePreset::SOUND);
        song_started = false;
        paused = false;
        return std::nullopt;
    }

    if (ray::IsKeyPressed(global_data.config->keys.back_key)) {
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
            practice_player->spawn_scrobble_effect(DrumType::DON, Side::LEFT, other_idx);
        }
    } else {
        if (is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num)) {
            pause_song_practice();
            resume_don_anim->start();
            practice_player->spawn_scrobble_effect(DrumType::DON, Side::LEFT, player_idx);
        }

        bool speed_down = is_l_kat_pressed(other_player);
        bool speed_up   = is_r_kat_pressed(other_player);
        if (paused && (speed_down || speed_up)) {
            audio.play_sound("kat", VolumePreset::SOUND);
            if (speed_down) { song_speed = std::max(1, song_speed - 1); speed_l_kat_anim->start(); }
            if (speed_up)   { song_speed++;                              speed_r_kat_anim->start(); }
            if (song_music.has_value())
                audio.set_sound_pitch(song_music.value(), song_speed / 10.0f);
        }

        bool scrobble_left  = is_l_kat_pressed(global_data.player_num);
        bool scrobble_right = is_r_kat_pressed(global_data.player_num);

        if (!bars.empty() && (scrobble_left || scrobble_right)) {
            audio.play_sound("kat", VolumePreset::SOUND);
            if (scrobble_left)  skip_l_kat_anim->start();
            if (scrobble_right) skip_r_kat_anim->start();

            if (practice_player) {
                if (scrobble_left)  practice_player->spawn_scrobble_effect(DrumType::KAT, Side::LEFT,  player_idx);
                if (scrobble_right) practice_player->spawn_scrobble_effect(DrumType::KAT, Side::RIGHT, player_idx);
            }

            // Snap any in-progress animation
            if (scrobble_move->is_started && !scrobble_move->is_finished) {
                scrobble_time = bars[scrobble_index].hit_ms;
            }

            int old_index = scrobble_index;
            if (scrobble_left) {
                scrobble_index = (scrobble_index > 0) ? scrobble_index - 1 : (int)bars.size() - 1;
            } else {
                scrobble_index = (scrobble_index + 1) % (int)bars.size();
            }

            double time_difference = bars[scrobble_index].hit_ms - bars[old_index].hit_ms;
            scrobble_move = std::make_unique<MoveAnimation>(400.0, (int)time_difference, false, false, 0, 0.0,
                                                            std::nullopt, std::nullopt, std::string("quadratic"));
            scrobble_move->start();
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
    if (transition->is_finished()) {
        start_song(current_ms);
        global_data.input_locked = 0;
    }

    resync_song(current_ms);

    update_background(current_ms);

    // Process practice input before player update so events aren't consumed by handle_input
    auto next_screen = global_keys_practice();
    if (next_screen.has_value()) return next_screen;

    for (auto& player : players)
        player->update(ms_from_start, current_ms, background);
    song_info.update(current_ms);

    scrobble_move->update(current_ms);
    if (scrobble_move->is_started && scrobble_move->is_finished) {
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

void PracticeGameScreen::draw_bar_scrobble(const Note& bar, double current_ms) const {
    if (!bar.display) return;
    float x = get_scrobble_position_x(bar, current_ms);
    float y_off = (float)((bar.hit_ms - current_ms) * (bar.bpm / 240000.0f * bar.scroll_y * ((tex.screen_width - JudgePos::X) / tex.screen_width) * tex.screen_width));
    float angle = (y_off != 0) ? std::atan2(bar.scroll_y, bar.scroll_x) * 180.0f / PI : 0.0f;
    tex.draw_texture(NOTES::_0, {.frame = bar.is_branch_start,
                                  .x = x + tex.skin_config[SC::MOJI_DRUMROLL].x - tex.textures[NOTES::_9]->width / 2.0f,
                                  .y = y_off + tex.skin_config[SC::MOJI_DRUMROLL].y + (184 * tex.screen_scale),
                                  .rotation = angle});
}

void PracticeGameScreen::draw_drumroll_scrobble(const Note& head, double current_ms) const {
    float start_pos = get_scrobble_position_x(head, current_ms);
    // Find corresponding TAIL
    const Note* tail = nullptr;
    for (const auto& n : scrobble_note_list) {
        if (n.type == NoteType::TAIL && n.index > head.index) { tail = &n; break; }
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
        tex.draw_texture(NOTES::_8, {.color = color, .frame = is_big, .x = start_pos, .y = y_pos, .x2 = length + tex.skin_config[SC::DRUMROLL_WIDTH_OFFSET].width});
        if (is_big) tex.draw_texture(NOTES::DRUMROLL_BIG_TAIL, {.color = color, .x = end_pos, .y = y_pos});
        else        tex.draw_texture(NOTES::DRUMROLL_TAIL,     {.color = color, .x = end_pos, .y = y_pos});
        TexID head_tex = tex_id_map.count("notes/" + std::to_string((int)head.type))
                         ? tex.get_enum("notes/" + std::to_string((int)head.type)) : TexID(0);
        tex.draw_texture(head_tex, {.color = color, .x = start_pos - tex.textures[NOTES::_9]->width / 2.0f, .y = y_pos});
    }
    tex.draw_texture(NOTES::MOJI_DRUMROLL_MID, {.x = start_pos, .y = moji_y, .x2 = length});
    tex.draw_texture(NOTES::MOJI, {.frame = head.moji, .x = start_pos - tex.textures[NOTES::MOJI]->width / 2.0f, .y = moji_y});
    tex.draw_texture(NOTES::MOJI, {.frame = tail->moji, .x = end_pos  - tex.textures[NOTES::MOJI]->width / 2.0f, .y = moji_y});
}

void PracticeGameScreen::draw_balloon_scrobble(const Note& head, double current_ms) const {
    float offset = tex.skin_config[SC::BALLOON_OFFSET].x;
    float start_pos = get_scrobble_position_x(head, current_ms);
    const Note* tail = nullptr;
    for (const auto& n : scrobble_note_list) {
        if (n.type == NoteType::TAIL && n.index > head.index) { tail = &n; break; }
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
        TexID head_tex = tex_id_map.count("notes/" + std::to_string((int)head.type))
                         ? tex.get_enum("notes/" + std::to_string((int)head.type)) : TexID(0);
        tex.draw_texture(head_tex, {.x = position - offset - tex.textures[NOTES::_9]->width / 2.0f, .y = y_pos});
        tex.draw_texture(NOTES::_10, {.x = position - offset + tex.textures[NOTES::_10]->width - tex.textures[NOTES::_9]->width / 2.0f, .y = y_pos});
    }
    float moji_y = tex.skin_config[SC::MOJI].y + (184 * tex.screen_scale);
    tex.draw_texture(NOTES::MOJI, {.frame = head.moji, .x = position - tex.textures[NOTES::MOJI]->width / 2.0f, .y = moji_y});
}

void PracticeGameScreen::draw_notes_scrobble(double current_ms) const {
    int scissor_x = players[0]->get_scissor_x();
    int win_w = ray::GetScreenWidth();
    ray::BeginScissorMode(scissor_x, 0, win_w - scissor_x, ray::GetScreenHeight());

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
                TexID note_tex = tex_id_map.count("notes/" + std::to_string((int)note.type))
                                 ? tex.get_enum("notes/" + std::to_string((int)note.type)) : TexID(0);
                tex.draw_texture(note_tex, {.center = true, .x = x - tex.textures[NOTES::_9]->width / 2.0f, .y = tex.skin_config[SC::NOTES].y + y});
            }
            float moji_x = get_scrobble_position_x(note, current_ms) - tex.textures[NOTES::MOJI]->width / 2.0f;
            tex.draw_texture(NOTES::MOJI, {.frame = note.moji, .x = moji_x, .y = tex.skin_config[SC::MOJI].y + y});
        }
    }

    ray::EndScissorMode();

    if (!bars.empty() && scrobble_index < (int)bars.size()) {
        std::string bar_str = std::to_string(scrobble_index + 1);
        float dw = tex.textures[PRACTICE::BAR_COUNT_BAR]->x2[0];
        float x  = get_scrobble_position_x(bars[scrobble_index], current_ms) + 4 * tex.screen_scale;
        float y  = tex.skin_config[SC::NOTES].y + 184 * tex.screen_scale
                   + tex.textures[PRACTICE::BAR_COUNT_BAR]->y2[0] + 2 * tex.screen_scale;
        for (int i = 0; i < (int)bar_str.size(); i++)
            tex.draw_texture(PRACTICE::BAR_COUNT_BAR, {.frame = bar_str[i] - '0', .x = x + i * dw, .y = y});
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
    }

    // Player overlays after practice graphics (hit effects, combos, etc.)
    if (players.size() == 1) {
        players[0]->draw_overlays(184 * tex.screen_scale, mask_shader);
    }

    tex.draw_texture(PRACTICE::LARGE_DRUM, {.index = 0});
    tex.draw_texture(PRACTICE::LARGE_DRUM, {.index = 1});

    int other_idx = (global_data.player_num == PlayerNum::P1) ? 1 : 0;
    int player_idx = (global_data.player_num == PlayerNum::P1) ? 0 : 1;
    if (!paused) {
        tex.draw_texture(PRACTICE::PAUSE_DON, {.scale = pause_don_anim  ? (float)pause_don_anim->attribute  : 1.0f, .center = true, .index = other_idx});
        tex.draw_texture(PRACTICE::PAUSE_KAT, {.scale = pause_kat_anim  ? (float)pause_kat_anim->attribute  : 1.0f, .center = true, .index = other_idx * 2});
        tex.draw_texture(PRACTICE::PAUSE_KAT, {.scale = pause_kat_anim  ? (float)pause_kat_anim->attribute  : 1.0f, .center = true, .index = other_idx * 2 + 1});
    } else {
        tex.draw_texture(PRACTICE::RESUME_DON,  {.scale = resume_don_anim  ? (float)resume_don_anim->attribute  : 1.0f, .center = true, .index = player_idx});
        tex.draw_texture(PRACTICE::SKIP_L_KAT,  {.scale = skip_l_kat_anim  ? (float)skip_l_kat_anim->attribute  : 1.0f, .center = true, .index = player_idx * 2});
        tex.draw_texture(PRACTICE::SKIP_R_KAT,  {.scale = skip_r_kat_anim  ? (float)skip_r_kat_anim->attribute  : 1.0f, .center = true, .index = player_idx * 2 + 1});
        tex.draw_texture(PRACTICE::MENU_DON,    {.scale = menu_don_anim    ? (float)menu_don_anim->attribute    : 1.0f, .center = true, .index = other_idx});
        tex.draw_texture(PRACTICE::SPEED_L_KAT, {.scale = speed_l_kat_anim ? (float)speed_l_kat_anim->attribute : 1.0f, .center = true, .index = other_idx * 2});
        tex.draw_texture(PRACTICE::SPEED_R_KAT, {.scale = speed_r_kat_anim ? (float)speed_r_kat_anim->attribute : 1.0f, .center = true, .index = other_idx * 2 + 1});
    }

    if (!paused) {
        tex.draw_texture(PRACTICE::PLAYING, {.fade = 0.5, .index = (int)global_data.player_num - 1});
    }

    // Progress bar
    tex.draw_texture(PRACTICE::PROGRESS_BAR_BG, {});
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
    tex.draw_texture(PRACTICE::PROGRESS_BAR, {.x2 = progress * bar_width});

    if (!bars.empty()) {
        double first_bar_time = bars[0].hit_ms;
        double total_time = players.empty() ? 1.0 : players[0]->end_time;
        for (double marker : markers) {
            float mx = (float)((marker - first_bar_time) / total_time) * bar_width;
            tex.draw_texture(PRACTICE::GOGO_MARKER, {.x = mx});
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

        float dw   = tex.textures[PRACTICE::BAR_COUNT]->x2[0];
        float divw = tex.textures[PRACTICE::BAR_DIVIDER]->x2[0];
        float dh   = tex.textures[PRACTICE::BAR_COUNT]->y2[0];
        float pb_y = tex.textures[PRACTICE::PROGRESS_BAR_BG]->y[0];
        float pb_h = tex.textures[PRACTICE::PROGRESS_BAR_BG]->y2[0];
        float digit_y = pb_y + (pb_h - dh) * 0.5f;

        std::string cur_str = std::to_string(current_bar);
        std::string tot_str = std::to_string(total_bars);
        int d1 = song_speed / 10;
        int d2 = song_speed % 10;

        tex.draw_texture(PRACTICE::SONG_TEMPO, {});
        tex.draw_texture(PRACTICE::DOT, {});
        tex.draw_texture(PRACTICE::MULTIPLIER, {});
        tex.draw_texture(PRACTICE::BAR_LABEL, {});

        float st_right  = tex.textures[PRACTICE::SONG_TEMPO]->x[0] + tex.textures[PRACTICE::SONG_TEMPO]->x2[0];
        float dot_right = tex.textures[PRACTICE::DOT]->x[0] + tex.textures[PRACTICE::DOT]->x2[0];
        float lbl_right = tex.textures[PRACTICE::BAR_LABEL]->x[0] + tex.textures[PRACTICE::BAR_LABEL]->x2[0];

        tex.draw_texture(PRACTICE::BAR_COUNT, {.frame = d1, .x = st_right - 4 * tex.screen_scale, .y = digit_y});
        tex.draw_texture(PRACTICE::BAR_COUNT, {.frame = d2, .x = dot_right - 4 * tex.screen_scale, .y = digit_y});

        for (int i = 0; i < (int)cur_str.size(); i++)
            tex.draw_texture(PRACTICE::BAR_COUNT, {.frame = cur_str[i] - '0', .x = lbl_right + i * dw, .y = digit_y});
        float div_x = lbl_right + (float)cur_str.size() * dw;
        tex.draw_texture(PRACTICE::BAR_DIVIDER, {.x = div_x, .y = digit_y});
        for (int i = 0; i < (int)tot_str.size(); i++)
            tex.draw_texture(PRACTICE::BAR_COUNT, {.frame = tot_str[i] - '0', .x = div_x + divw + i * dw, .y = digit_y});
    }

    if (paused) {
        tex.draw_texture(PRACTICE::PAUSED, {.fade = 0.5});
    }

    draw_overlay();
}
