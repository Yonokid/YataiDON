#include "dan_select.h"
#include "../libs/input.h"
#include "../libs/parsers/song_parser.h"
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <fstream>
#include <spdlog/spdlog.h>

// ─── DanNavigator ────────────────────────────────────────────────────────────

int DanNavigator::total_notes_for(const std::vector<DanSongEntry>& songs) {
    int total = 0;
    for (const auto& entry : songs) {
        try {
            SongParser sp(entry.song_path);
            auto [notes, bm, be, bn] = sp.notes_to_position(entry.difficulty);
            for (const Note& n : notes.notes)
                if (n.type >= NoteType::DON && n.type <= NoteType::KAT_L) total++;
            for (auto& sec : bm)
                for (const Note& n : sec.notes)
                    if (n.type >= NoteType::DON && n.type <= NoteType::KAT_L) total++;
        } catch (...) {}
    }
    return total;
}

void DanNavigator::init(const std::vector<fs::path>& song_paths) {
    boxes.clear();
    selected_index = 0;

    for (const fs::path& root : song_paths) {
        if (!fs::exists(root)) continue;
        std::error_code ec;
        auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
        while (it != fs::end(it)) {
            if (!ec && it->path().filename() == "dan.json") {
                fs::path json_path = it->path();
                try {
                    std::ifstream ifs(json_path);
                    if (!ifs.is_open()) { it.increment(ec); continue; }
                    rapidjson::IStreamWrapper isw(ifs);
                    rapidjson::Document doc;
                    doc.ParseStream(isw);
                    if (doc.HasParseError()) { it.increment(ec); continue; }

                    std::string title = doc["title"].GetString();
                    int color = doc["color"].GetInt();

                    std::vector<DanSongEntry> songs;
                    if (doc.HasMember("charts")) {
                        for (auto& chart : doc["charts"].GetArray()) {
                            std::string chart_title    = chart["title"].GetString();
                            std::string chart_subtitle = chart.HasMember("subtitle") ? chart["subtitle"].GetString() : "";
                            int diff = chart["difficulty"].GetInt();

                            // Strip '--' prefix from subtitle (PyTaiko convention)
                            if (chart_subtitle.substr(0, 2) == "--")
                                chart_subtitle = chart_subtitle.substr(2);

                            auto path_opt = navigator.find_song_by_title(chart_title, chart_subtitle);
                            if (!path_opt.has_value()) {
                                spdlog::warn("DanNavigator: song '{}' not found", chart_title);
                                continue;
                            }

                            try {
                                SongParser sp(*path_opt);
                                int level = sp.metadata.course_data.count(diff) ? sp.metadata.course_data.at(diff).level : 10;
                                int genre = (int)GenreIndex::NAMCO;
                                fs::path box_def_dir = path_opt->parent_path().parent_path();
                                if (fs::exists(box_def_dir / "box.def")) {
                                    BoxDef bd = navigator.parse_box_def(box_def_dir);
                                    genre = (int)bd.genre_index;
                                }
                                songs.push_back({*path_opt, genre, diff, level});
                            } catch (...) {
                                spdlog::warn("DanNavigator: failed to parse '{}'", path_opt->string());
                            }
                        }
                    }

                    if (songs.empty()) { it.increment(ec); continue; }

                    std::vector<Exam> exams;
                    if (doc.HasMember("exams")) {
                        for (auto& e : doc["exams"].GetArray()) {
                            Exam exam;
                            exam.type  = e["type"].GetString();
                            exam.range = e["range"].GetString();
                            if (e.HasMember("value") && e["value"].IsArray() && e["value"].Size() >= 2) {
                                exam.red  = e["value"][0].GetInt();
                                exam.gold = e["value"][1].GetInt();
                            }
                            exams.push_back(exam);
                        }
                    }

                    int tn = total_notes_for(songs);
                    boxes.push_back(std::make_unique<DanBox>(json_path, title, color, songs, exams, tn));
                    spdlog::debug("DanNavigator: loaded '{}'", title);
                } catch (const std::exception& ex) {
                    spdlog::warn("DanNavigator: error loading {}: {}", json_path.string(), ex.what());
                }
            }
            it.increment(ec);
        }
    }

    if (boxes.empty()) {
        spdlog::warn("DanNavigator: no dan courses found");
        return;
    }

    set_positions(true, 0);
    boxes[selected_index]->expand_box();
    for (auto& b : boxes) {
        b->fade_in(100);
    }
}

void DanNavigator::set_positions(bool init, float duration) {
    int n = (int)boxes.size();
    if (n == 0) return;
    for (int i = 0; i < n; i++) {
        float offset = i - selected_index;
        if (offset > n / 2.0f)  offset -= n;
        else if (offset < -n / 2.0f) offset += n;

        float base    = BOX_CENTER * tex.screen_scale;
        float spacing = BASE_SPACING * tex.screen_scale;
        float side_l  = SIDE_OFFSET_L * tex.screen_scale;
        float side_r  = SIDE_OFFSET_R * tex.screen_scale;

        float pos = (base - 150 * tex.screen_scale) + offset * spacing;
        if (std::abs(pos - (base - 150 * tex.screen_scale)) < 1.0f)
            pos = base;
        else if (pos > base - 150 * tex.screen_scale)
            pos += side_r;
        else
            pos -= side_l;

        if (init || std::abs(pos - boxes[i]->position) >= tex.screen_width)
            boxes[i]->set_position(pos);
        else
            boxes[i]->move_box(pos, duration);
    }
}

void DanNavigator::move_left() {
    if (boxes.empty()) return;
    boxes[selected_index]->close_box();
    selected_index = (selected_index - 1 + (int)boxes.size()) % (int)boxes.size();
    set_positions(false, 166);
    boxes[selected_index]->expand_box();
}

void DanNavigator::move_right() {
    if (boxes.empty()) return;
    boxes[selected_index]->close_box();
    selected_index = (selected_index + 1) % (int)boxes.size();
    set_positions(false, 166);
    boxes[selected_index]->expand_box();
}

void DanNavigator::skip(int delta) {
    if (boxes.empty()) return;
    boxes[selected_index]->close_box();
    selected_index = ((selected_index + delta) % (int)boxes.size() + (int)boxes.size()) % (int)boxes.size();
    set_positions(true, 0);
    boxes[selected_index]->expand_box();
}

DanBox* DanNavigator::get_current() {
    if (boxes.empty()) return nullptr;
    return boxes[selected_index].get();
}

void DanNavigator::update(double current_ms) {
    for (auto& b : boxes) {
        bool on_screen = b->position > -156 * tex.screen_scale && b->position < tex.screen_width + 144 * tex.screen_scale;
        if (on_screen && !b->text_loaded)
            b->load_text();
        b->update(current_ms);
    }
}

void DanNavigator::draw() {
    for (auto& b : boxes) {
        float pos = b->position;
        if (pos >= -156 * tex.screen_scale && pos <= tex.screen_width + 144 * tex.screen_scale) {
            b->draw(false);
        }
    }
}

// ─── DanSelectScreen ─────────────────────────────────────────────────────────

void DanSelectScreen::on_screen_start() {
    Screen::on_screen_start();
    audio->play_sound("bgm", "music");
    audio->play_sound("dan_select", "voice");

    indicator    = std::make_unique<Indicator>(Indicator::State::SELECT);
    confirm_fade = (FadeAnimation*)tex.get_animation(8);
    state        = SongSelectState::BROWSING;
    is_confirmed = false;
    last_moved   = 0;

    dan_navigator.init(global_data.config->paths.tja_path);
}

Screens DanSelectScreen::on_screen_end(Screens next_screen) {
    DanBox* current = dan_navigator.get_current();
    if (current && next_screen == Screens::GAME_DAN) {
        SessionData& sd = global_data.session_data[(int)global_data.player_num];
        sd.selected_dan      = current->songs;
        sd.selected_dan_exam = current->exams;
        sd.song_title        = current->dan_title;
        sd.dan_color         = current->dan_color;
        if (!current->songs.empty())
            sd.selected_song = current->songs[0].song_path;
    }
    return Screen::on_screen_end(next_screen);
}

void DanSelectScreen::handle_input_browsing(double current_ms) {
    if (dan_navigator.boxes.empty()) return;

    bool skip_left  = check_key_pressed(ray::KEY_LEFT_CONTROL);
    bool skip_right = check_key_pressed(ray::KEY_RIGHT_CONTROL);
    bool nav_left   = is_l_kat_pressed(global_data.player_num);
    bool nav_right  = is_r_kat_pressed(global_data.player_num);
    bool confirm    = is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num);

    if (skip_left || (nav_left && current_ms <= last_moved + 50)) {
        audio->play_sound("skip", "sound");
        dan_navigator.skip(-10);
        last_moved = current_ms;
    } else if (skip_right || (nav_right && current_ms <= last_moved + 50)) {
        audio->play_sound("skip", "sound");
        dan_navigator.skip(10);
        last_moved = current_ms;
    } else if (nav_left) {
        audio->play_sound("kat", "sound");
        dan_navigator.move_left();
        last_moved = current_ms;
    } else if (nav_right) {
        audio->play_sound("kat", "sound");
        dan_navigator.move_right();
        last_moved = current_ms;
    } else if (confirm) {
        audio->play_sound("don", "sound");
        audio->play_sound("confirm_box", "sound");
        audio->play_sound("dan_confirm", "voice");
        confirm_fade->start();
        state = SongSelectState::SONG_SELECTED;
        is_confirmed = false;
    }
}

void DanSelectScreen::handle_input_selected() {
    if (is_l_kat_pressed(global_data.player_num)) { audio->play_sound("kat", "sound"); is_confirmed = false; }
    if (is_r_kat_pressed(global_data.player_num)) { audio->play_sound("kat", "sound"); is_confirmed = true;  }

    // Only consume the don press when canceling; leave it in the buffer for update() to handle confirm
    if (!is_confirmed && (is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num)))
        state = SongSelectState::BROWSING;
}

std::optional<Screens> DanSelectScreen::update() {
    Screen::update();
    double current_ms = get_current_ms();
    dan_navigator.update(current_ms);
    indicator->update(current_ms);
    confirm_fade->update(current_ms);

    if (state == SongSelectState::BROWSING) {
        handle_input_browsing(current_ms);
        if (is_r_don_pressed(global_data.player_num) || is_l_don_pressed(global_data.player_num)) {
            // handled in browsing
        }
    } else if (state == SongSelectState::SONG_SELECTED) {
        handle_input_selected();
        if (is_confirmed && (is_l_don_pressed(global_data.player_num) || is_r_don_pressed(global_data.player_num))) {
            audio->play_sound("don", "sound");
            return on_screen_end(Screens::GAME_DAN);
        }
    }

    // Back: left kat in browsing with no songs or specific back logic
    if (check_key_pressed(global_data.config->keys.back_key) && state == SongSelectState::BROWSING) {
        return on_screen_end(Screens::SONG_SELECT);
    }
    return std::nullopt;
}

void DanSelectScreen::draw_confirm_overlay() {
    float f = confirm_fade->attribute;
    if (f <= 0) return;
    ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height,
                       ray::Fade(ray::BLACK, std::min(0.5f, (float)f)));
    tex.draw_texture(CONFIRM_BOX::BG,               {.fade=f});
    tex.draw_texture(CONFIRM_BOX::CONFIRMATION_TEXT,{.fade=f});
    for (int i = 0; i < 2; i++)
        tex.draw_texture(CONFIRM_BOX::SELECTION_BOX,{.fade=f, .index=i});
    int side = is_confirmed ? 1 : 0;
    tex.draw_texture(CONFIRM_BOX::SELECTION_BOX_HIGHLIGHT,{.fade=f, .index=side});
    tex.draw_texture(CONFIRM_BOX::SELECTION_BOX_OUTLINE,  {.fade=f, .index=side});
    tex.draw_texture(CONFIRM_BOX::YES, {.fade=f});
    tex.draw_texture(CONFIRM_BOX::NO,  {.fade=f});
}

void DanSelectScreen::draw() {
    tex.draw_texture(GLOBAL::BG,        {});
    tex.draw_texture(GLOBAL::BG_HEADER, {});
    tex.draw_texture(GLOBAL::BG_FOOTER, {});
    tex.draw_texture(GLOBAL::FOOTER,    {});

    dan_navigator.draw();

    if (state == SongSelectState::SONG_SELECTED)
        draw_confirm_overlay();

    indicator->draw(tex.skin_config[SC::DAN_SELECT_INDICATOR].x, tex.skin_config[SC::DAN_SELECT_INDICATOR].y);
    coin_overlay.draw();
    tex.draw_texture(GLOBAL::DAN_SELECT, {});
    allnet_indicator.draw();
}
