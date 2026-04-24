#include "game_practice.h"
#include "raylib.h"

void PracticeGameScreen::on_screen_start() {
    GameScreen::on_screen_start();
    // Override background with practice preset
    if (!movie.has_value()) {
        background.emplace(global_data.player_num, bpm, "PRACTICE");
    }
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
            global_data.modifiers[(int)global_data.player_num]);
        practice_player = static_cast<PracticePlayer*>(players.back().get());
    }
}

void PracticeGameScreen::init_tja_practice(const fs::path& song) {
    // Extract bars and note list for scrobbling from the already-parsed TJA
    int difficulty = global_data.session_data[(int)global_data.player_num].selected_difficulty;
    auto [notes, bm, be, bn] = parser->notes_to_position(difficulty);
    apply_modifiers(notes, global_data.modifiers[(int)global_data.player_num]);

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
            audio->stop_sound(song_music.value());
        }
        pause_time = (int)(get_current_ms() - start_ms);

        if (bars.empty()) return;
        double first_bar_time = bars[0].hit_ms;
        int nearest_bar_index = 0;
        double min_distance = std::numeric_limits<double>::infinity();
        for (int i = 0; i < (int)bars.size(); ++i) {
            double bar_relative_time = bars[i].hit_ms - first_bar_time;
            double distance = std::abs(bar_relative_time - current_ms);
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
            audio->play_sound(song_music.value(), "music");
            double seek_sec = (start_time - start_delay) / 1000.0 - parser->metadata.offset;
            audio->seek_sound(song_music.value(), std::max(0.0, seek_sec));
        }
        song_started = true;
        start_ms = get_current_ms() - pause_time;
    }
}

std::optional<Screens> PracticeGameScreen::global_keys_practice() {
    if (ray::IsKeyPressed(global_data.config->keys.restart_key)) {
        if (song_music.has_value()) {
            audio->stop_sound(song_music.value());
        }
        players.clear();
        init_tja(global_data.session_data[(int)global_data.player_num].selected_song);
        init_tja_practice(global_data.session_data[(int)global_data.player_num].selected_song);
        audio->play_sound("restart", "sound");
        song_started = false;
        paused = false;
        return std::nullopt;
    }

    if (ray::IsKeyPressed(global_data.config->keys.back_key)) {
        if (song_music.has_value()) {
            audio->stop_sound(song_music.value());
        }
        return on_screen_end(Screens::PRACTICE_SELECT);
    }

    if (ray::IsKeyPressed(global_data.config->keys.pause_key)) {
        pause_song_practice();
    }

    if (!bars.empty() && (ray::IsKeyPressed(ray::KEY_LEFT) || ray::IsKeyPressed(ray::KEY_RIGHT))) {
        audio->play_sound("kat", "sound");

        // Snap any in-progress animation
        if (scrobble_move->is_started && !scrobble_move->is_finished) {
            scrobble_time = bars[scrobble_index].hit_ms;
        }

        int old_index = scrobble_index;
        if (ray::IsKeyPressed(ray::KEY_LEFT)) {
            scrobble_index = (scrobble_index > 0) ? scrobble_index - 1 : (int)bars.size() - 1;
        } else {
            scrobble_index = (scrobble_index + 1) % (int)bars.size();
        }

        double time_difference = bars[scrobble_index].hit_ms - bars[old_index].hit_ms;
        scrobble_move = std::make_unique<MoveAnimation>(400.0, (int)time_difference, false, false, 0, 0.0,
                                                        std::nullopt, std::nullopt, std::string("quadratic"));
        scrobble_move->start();
    }

    return std::nullopt;
}

std::optional<Screens> PracticeGameScreen::update() {
    Screen::update();

    double current_time = get_current_ms();
    transition->update(current_time);
    if (!paused) {
        current_ms = current_time - start_ms;
    }
    if (transition->is_finished()) {
        start_song(current_ms);
        global_data.input_locked = 0;
    } else {
        start_ms = current_time - parser->metadata.offset * 1000;
    }

    if (song_started && song_music.has_value() && audio->is_sound_playing(song_music.value())) {
        double audio_ms = audio->get_sound_time_played(song_music.value()) * 1000.0;
        double audio_ms_adjusted = audio_ms + (parser->metadata.offset * 1000 + start_delay - (double)global_data.config->general.audio_offset);
        if (std::abs(current_ms - audio_ms_adjusted) > Timing::GOOD) {
            current_ms = audio_ms_adjusted;
            start_ms = current_time - current_ms;
        }
    }

    update_background(current_time);

    for (auto& player : players) {
        player->update(current_ms, current_time, background);
    }
    song_info.update(current_time);

    scrobble_move->update(current_time);
    if (scrobble_move->is_started && scrobble_move->is_finished) {
        scrobble_time = bars[scrobble_index].hit_ms;
        scrobble_move = std::make_unique<MoveAnimation>(200.0, 0);
    }

    return global_keys_practice();
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
                         ? tex_id_map.at("notes/" + std::to_string((int)head.type)) : TexID(0);
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
                         ? tex_id_map.at("notes/" + std::to_string((int)head.type)) : TexID(0);
        tex.draw_texture(head_tex, {.x = position - offset - tex.textures[NOTES::_9]->width / 2.0f, .y = y_pos});
        tex.draw_texture(NOTES::_10, {.x = position - offset + tex.textures[NOTES::_10]->width - tex.textures[NOTES::_9]->width / 2.0f, .y = y_pos});
    }
    float moji_y = tex.skin_config[SC::MOJI].y + (184 * tex.screen_scale);
    tex.draw_texture(NOTES::MOJI, {.frame = head.moji, .x = position - tex.textures[NOTES::MOJI]->width / 2.0f, .y = moji_y});
}

void PracticeGameScreen::draw_notes_scrobble(double current_ms) const {
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
                                 ? tex_id_map.at("notes/" + std::to_string((int)note.type)) : TexID(0);
                tex.draw_texture(note_tex, {.center = true, .x = x - tex.textures[NOTES::_9]->width / 2.0f, .y = tex.skin_config[SC::NOTES].y + y});
            }
            float moji_x = get_scrobble_position_x(note, current_ms) - tex.textures[NOTES::MOJI]->width / 2.0f;
            tex.draw_texture(NOTES::MOJI, {.frame = note.moji, .x = moji_x, .y = tex.skin_config[SC::MOJI].y + y});
        }
    }
}

void PracticeGameScreen::draw() {
    ray::ClearBackground(ray::BLACK);
    if (movie.has_value()) {
        movie->draw();
    } else if (background.has_value()) {
        background->draw_back();
    }

    players[0]->draw_practice(current_ms, 0, 184 * tex.screen_scale, mask_shader, !paused);

    if (background.has_value()) background->draw_fore();

    // Scrobble note overlay when paused
    if (paused) {
        draw_notes_scrobble(scrobble_time);
    }

    // Practice large drum graphics
    tex.draw_texture(PRACTICE::LARGE_DRUM, {.index = 0});
    tex.draw_texture(PRACTICE::LARGE_DRUM, {.index = 1});

    // Player overlays after practice graphics (hit effects, combos, etc.)
    if (players.size() == 1) {
        players[0]->draw_overlays(184 * tex.screen_scale, mask_shader);
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
        progress = players.empty() ? 0.0f : (float)std::min(current_ms / players[0]->end_time, 1.0);
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

    if (paused) {
        tex.draw_texture(PRACTICE::PAUSED, {.fade = 0.5});
    }

    draw_overlay();
}
