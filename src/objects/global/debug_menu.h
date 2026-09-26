#pragma once

#include "../../libs/texture.h"
#include "../../libs/screen.h"
#include "../../libs/script.h"
#include "../../libs/filesystem.h"
#include "../../libs/animation.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>

#ifdef DrawTextEx
    #undef DrawTextEx
#endif

class DebugMenu {
public:
    static const int TAB_COUNT = 4;
    static constexpr float PANEL_WIDTH      = 360.0f;
    static constexpr float TAB_HEIGHT       = 36.0f;
    static constexpr float ROW_HEIGHT       = 20.0f;
    static constexpr float SCROLLBAR_WIDTH  = 6.0f;
    static constexpr float EDIT_ROW_HEIGHT   = 24.0f;
    static constexpr float VERDICT_TOP       = 44.0f;
    static constexpr float VERDICT_HEIGHT    = 34.0f;
    static constexpr float EDIT_FIELDS_TOP   = VERDICT_TOP + VERDICT_HEIGHT + 4.0f;
    static constexpr float EDIT_PANEL_HEIGHT = EDIT_FIELDS_TOP + 4 * EDIT_ROW_HEIGHT + 6.0f;
    static constexpr float FRAME_CELL_WIDTH      = 56.0f;
    static constexpr float FRAME_THUMB_SIZE      = 40.0f;
    static constexpr float FRAME_BTN_ROW_HEIGHT  = 20.0f;
    static constexpr float FRAMES_HEADER_HEIGHT  = 20.0f;
    static constexpr float FRAME_ROW_HEIGHT      = FRAME_THUMB_SIZE + 4.0f + FRAME_BTN_ROW_HEIGHT;
    static constexpr float SCENE_LIST_HEIGHT     = 120.0f;
    static constexpr float SWITCH_BTN_HEIGHT     = 28.0f;
    static constexpr int   LUA_TEXT_SIZE         = 12;
    static constexpr float LUA_LINE_HEIGHT       = 15.0f;
    static constexpr float LUA_BTN_HEIGHT        = 22.0f;
    static constexpr float LUA_BTN_GAP           = 6.0f;
    static constexpr int   CALL_TEXT_MAX_ROWS    = 6;
    static constexpr size_t SOURCE_MAX_FILES     = 8;
    static constexpr std::uintmax_t SOURCE_MAX_BYTES = 1 << 20;
    static constexpr double SOURCE_RESTAT_SECONDS = 0.5;

    bool open = false;
    static constexpr double SLIDE_DURATION_MS = 180.0;
    std::unique_ptr<MoveAnimation> slide_anim;
    int active_tab = 0;

    float slide_offset() const {
        if (slide_anim) return (float)slide_anim->attribute;
        return open ? 0.0f : PANEL_WIDTH;
    }

    void toggle_open() {
        const float current = slide_offset();
        open = !open;
        const float target = open ? 0.0f : PANEL_WIDTH;
        slide_anim = std::make_unique<MoveAnimation>(
            SLIDE_DURATION_MS, (int)(target - current), false, false, (int)current,
            0.0, std::nullopt, std::nullopt, EaseType::Quadratic);
        slide_anim->start();
        if (open) ray::ShowCursor(); else ray::HideCursor();
    }

    bool is_visible() const {
        return open || (slide_anim && !slide_anim->is_finished);
    }
    int hovered_log_index = -1;
    int scroll_offset = 0;

    bool has_selection = false;
    std::string selected_name;
    int selected_tex_index = 0;
    int selected_log_index = -1;

    int editing_field = -1;
    std::string edit_buffer;

    std::optional<Screens> requested_screen;

    static constexpr Screens ALL_SCREENS[] = {
        Screens::TITLE, Screens::ENTRY, Screens::SONG_SELECT, Screens::GAME, Screens::GAME_2P,
        Screens::RESULT, Screens::RESULT_2P, Screens::SONG_SELECT_2P, Screens::DAN_SELECT,
        Screens::GAME_DAN, Screens::DAN_RESULT, Screens::PRACTICE_SELECT, Screens::GAME_PRACTICE,
        Screens::SETTINGS, Screens::LOADING, Screens::INPUT_CALI, Screens::GAME_OVER, Screens::INPUT_TEST
    };

    void clear_selection() {
        commit_edit();
        has_selection = false;
        selected_name.clear();
        selected_log_index = -1;
        source_cache.clear();
    }

    ray::Font ui_font{};
    ray::Font code_font{};
    bool fonts_loaded = false;

    void load_fonts() {
        ui_font = ray::LoadFontEx(resolve_skin_path("Fonts/debug_ui.ttf").string().c_str(), 32, nullptr, 0);
        code_font = ray::LoadFontEx(resolve_skin_path("Fonts/debug_code.ttf").string().c_str(), 32, nullptr, 0);
        ray::SetTextureFilter(ui_font.texture, ray::TEXTURE_FILTER_BILINEAR);
        ray::SetTextureFilter(code_font.texture, ray::TEXTURE_FILTER_BILINEAR);
        fonts_loaded = true;
    }

    void unload_fonts() {
        if (!fonts_loaded) return;
        ray::UnloadFont(ui_font);
        ray::UnloadFont(code_font);
        fonts_loaded = false;
    }

    void draw_text(const char* text, int x, int y, int font_size, ray::Color color) const {
        ray::DrawTextEx(ui_font, text, {(float)x, (float)y}, (float)font_size, font_size / 10.0f, color);
    }

    int measure_text(const char* text, int font_size) const {
        return (int)ray::MeasureTextEx(ui_font, text, (float)font_size, font_size / 10.0f).x;
    }

    void draw_code_text(const char* text, int x, int y, int font_size, ray::Color color) const {
        ray::DrawTextEx(code_font, text, {(float)x, (float)y}, (float)font_size, font_size / 10.0f, color);
    }

    int measure_code_text(const char* text, int font_size) const {
        return (int)ray::MeasureTextEx(code_font, text, (float)font_size, font_size / 10.0f).x;
    }

    FramedTexture* get_selected_framed() const {
        return dynamic_cast<FramedTexture*>(selected_obj());
    }

    static int frame_grid_cols() { return std::max(1, (int)(PANEL_WIDTH / FRAME_CELL_WIDTH)); }

    static float frames_section_height(int frame_count) {
        int rows = (frame_count + frame_grid_cols() - 1) / frame_grid_cols();
        return FRAMES_HEADER_HEIGHT + rows * FRAME_ROW_HEIGHT;
    }

    float edit_panel_height() const {
        FramedTexture* framed = get_selected_framed();
        if (framed && framed->frame_count() > 1) return EDIT_PANEL_HEIGHT + frames_section_height(framed->frame_count());
        return EDIT_PANEL_HEIGHT;
    }

    void commit_edit() {
        if (editing_field < 0) return;
        int* value = field_ptr(editing_field);
        if (value && !edit_buffer.empty() && edit_buffer != "-") {
            try { *value = std::stoi(edit_buffer); } catch (...) {}
        }
        editing_field = -1;
        edit_buffer.clear();
    }

    // Writes edit_data_buffer into whichever global_data/session_data field
    // the Scenes tab's data editor is currently focused on.
    void commit_data_edit() {
        if (!editing_data_ptr) return;
        if (editing_data_kind == DataField::Kind::STRING) {
            *static_cast<std::string*>(editing_data_ptr) = edit_data_buffer;
        } else {
            try { *static_cast<int*>(editing_data_ptr) = std::stoi(edit_data_buffer); } catch (...) {}
        }
        editing_data_ptr = nullptr;
        edit_data_buffer.clear();
    }

    void update(const ray::Camera2D& camera) {
        if (ray::IsKeyPressed(ray::KEY_F7)) toggle_open();
        if (slide_anim) slide_anim->update(get_frame_ms());

        debug_draw_log_prev.swap(debug_draw_log);
        debug_draw_log.clear();

        const bool textures_tab_active = open && active_tab == 0;
        debug_log_draws = open && (active_tab == 0 || active_tab == 2);
        if (!open) { commit_edit(); commit_data_edit(); return; }

        const float panel_x   = tex.screen_width - PANEL_WIDTH + slide_offset();
        const float tab_width = PANEL_WIDTH / TAB_COUNT;
        const ray::Vector2 mouse = ray::GetScreenToWorld2D(ray::GetMousePosition(), camera);
        const bool clicked = ray::IsMouseButtonPressed(ray::MOUSE_BUTTON_LEFT);
        const bool mouse_over_panel = mouse.x >= panel_x;

        if (clicked) { commit_edit(); commit_data_edit(); }

        if (editing_field < 0 && ray::IsKeyPressed(ray::KEY_TAB)) active_tab = (active_tab + 1) % TAB_COUNT;

        if (clicked && mouse_over_panel && mouse.y < TAB_HEIGHT) {
            int hit = (int)((mouse.x - panel_x) / tab_width);
            if (hit >= 0 && hit < TAB_COUNT) active_tab = hit;
        }

        if (active_tab == 1) {
            const float list_top     = TAB_HEIGHT;
            const float list_bottom  = list_top + SCENE_LIST_HEIGHT;
            const int   scene_count  = (int)std::size(ALL_SCREENS);
            const int   scene_shown  = std::max(0, (int)(SCENE_LIST_HEIGHT / ROW_HEIGHT));
            const int   scene_max_scroll = std::max(0, scene_count - scene_shown);
            const bool  mouse_in_scenes  = mouse_over_panel && mouse.y >= list_top && mouse.y < list_bottom;

            if (mouse_in_scenes) {
                float wheel = ray::GetMouseWheelMove();
                if (wheel != 0.0f) scene_scroll -= (int)wheel;
            }
            scene_scroll = std::clamp(scene_scroll, 0, scene_max_scroll);

            if (clicked && mouse_in_scenes) {
                int row = (int)((mouse.y - list_top) / ROW_HEIGHT) + scene_scroll;
                if (row >= 0 && row < scene_count) selected_screen = ALL_SCREENS[row];
            }

            const float button_top    = list_bottom;
            const float button_bottom = button_top + SWITCH_BTN_HEIGHT;
            if (clicked && selected_screen.has_value() && mouse_over_panel &&
                mouse.y >= button_top && mouse.y < button_bottom) {
                requested_screen = selected_screen;
            }

            const float data_top    = button_bottom;
            const float data_bottom = (float)tex.screen_height;
            std::vector<DataField> fields = build_data_fields();
            const int   field_count  = (int)fields.size();
            const int   data_shown   = std::max(0, (int)((data_bottom - data_top) / ROW_HEIGHT));
            const int   data_max_scroll = std::max(0, field_count - data_shown);
            const bool  mouse_in_data   = mouse_over_panel && mouse.y >= data_top && mouse.y < data_bottom;

            if (mouse_in_data) {
                float wheel = ray::GetMouseWheelMove();
                if (wheel != 0.0f) data_scroll -= (int)wheel;
            }
            data_scroll = std::clamp(data_scroll, 0, data_max_scroll);

            if (clicked && mouse_in_data) {
                int row = (int)((mouse.y - data_top) / ROW_HEIGHT) + data_scroll;
                if (row >= 0 && row < field_count) {
                    const DataField& f = fields[row];
                    float row_y = data_top + (row - data_scroll) * ROW_HEIGHT;
                    DataFieldRects r = data_field_rects(f, panel_x, row_y);
                    const int step = ray::IsKeyDown(ray::KEY_LEFT_SHIFT) ? 10 : 1;
                    if (f.kind == DataField::Kind::BOOL) {
                        if (in_rect(mouse, r.value)) *f.b = !*f.b;
                    } else if (f.kind == DataField::Kind::STRING) {
                        if (in_rect(mouse, r.value)) {
                            editing_data_ptr = f.s;
                            editing_data_kind = f.kind;
                            edit_data_buffer = *f.s;
                        }
                    } else if (f.kind == DataField::Kind::PLAYER_NUM) {
                        if (in_rect(mouse, r.minus)) *f.pn = (PlayerNum)(((int)*f.pn + 5) % 6);
                        else if (in_rect(mouse, r.plus)) *f.pn = (PlayerNum)(((int)*f.pn + 1) % 6);
                    } else { // INT
                        if (in_rect(mouse, r.minus)) *f.i -= step;
                        else if (in_rect(mouse, r.plus)) *f.i += step;
                        else if (in_rect(mouse, r.value)) {
                            editing_data_ptr = f.i;
                            editing_data_kind = f.kind;
                            edit_data_buffer = std::to_string(*f.i);
                        }
                    }
                }
            }

            if (editing_data_ptr) {
                if (ray::IsKeyPressed(ray::KEY_ESCAPE)) {
                    editing_data_ptr = nullptr;
                    edit_data_buffer.clear();
                } else if (ray::IsKeyPressed(ray::KEY_ENTER)) {
                    commit_data_edit();
                } else {
                    if (ray::IsKeyPressed(ray::KEY_BACKSPACE) && !edit_data_buffer.empty()) edit_data_buffer.pop_back();
                    int ch;
                    while ((ch = ray::GetCharPressed()) > 0) {
                        bool ok = (editing_data_kind == DataField::Kind::STRING)
                            ? (ch >= 32 && ch < 127)
                            : ((ch >= '0' && ch <= '9') || (ch == '-' && edit_data_buffer.empty()));
                        if (ok && edit_data_buffer.size() < 64) edit_data_buffer += (char)ch;
                    }
                }
            }
            return;
        }

        if (active_tab == 2) {
            const DrawLogEntry* entry = clicked && mouse_over_panel ? find_selected_entry() : nullptr;
            if (entry && entry->from_lua && !entry->lua_source.empty()) {
                LuaButtons buttons = lua_buttons(panel_x, (float)tex.screen_height);
                std::string path = resolve_lua_path(entry->lua_source).string();
                if (path.empty()) return;
                if (in_rect(mouse, buttons.copy_path)) ray::SetClipboardText(path.c_str());
                else if (in_rect(mouse, buttons.copy_path_line))
                    ray::SetClipboardText((path + ":" + std::to_string(entry->lua_line)).c_str());
            }
            return;
        }

        if (!textures_tab_active) return;

        rebuild_visible_rows();

        const float list_top    = TAB_HEIGHT;
        const float list_bottom = tex.screen_height - edit_panel_height();
        const int   row_count   = (int)visible_rows.size();
        const int   rows_shown  = std::max(0, (int)((list_bottom - list_top) / ROW_HEIGHT));
        const int   max_scroll  = std::max(0, row_count - rows_shown);
        const bool  mouse_in_list = mouse_over_panel && mouse.y >= list_top && mouse.y < list_bottom;

        if (mouse_in_list) {
            float wheel = ray::GetMouseWheelMove();
            if (wheel != 0.0f) scroll_offset -= (int)(wheel * 3.0f);
        }
        scroll_offset = std::clamp(scroll_offset, 0, max_scroll);

        hovered_log_index = -1;
        if (!mouse_over_panel) {
            for (int i = (int)debug_draw_log_prev.size() - 1; i >= 0; i--) {
                const ray::Rectangle& r = debug_draw_log_prev[i].rect;
                if (mouse.x >= r.x && mouse.x <= r.x + r.width &&
                    mouse.y >= r.y && mouse.y <= r.y + r.height) {
                    hovered_log_index = i;
                    break;
                }
            }
        }

        if (clicked && mouse_in_list) {
            int row = (int)((mouse.y - list_top) / ROW_HEIGHT) + scroll_offset;
            if (row >= 0 && row < row_count) {
                const VisualRow& vr = visible_rows[row];
                if (vr.is_header) {
                    if (expanded_subsets.count(vr.subset)) expanded_subsets.erase(vr.subset);
                    else expanded_subsets.insert(vr.subset);
                } else {
                    const DrawLogEntry& e = debug_draw_log_prev[vr.log_index];
                    has_selection = true;
                    selected_name = e.name;
                    selected_tex_index = e.index;
                    selected_log_index = vr.log_index;
                }
            }
        }

        if (clicked && has_selection && mouse_over_panel && mouse.y >= list_bottom) {
            const int step = ray::IsKeyDown(ray::KEY_LEFT_SHIFT) ? 10 : 1;
            for (int i = 0; i < 4; i++) {
                int* value = field_ptr(i);
                if (!value) continue;
                FieldButtons b = field_buttons(i, panel_x, list_bottom);
                if (in_rect(mouse, b.minus)) *value -= step;
                else if (in_rect(mouse, b.plus)) *value += step;
                else if (in_rect(mouse, b.value)) {
                    editing_field = i;
                    edit_buffer = std::to_string(*value);
                }
            }
        }

        if (FramedTexture* framed = get_selected_framed()) {
            int frame_count = framed->frame_count();
            if (clicked && frame_count > 1 && mouse_over_panel && mouse.y >= list_bottom + EDIT_PANEL_HEIGHT) {
                const float frames_top = list_bottom + EDIT_PANEL_HEIGHT;
                for (int idx = 0; idx < frame_count; idx++) {
                    FrameCellButtons b = frame_cell_buttons(idx, panel_x, frames_top);
                    if (in_rect(mouse, b.left) && idx > 0) {
                        std::swap(framed->textures[idx], framed->textures[idx - 1]);
                        break;
                    } else if (in_rect(mouse, b.right) && idx < frame_count - 1) {
                        std::swap(framed->textures[idx], framed->textures[idx + 1]);
                        break;
                    }
                }
            }
        }

        if (editing_field >= 0) {
            if (ray::IsKeyPressed(ray::KEY_ESCAPE)) {
                editing_field = -1;
                edit_buffer.clear();
            } else if (ray::IsKeyPressed(ray::KEY_ENTER)) {
                commit_edit();
            } else {
                if (ray::IsKeyPressed(ray::KEY_BACKSPACE) && !edit_buffer.empty()) edit_buffer.pop_back();
                int ch;
                while ((ch = ray::GetCharPressed()) > 0) {
                    bool is_digit = ch >= '0' && ch <= '9';
                    bool is_sign  = ch == '-' && edit_buffer.empty();
                    if ((is_digit || is_sign) && edit_buffer.size() < 8) edit_buffer += (char)ch;
                }
            }
        }
    }

    void draw() {
        if (!is_visible()) return;

        const float screen_w  = (float)tex.screen_width;
        const float screen_h  = (float)tex.screen_height;
        const float panel_x   = screen_w - PANEL_WIDTH + slide_offset();
        const float tab_width = PANEL_WIDTH / TAB_COUNT;

        ray::DrawRectangle((int)panel_x, 0, (int)PANEL_WIDTH, (int)screen_h, ray::Fade(ray::BLACK, 0.85f));

        static const char* tab_labels[TAB_COUNT] = {"Textures", "Scenes", "Lua", ""};
        for (int i = 0; i < TAB_COUNT; i++) {
            float tab_x = panel_x + i * tab_width;
            ray::Color tab_color = (i == active_tab) ? ray::Fade(ray::WHITE, 0.3f) : ray::Fade(ray::WHITE, 0.1f);
            ray::DrawRectangle((int)tab_x, 0, (int)tab_width, (int)TAB_HEIGHT, tab_color);
            ray::DrawRectangleLines((int)tab_x, 0, (int)tab_width, (int)TAB_HEIGHT, ray::Fade(ray::WHITE, 0.4f));

            const char* label = tab_labels[i];
            int label_w = measure_text(label, 16);
            int label_x = (int)(tab_x + (tab_width - label_w) * 0.5f);
            int label_y = (int)((TAB_HEIGHT - 16) * 0.5f);
            draw_text(label, label_x, label_y, 16, ray::WHITE);
        }

        if (active_tab == 0) draw_textures_tab(panel_x, screen_h);
        else if (active_tab == 1) draw_scenes_tab(panel_x, screen_h);
        else if (active_tab == 2) draw_lua_tab(panel_x, screen_h);
    }

private:
    struct LuaButtons { ray::Rectangle copy_path, copy_path_line; };

    static LuaButtons lua_buttons(float panel_x, float screen_h) {
        float width = (PANEL_WIDTH - LUA_BTN_GAP * 3) / 2;
        float row_y = screen_h - LUA_BTN_HEIGHT - LUA_BTN_GAP;
        ray::Rectangle left  = {panel_x + LUA_BTN_GAP, row_y, width, LUA_BTN_HEIGHT};
        ray::Rectangle right = {left.x + width + LUA_BTN_GAP, row_y, width, LUA_BTN_HEIGHT};
        return {left, right};
    }

    static fs::path resolve_lua_path(const std::string& source) {
        std::error_code ec;
        fs::path path = fs::weakly_canonical(source, ec);
        if (fs::exists(path, ec)) return path;
        path = fs::weakly_canonical(resolve_skin_path(fs::path("Scripts") / fs::path(source).filename()), ec);
        return fs::exists(path, ec) ? path : fs::path();
    }

    static const char* loaded_by(const std::string& name) {
        if (script_manager.tex.textures.count(name)) return "a lua script (tex.load_folder)";
        if (tex.textures.count(name)) return "c++ (load_screen_textures)";
        if (global_tex.textures.count(name)) return "c++ (global textures)";
        return "unknown";
    }

    static std::string call_text(const std::vector<std::string>& lines, int first_line) {
        std::string text;
        int depth = 0;
        for (int i = first_line - 1; i >= 0 && i < (int)lines.size() && i < first_line - 1 + CALL_TEXT_MAX_ROWS; i++) {
            text += lines[i];
            for (char c : lines[i]) depth += (c == '(') - (c == ')');
            if (depth <= 0) break;
            text += ' ';
        }
        size_t start = text.find_first_not_of(" \t");
        return start == std::string::npos ? std::string() : text.substr(start);
    }

    int lua_chars_per_row() const {
        int ten_chars = std::max(1, measure_code_text("ABCDEFGHIJ", LUA_TEXT_SIZE));
        return std::max(1, (int)((PANEL_WIDTH - 12.0f) * 10.0f / ten_chars));
    }

    float draw_lua_rows(const std::string& text, float x, float y, int max_rows, ray::Color color) const {
        int budget = lua_chars_per_row();
        for (int row = 0; row < max_rows && (size_t)row * budget < text.size(); row++) {
            draw_code_text(text.substr((size_t)row * budget, budget).c_str(), (int)x, (int)y, LUA_TEXT_SIZE, color);
            y += LUA_LINE_HEIGHT;
        }
        return y;
    }

    void draw_lua_button(const ray::Rectangle& box, const char* label, bool enabled) const {
        ray::DrawRectangleRec(box, ray::Fade(ray::WHITE, enabled ? 0.2f : 0.05f));
        ray::DrawRectangleLinesEx(box, 1.0f, ray::Fade(ray::WHITE, 0.4f));
        int label_w = measure_code_text(label, LUA_TEXT_SIZE);
        draw_code_text(label, (int)(box.x + (box.width - label_w) * 0.5f), (int)box.y + 5, LUA_TEXT_SIZE,
                      enabled ? ray::WHITE : ray::GRAY);
    }

    struct SourceFile {
        std::vector<std::string> lines;
        fs::file_time_type mtime;
        double checked_at = -1.0;
    };
    std::map<std::string, SourceFile> source_cache;

    const std::vector<std::string>* source_lines(const fs::path& path) {
        auto it = source_cache.find(path.string());
        if (it == source_cache.end()) {
            if (source_cache.size() >= SOURCE_MAX_FILES) source_cache.clear();
            it = source_cache.emplace(path.string(), SourceFile{}).first;
        }
        SourceFile& file = it->second;
        if (ray::GetTime() - file.checked_at < SOURCE_RESTAT_SECONDS) return file.lines.empty() ? nullptr : &file.lines;

        file.checked_at = ray::GetTime();
        std::error_code ec;
        fs::file_time_type mtime = fs::last_write_time(path, ec);
        if (!ec && (file.lines.empty() || mtime != file.mtime)) {
            file.mtime = mtime;
            file.lines.clear();
            if (fs::file_size(path, ec) <= SOURCE_MAX_BYTES && !ec) {
                std::ifstream in(path);
                for (std::string line; std::getline(in, line); ) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    file.lines.push_back(line);
                }
            }
        }
        return file.lines.empty() ? nullptr : &file.lines;
    }

    int selected_draw_count() const {
        int count = 0;
        for (const DrawLogEntry& entry : debug_draw_log_prev)
            if (entry.name == selected_name && entry.index == selected_tex_index) count++;
        return count;
    }

    void draw_lua_tab(float panel_x, float screen_h) {
        const float x = panel_x + 6.0f;
        float y = TAB_HEIGHT + 8.0f;

        if (!has_selection) {
            draw_lua_rows("Select an element on the Textures tab.", x, y, 1, ray::GRAY);
            return;
        }

        y = draw_lua_rows(selected_name, x, y, 2, ray::WHITE) + 4.0f;

        const DrawLogEntry* entry = find_selected_entry();
        if (!entry) {
            draw_lua_rows("Not drawn this frame.", x, y, 1, ray::GRAY);
            return;
        }

        y = draw_lua_rows(ray::TextFormat("%d draw(s) with this name this frame", selected_draw_count()), x, y, 1, ray::GRAY);
        if (entry->tex_obj)
            y = draw_lua_rows(ray::TextFormat("texture loaded by %s", loaded_by(entry->name)), x, y, 2, ray::GRAY);
        y += 6.0f;

        if (!entry->from_lua) {
            draw_lua_rows("Drawn from C++: no lua frame was on the stack.", x, y, 2, ray::SKYBLUE);
            return;
        }

        fs::path path = entry->lua_source.empty() ? fs::path() : resolve_lua_path(entry->lua_source);
        std::string shown = path.empty() ? entry->lua_source : path.string();
        if (shown.empty()) shown = "(chunk has no file)";
        std::string file = fs::path(shown).filename().string();
        y = draw_lua_rows(entry->lua_function.empty()
                              ? ray::TextFormat("%s:%d, in the function defined at line %d", file.c_str(), entry->lua_line, entry->lua_defined_line)
                              : ray::TextFormat("%s:%d, in %s()", file.c_str(), entry->lua_line, entry->lua_function.c_str()),
                          x, y, 2, ray::ORANGE);
        y = draw_lua_rows(shown, x, y, 4, ray::Fade(ray::ORANGE, 0.6f)) + 6.0f;

        if (!path.empty())
            if (const std::vector<std::string>* lines = source_lines(path))
                draw_lua_rows(call_text(*lines, entry->lua_line), x, y, CALL_TEXT_MAX_ROWS, ray::WHITE);

        LuaButtons buttons = lua_buttons(panel_x, screen_h);
        draw_lua_button(buttons.copy_path, "copy path", !path.empty());
        draw_lua_button(buttons.copy_path_line, "copy path:line", !path.empty());
    }

    std::optional<Screens> selected_screen;
    int scene_scroll = 0;
    int data_scroll = 0;

    struct DataField {
        std::string label;
        enum class Kind { INT, BOOL, STRING, PLAYER_NUM } kind;
        int* i = nullptr;
        bool* b = nullptr;
        std::string* s = nullptr;
        PlayerNum* pn = nullptr;
    };
    void* editing_data_ptr = nullptr;
    DataField::Kind editing_data_kind = DataField::Kind::INT;
    std::string edit_data_buffer;

    static const char* player_num_name(PlayerNum p) {
        switch (p) {
            case PlayerNum::ALL: return "ALL";
            case PlayerNum::P1: return "P1";
            case PlayerNum::P2: return "P2";
            case PlayerNum::TWO_PLAYER: return "TWO_PLAYER";
            case PlayerNum::DAN: return "DAN";
            case PlayerNum::AI: return "AI";
        }
        return "?";
    }

    static const char* difficulty_name(int v) {
        switch ((Difficulty)v) {
            case Difficulty::BACK: return "BACK";
            case Difficulty::MODIFIER: return "MODIFIER";
            case Difficulty::NEIRO: return "NEIRO";
            case Difficulty::EASY: return "EASY";
            case Difficulty::NORMAL: return "NORMAL";
            case Difficulty::HARD: return "HARD";
            case Difficulty::ONI: return "ONI";
            case Difficulty::URA: return "URA";
            case Difficulty::TOWER: return "TOWER";
            case Difficulty::DAN: return "DAN";
        }
        return "?";
    }

    static std::string data_field_display(const DataField& f) {
        switch (f.kind) {
            case DataField::Kind::INT: {
                std::string text = std::to_string(*f.i);
                if (f.label == "session.selected_difficulty")
                    text += std::string(" ") + difficulty_name(*f.i);
                return text;
            }
            case DataField::Kind::BOOL: return *f.b ? "true" : "false";
            case DataField::Kind::STRING: return *f.s;
            case DataField::Kind::PLAYER_NUM: return player_num_name(*f.pn);
        }
        return "";
    }

    static std::vector<DataField> build_data_fields() {
        std::vector<DataField> f;
        GlobalData& g = global_data;
        f.push_back({"player_num", DataField::Kind::PLAYER_NUM, nullptr, nullptr, nullptr, &g.player_num});
        f.push_back({"first_login_player", DataField::Kind::PLAYER_NUM, nullptr, nullptr, nullptr, &g.first_login_player});
        f.push_back({"entry_joined_seat", DataField::Kind::PLAYER_NUM, nullptr, nullptr, nullptr, &g.entry_joined_seat});
        f.push_back({"songs_played", DataField::Kind::INT, &g.songs_played});
        f.push_back({"total_songs", DataField::Kind::INT, &g.total_songs});
        f.push_back({"force_auto_play", DataField::Kind::BOOL, nullptr, &g.force_auto_play});
        f.push_back({"returned_from_result", DataField::Kind::BOOL, nullptr, &g.returned_from_result});
        f.push_back({"entry_join_pending", DataField::Kind::BOOL, nullptr, &g.entry_join_pending});
        f.push_back({"live_skip_count", DataField::Kind::INT, &g.live_skip_count});
        f.push_back({"live_skip_used", DataField::Kind::BOOL, nullptr, &g.live_skip_used});

        size_t idx = std::min((size_t)g.player_num, g.session_data.size() - 1);
        SessionData& sd = g.session_data[idx];
        f.push_back({"session.selected_difficulty", DataField::Kind::INT, &sd.selected_difficulty});
        f.push_back({"session.genre_index", DataField::Kind::INT, &sd.genre_index});
        f.push_back({"session.song_title", DataField::Kind::STRING, nullptr, nullptr, &sd.song_title});
        f.push_back({"session.song_subtitle", DataField::Kind::STRING, nullptr, nullptr, &sd.song_subtitle});
        f.push_back({"session.subtitle_full_display", DataField::Kind::BOOL, nullptr, &sd.song_subtitle_full_display});
        f.push_back({"session.song_hash", DataField::Kind::STRING, nullptr, nullptr, &sd.song_hash});
        f.push_back({"session.dan_color", DataField::Kind::INT, &sd.dan_color});
        f.push_back({"session.dan_rank", DataField::Kind::INT, &sd.dan_rank});
        f.push_back({"session.dan_index", DataField::Kind::INT, &sd.dan_index});
        f.push_back({"session.dan_index_max", DataField::Kind::INT, &sd.dan_index_max});
        f.push_back({"session.dan_gaiden", DataField::Kind::BOOL, nullptr, &sd.dan_gaiden});
        return f;
    }

    struct DataFieldRects { ray::Rectangle minus, plus, value; };

    static DataFieldRects data_field_rects(const DataField& f, float panel_x, float row_y) {
        float btn_size = ROW_HEIGHT - 4.0f;
        if (f.kind == DataField::Kind::BOOL || f.kind == DataField::Kind::STRING) {
            ray::Rectangle value = {panel_x + 148, row_y + 1, PANEL_WIDTH - 148 - 6, ROW_HEIGHT - 2};
            return {{}, {}, value};
        }
        ray::Rectangle minus = {panel_x + 148, row_y + 1, btn_size, btn_size};
        ray::Rectangle value = {minus.x + btn_size + 4, row_y + 1, 88.0f, btn_size};
        ray::Rectangle plus  = {value.x + value.width + 4, row_y + 1, btn_size, btn_size};
        return {minus, plus, value};
    }

    void draw_scenes_tab(float panel_x, float screen_h) {
        const float list_top    = TAB_HEIGHT;
        const float list_bottom = list_top + SCENE_LIST_HEIGHT;
        const int   scene_count = (int)std::size(ALL_SCREENS);
        const int   scene_shown = std::max(0, (int)(SCENE_LIST_HEIGHT / ROW_HEIGHT));

        const int scissor_x0 = virtual_to_screen_x(panel_x);
        const int scissor_x1 = virtual_to_screen_x(panel_x + PANEL_WIDTH);
        int scissor_y0 = virtual_to_screen_y(list_top);
        int scissor_y1 = virtual_to_screen_y(list_bottom);
        ray::BeginScissorMode(scissor_x0, scissor_y0, scissor_x1 - scissor_x0, scissor_y1 - scissor_y0);
        for (int row = 0; row < scene_shown; row++) {
            int idx = row + scene_scroll;
            if (idx >= scene_count) break;
            float row_y = list_top + row * ROW_HEIGHT;
            std::string name = screens_to_string(ALL_SCREENS[idx]);
            bool is_current  = name == global_data.current_screen;
            bool is_selected = selected_screen.has_value() && *selected_screen == ALL_SCREENS[idx];

            if (is_selected) {
                ray::DrawRectangle((int)panel_x, (int)row_y, (int)PANEL_WIDTH, (int)ROW_HEIGHT, ray::Fade(ray::YELLOW, 0.3f));
            } else if (is_current) {
                ray::DrawRectangle((int)panel_x, (int)row_y, (int)PANEL_WIDTH, (int)ROW_HEIGHT, ray::Fade(ray::SKYBLUE, 0.35f));
            }
            ray::Color color = is_selected ? ray::YELLOW : (is_current ? ray::SKYBLUE : ray::WHITE);
            draw_text(name.c_str(), (int)panel_x + 4, (int)row_y + 3, 14, color);
        }
        ray::EndScissorMode();

        if (scene_count > scene_shown) {
            float track_x = panel_x + PANEL_WIDTH - SCROLLBAR_WIDTH;
            ray::DrawRectangle((int)track_x, (int)list_top, (int)SCROLLBAR_WIDTH, (int)SCENE_LIST_HEIGHT, ray::Fade(ray::WHITE, 0.1f));
            int max_scroll = scene_count - scene_shown;
            float thumb_h = std::max(10.0f, SCENE_LIST_HEIGHT * ((float)scene_shown / scene_count));
            float thumb_y = list_top + (max_scroll > 0 ? (scene_scroll / (float)max_scroll) * (SCENE_LIST_HEIGHT - thumb_h) : 0.0f);
            ray::DrawRectangle((int)track_x, (int)thumb_y, (int)SCROLLBAR_WIDTH, (int)thumb_h, ray::Fade(ray::WHITE, 0.5f));
        }

        const float button_top = list_bottom;
        ray::Rectangle button = {panel_x + 6, button_top + 3, PANEL_WIDTH - 12, SWITCH_BTN_HEIGHT - 6};
        bool enabled = selected_screen.has_value();
        ray::DrawRectangleRec(button, ray::Fade(ray::WHITE, enabled ? 0.25f : 0.08f));
        ray::DrawRectangleLinesEx(button, 1.0f, enabled ? ray::YELLOW : ray::Fade(ray::WHITE, 0.3f));
        std::string label = enabled ? ("Switch to " + screens_to_string(*selected_screen)) : "Select a scene above";
        int label_w = measure_text(label.c_str(), 14);
        draw_text(label.c_str(), (int)(button.x + (button.width - label_w) * 0.5f), (int)button.y + 6, 14,
                      enabled ? ray::YELLOW : ray::GRAY);

        const float data_top    = button_top + SWITCH_BTN_HEIGHT;
        const float data_bottom = screen_h;
        std::vector<DataField> fields = build_data_fields();
        const int   field_count = (int)fields.size();
        const int   data_shown  = std::max(0, (int)((data_bottom - data_top) / ROW_HEIGHT));

        ray::DrawLine((int)panel_x, (int)data_top, (int)(panel_x + PANEL_WIDTH), (int)data_top, ray::Fade(ray::WHITE, 0.4f));
        scissor_y0 = virtual_to_screen_y(data_top);
        scissor_y1 = virtual_to_screen_y(data_bottom);
        ray::BeginScissorMode(scissor_x0, scissor_y0, scissor_x1 - scissor_x0, scissor_y1 - scissor_y0);
        for (int row = 0; row < data_shown; row++) {
            int idx = row + data_scroll;
            if (idx >= field_count) break;
            const DataField& f = fields[idx];
            float row_y = data_top + row * ROW_HEIGHT;
            DataFieldRects r = data_field_rects(f, panel_x, row_y);

            draw_text(f.label.c_str(), (int)panel_x + 4, (int)row_y + 4, 12, ray::WHITE);

            if (f.kind == DataField::Kind::BOOL) {
                bool v = *f.b;
                ray::DrawRectangleRec(r.value, ray::Fade(v ? ray::GREEN : ray::RED, 0.25f));
                ray::DrawRectangleLinesEx(r.value, 1.0f, ray::Fade(ray::WHITE, 0.4f));
                draw_text(v ? "true" : "false", (int)r.value.x + 4, (int)r.value.y + 3, 13, ray::WHITE);
            } else if (f.kind == DataField::Kind::STRING) {
                bool is_editing = editing_data_ptr == static_cast<void*>(f.s);
                ray::DrawRectangleRec(r.value, ray::Fade(ray::WHITE, is_editing ? 0.25f : 0.1f));
                ray::DrawRectangleLinesEx(r.value, 1.0f, is_editing ? ray::SKYBLUE : ray::Fade(ray::WHITE, 0.4f));
                std::string text = is_editing ? edit_data_buffer : *f.s;
                if (is_editing && std::fmod(ray::GetTime(), 1.0) < 0.5) text += "|";
                draw_text(text.c_str(), (int)r.value.x + 4, (int)r.value.y + 3, 13, ray::WHITE);
            } else if (f.kind == DataField::Kind::PLAYER_NUM) {
                ray::DrawRectangleRec(r.minus, ray::Fade(ray::WHITE, 0.2f));
                draw_text("-", (int)r.minus.x + 5, (int)r.minus.y, 14, ray::WHITE);
                ray::DrawRectangleRec(r.plus, ray::Fade(ray::WHITE, 0.2f));
                draw_text("+", (int)r.plus.x + 4, (int)r.plus.y, 14, ray::WHITE);
                const char* name = player_num_name(*f.pn);
                int name_w = measure_text(name, 13);
                draw_text(name, (int)(r.value.x + (r.value.width - name_w) * 0.5f), (int)r.value.y + 3, 13, ray::WHITE);
            } else { // INT
                bool is_editing = editing_data_ptr == static_cast<void*>(f.i);
                ray::DrawRectangleRec(r.minus, ray::Fade(ray::WHITE, 0.2f));
                draw_text("-", (int)r.minus.x + 5, (int)r.minus.y, 14, ray::WHITE);
                ray::DrawRectangleRec(r.plus, ray::Fade(ray::WHITE, 0.2f));
                draw_text("+", (int)r.plus.x + 4, (int)r.plus.y, 14, ray::WHITE);
                ray::DrawRectangleRec(r.value, ray::Fade(ray::WHITE, is_editing ? 0.25f : 0.1f));
                ray::DrawRectangleLinesEx(r.value, 1.0f, is_editing ? ray::SKYBLUE : ray::Fade(ray::WHITE, 0.4f));
                std::string text = is_editing ? edit_data_buffer : data_field_display(f);
                if (is_editing && std::fmod(ray::GetTime(), 1.0) < 0.5) text += "|";
                draw_text(text.c_str(), (int)r.value.x + 4, (int)r.value.y + 3, 12, ray::WHITE);
            }
        }
        ray::EndScissorMode();

        if (field_count > data_shown) {
            float track_x = panel_x + PANEL_WIDTH - SCROLLBAR_WIDTH;
            float area_h = data_bottom - data_top;
            ray::DrawRectangle((int)track_x, (int)data_top, (int)SCROLLBAR_WIDTH, (int)area_h, ray::Fade(ray::WHITE, 0.1f));
            int max_scroll = field_count - data_shown;
            float thumb_h = std::max(10.0f, area_h * ((float)data_shown / field_count));
            float thumb_y = data_top + (max_scroll > 0 ? (data_scroll / (float)max_scroll) * (area_h - thumb_h) : 0.0f);
            ray::DrawRectangle((int)track_x, (int)thumb_y, (int)SCROLLBAR_WIDTH, (int)thumb_h, ray::Fade(ray::WHITE, 0.5f));
        }
    }

    bool is_selected_entry(const DrawLogEntry& entry) const {
        return entry.name == selected_name && entry.index == selected_tex_index;
    }

    const DrawLogEntry* find_selected_entry() const {
        if (!has_selection) return nullptr;
        if (selected_log_index >= 0 && selected_log_index < (int)debug_draw_log_prev.size()) {
            const DrawLogEntry& entry = debug_draw_log_prev[selected_log_index];
            if (is_selected_entry(entry)) return &entry;
        }
        for (const DrawLogEntry& entry : debug_draw_log_prev)
            if (is_selected_entry(entry)) return &entry;
        return nullptr;
    }

    TextureObject* selected_obj() const {
        const DrawLogEntry* entry = find_selected_entry();
        return entry ? entry->tex_obj : nullptr;
    }

    enum class Position { Json, JsonPlusOffset, NoJson, Unknown };

    static Position position_of(const DrawLogEntry& entry) {
        if (!entry.tex_obj) return Position::NoJson;
        if (entry.origin.x != 0 || entry.origin.y != 0 || entry.rotation != 0) return Position::Unknown;
        if (entry.offset_x == 0 && entry.offset_y == 0 && !(entry.center && entry.scale != 1.0f)) return Position::Json;
        return Position::JsonPlusOffset;
    }

    void draw_verdict(const DrawLogEntry& entry, float panel_x, float top) {
        ray::Color color = ray::GRAY;
        std::string headline, advice;
        switch (position_of(entry)) {
            case Position::Json:
                color = ray::GREEN;
                headline = "position comes from texture.json";
                break;
            case Position::JsonPlusOffset:
                color = ray::ORANGE;
                headline = ray::TextFormat("caller adds x%+.0f y%+.0f to the json base", entry.offset_x, entry.offset_y);
                break;
            case Position::NoJson:
                color = ray::RED;
                headline = "no texture.json behind this draw";
                advice = "x/y here do nothing; the position is set in code";
                break;
            case Position::Unknown:
                headline = "origin/rotation in use";
                advice = "the box and these numbers are approximate";
                break;
        }
        if (advice.empty())
            advice = entry.scale != 1.0f
                ? ray::TextFormat("x/y move it 1:1; x2/y2 change by x%.2f", entry.scale)
                : "x/y move it 1:1 whatever the caller adds";
        ray::DrawRectangle((int)panel_x, (int)top, (int)PANEL_WIDTH, (int)VERDICT_HEIGHT, ray::Fade(color, 0.3f));
        draw_text(headline.c_str(), (int)panel_x + 6, (int)top + 3, 12, ray::WHITE);
        draw_text(advice.c_str(), (int)panel_x + 6, (int)top + 18, 12, ray::Fade(ray::WHITE, 0.75f));
    }

    struct FieldButtons { ray::Rectangle minus, plus, value; };

    struct VisualRow {
        bool is_header;
        std::string subset;
        std::string label;
        int log_index = -1;
    };
    std::vector<VisualRow> visible_rows;
    std::unordered_set<std::string> expanded_subsets;

    static std::pair<std::string, std::string> split_subset(const DrawLogEntry& e) {
        if (e.tex_obj) {
            size_t slash = e.name.find('/');
            if (slash != std::string::npos) return {e.name.substr(0, slash), e.name.substr(slash + 1)};
        }
        if (!e.from_lua) return {"[c++]", e.name};
        return {e.lua_source.empty() ? "[lua]" : fs::path(e.lua_source).stem().string(), e.name};
    }

    void rebuild_visible_rows() {
        visible_rows.clear();
        std::map<std::string, std::vector<int>> groups;
        for (int i = 0; i < (int)debug_draw_log_prev.size(); i++) {
            groups[split_subset(debug_draw_log_prev[i]).first].push_back(i);
        }
        for (auto& [subset, indices] : groups) {
            std::sort(indices.begin(), indices.end(), [](int a, int b) {
                return split_subset(debug_draw_log_prev[a]).second < split_subset(debug_draw_log_prev[b]).second;
            });
            visible_rows.push_back({true, subset, subset + " (" + std::to_string(indices.size()) + ")", -1});
            if (expanded_subsets.count(subset)) {
                for (int idx : indices) {
                    visible_rows.push_back({false, "", split_subset(debug_draw_log_prev[idx]).second, idx});
                }
            }
        }
    }

    static bool in_rect(ray::Vector2 p, const ray::Rectangle& r) {
        return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
    }

    int* field_ptr(int field_idx) {
        TextureObject* obj = selected_obj();
        if (!obj) return nullptr;
        size_t idx = (size_t)selected_tex_index;
        switch (field_idx) {
            case 0: return idx < obj->x.size()  ? &obj->x[idx]  : nullptr;
            case 1: return idx < obj->y.size()  ? &obj->y[idx]  : nullptr;
            case 2: return idx < obj->x2.size() ? &obj->x2[idx] : nullptr;
            case 3: return idx < obj->y2.size() ? &obj->y2[idx] : nullptr;
        }
        return nullptr;
    }

    static const char* field_label(int field_idx) {
        static const char* labels[4] = {"x", "y", "x2", "y2"};
        return labels[field_idx];
    }

    static FieldButtons field_buttons(int field_idx, float panel_x, float edit_top) {
        float row_y = edit_top + EDIT_FIELDS_TOP + field_idx * EDIT_ROW_HEIGHT;
        float btn_size = EDIT_ROW_HEIGHT - 6.0f;
        ray::Rectangle minus = {panel_x + 40, row_y + 3, btn_size, btn_size};
        ray::Rectangle value = {minus.x + btn_size + 6, row_y + 2, 60.0f, btn_size + 2};
        ray::Rectangle plus  = {value.x + value.width + 6, row_y + 3, btn_size, btn_size};
        return {minus, plus, value};
    }

    struct FrameCellButtons { ray::Rectangle left, right; };

    static FrameCellButtons frame_cell_buttons(int idx, float panel_x, float frames_top) {
        int cols = frame_grid_cols();
        float cell_x = panel_x + (idx % cols) * FRAME_CELL_WIDTH;
        float row_y  = frames_top + FRAMES_HEADER_HEIGHT + (idx / cols) * FRAME_ROW_HEIGHT;
        float btn_row_y = row_y + FRAME_THUMB_SIZE + 4.0f;
        float btn_size = FRAME_BTN_ROW_HEIGHT - 4.0f;
        ray::Rectangle left  = {cell_x + 2, btn_row_y, btn_size, btn_size};
        ray::Rectangle right = {cell_x + FRAME_CELL_WIDTH - btn_size - 2, btn_row_y, btn_size, btn_size};
        return {left, right};
    }

    const ray::Rectangle* find_selected_rect() const {
        const DrawLogEntry* entry = find_selected_entry();
        return entry ? &entry->rect : nullptr;
    }

    void draw_textures_tab(float panel_x, float screen_h) {
        const float list_top    = TAB_HEIGHT;
        const float list_bottom = screen_h - edit_panel_height();
        const int   row_count   = (int)visible_rows.size();
        const int   rows_shown  = std::max(0, (int)((list_bottom - list_top) / ROW_HEIGHT));

        const DrawLogEntry* selected = find_selected_entry();

        int scissor_x0 = virtual_to_screen_x(panel_x);
        int scissor_x1 = virtual_to_screen_x(panel_x + PANEL_WIDTH);
        int scissor_y0 = virtual_to_screen_y(list_top);
        int scissor_y1 = virtual_to_screen_y(list_bottom);
        ray::BeginScissorMode(scissor_x0, scissor_y0, scissor_x1 - scissor_x0, scissor_y1 - scissor_y0);
        for (int row = 0; row < rows_shown; row++) {
            int ridx = row + scroll_offset;
            if (ridx >= row_count) break;
            const VisualRow& vr = visible_rows[ridx];
            float row_y = list_top + row * ROW_HEIGHT;

            if (vr.is_header) {
                bool expanded = expanded_subsets.count(vr.subset) != 0;
                ray::DrawRectangle((int)panel_x, (int)row_y, (int)PANEL_WIDTH, (int)ROW_HEIGHT, ray::Fade(ray::WHITE, 0.12f));
                draw_text(expanded ? "v" : ">", (int)panel_x + 4, (int)row_y + 3, 14, ray::WHITE);
                draw_text(vr.label.c_str(), (int)panel_x + 18, (int)row_y + 3, 14, ray::WHITE);
                continue;
            }

            const DrawLogEntry& e = debug_draw_log_prev[vr.log_index];
            bool is_hovered  = (vr.log_index == hovered_log_index);
            bool is_selected = (&e == selected);
            if (is_selected) {
                ray::DrawRectangle((int)panel_x, (int)row_y, (int)PANEL_WIDTH, (int)ROW_HEIGHT, ray::Fade(ray::SKYBLUE, 0.35f));
            } else if (is_hovered) {
                ray::DrawRectangle((int)panel_x, (int)row_y, (int)PANEL_WIDTH, (int)ROW_HEIGHT, ray::Fade(ray::YELLOW, 0.3f));
            }
            ray::Color color = is_selected ? ray::SKYBLUE : (is_hovered ? ray::YELLOW : ray::WHITE);
            draw_text(vr.label.c_str(), (int)panel_x + 20, (int)row_y + 3, 14, color);
        }
        ray::EndScissorMode();

        if (row_count > rows_shown) {
            float track_x = panel_x + PANEL_WIDTH - SCROLLBAR_WIDTH;
            ray::DrawRectangle((int)track_x, (int)list_top, (int)SCROLLBAR_WIDTH, (int)(list_bottom - list_top), ray::Fade(ray::WHITE, 0.1f));
            float list_height = list_bottom - list_top;
            float thumb_h = std::max(12.0f, list_height * ((float)rows_shown / row_count));
            int max_scroll = row_count - rows_shown;
            float thumb_y = list_top + (max_scroll > 0 ? (scroll_offset / (float)max_scroll) * (list_height - thumb_h) : 0.0f);
            ray::DrawRectangle((int)track_x, (int)thumb_y, (int)SCROLLBAR_WIDTH, (int)thumb_h, ray::Fade(ray::WHITE, 0.5f));
        }

        const ray::Rectangle* box_rect = nullptr;
        std::string box_name;
        if (hovered_log_index >= 0) {
            box_rect = &debug_draw_log_prev[hovered_log_index].rect;
            box_name = debug_draw_log_prev[hovered_log_index].name;
        } else if (const ray::Rectangle* r = find_selected_rect()) {
            box_rect = r;
            box_name = selected_name;
        }
        if (box_rect) {
            ray::DrawRectangleLinesEx(*box_rect, 2.0f, ray::YELLOW);
            float label_y = box_rect->y - 20.0f;
            if (label_y < 0) label_y = box_rect->y + box_rect->height + 2.0f;
            int label_w = measure_text(box_name.c_str(), 18);
            ray::DrawRectangle((int)box_rect->x - 2, (int)label_y - 2, label_w + 4, 22, ray::Fade(ray::BLACK, 0.8f));
            draw_text(box_name.c_str(), (int)box_rect->x, (int)label_y, 18, ray::YELLOW);
        }

        draw_edit_panel(panel_x, list_bottom);
    }

    void draw_edit_panel(float panel_x, float edit_top) {
        ray::DrawRectangle((int)panel_x, (int)edit_top, (int)PANEL_WIDTH, (int)edit_panel_height(), ray::Fade(ray::WHITE, 0.05f));
        ray::DrawLine((int)panel_x, (int)edit_top, (int)(panel_x + PANEL_WIDTH), (int)edit_top, ray::Fade(ray::WHITE, 0.4f));

        if (!has_selection) {
            draw_text("Click a texture to select it", (int)panel_x + 8, (int)edit_top + 8, 14, ray::GRAY);
            return;
        }

        draw_text(selected_name.c_str(), (int)panel_x + 8, (int)edit_top + 8, 16, ray::WHITE);

        const DrawLogEntry* entry = find_selected_entry();
        if (!entry) {
            draw_text("(not drawn this frame)", (int)panel_x + 8, (int)edit_top + 26, 14, ray::GRAY);
            return;
        }
        draw_verdict(*entry, panel_x, edit_top + VERDICT_TOP);

        TextureObject* obj = entry->tex_obj;
        if (!obj) return;

        const char* info = ray::TextFormat("%dx%d px, %d frame(s)", obj->width,
                                            obj->height, obj->frame_count());
        draw_text(info, (int)panel_x + 8, (int)edit_top + 26, 14, ray::GRAY);

        for (int i = 0; i < 4; i++) {
            int* value = field_ptr(i);
            if (!value) continue;
            FieldButtons b = field_buttons(i, panel_x, edit_top);
            float row_y = edit_top + EDIT_FIELDS_TOP + i * EDIT_ROW_HEIGHT;

            draw_text(field_label(i), (int)panel_x + 8, (int)row_y + 4, 14, ray::WHITE);

            ray::DrawRectangleRec(b.minus, ray::Fade(ray::WHITE, 0.2f));
            draw_text("-", (int)b.minus.x + 6, (int)b.minus.y + 1, 16, ray::WHITE);
            ray::DrawRectangleRec(b.plus, ray::Fade(ray::WHITE, 0.2f));
            draw_text("+", (int)b.plus.x + 5, (int)b.plus.y + 1, 16, ray::WHITE);

            bool is_editing = (editing_field == i);
            ray::DrawRectangleRec(b.value, ray::Fade(ray::WHITE, is_editing ? 0.25f : 0.1f));
            ray::DrawRectangleLinesEx(b.value, 1.0f, is_editing ? ray::SKYBLUE : ray::Fade(ray::WHITE, 0.4f));

            std::string text = is_editing ? edit_buffer : std::to_string(*value);
            if (is_editing && std::fmod(ray::GetTime(), 1.0) < 0.5) text += "|";
            draw_text(text.c_str(), (int)b.value.x + 4, (int)b.value.y + 4, 14, ray::WHITE);
        }

        draw_frames_section(panel_x, edit_top);
    }

    void draw_frames_section(float panel_x, float edit_top) {
        FramedTexture* framed = get_selected_framed();
        if (!framed || framed->frame_count() <= 1) return;

        const int frame_count = framed->frame_count();
        const float frames_top = edit_top + EDIT_PANEL_HEIGHT;
        const int cols = frame_grid_cols();

        draw_text(ray::TextFormat("Frames (%d)", frame_count), (int)panel_x + 8, (int)frames_top + 2, 14, ray::WHITE);

        for (int idx = 0; idx < frame_count; idx++) {
            float cell_x = panel_x + (idx % cols) * FRAME_CELL_WIDTH;
            float row_y  = frames_top + FRAMES_HEADER_HEIGHT + (idx / cols) * FRAME_ROW_HEIGHT;

            ray::Rectangle thumb_bg = {cell_x + (FRAME_CELL_WIDTH - FRAME_THUMB_SIZE) * 0.5f, row_y,
                                        FRAME_THUMB_SIZE, FRAME_THUMB_SIZE};
            ray::DrawRectangleRec(thumb_bg, ray::Fade(ray::BLACK, 0.5f));
            const ray::Texture2D& t = framed->textures[idx];
            ray::Rectangle src = {0, 0, (float)t.width, (float)t.height};
            ray::DrawTexturePro(t, src, thumb_bg, {0, 0}, 0, ray::WHITE);
            ray::DrawRectangleLinesEx(thumb_bg, 1.0f, ray::Fade(ray::WHITE, 0.4f));

            FrameCellButtons b = frame_cell_buttons(idx, panel_x, frames_top);
            ray::DrawRectangleRec(b.left, ray::Fade(ray::WHITE, idx > 0 ? 0.2f : 0.05f));
            draw_text("<", (int)b.left.x + 4, (int)b.left.y, 14, idx > 0 ? ray::WHITE : ray::GRAY);

            const char* index_text = ray::TextFormat("%d", idx);
            int index_w = measure_text(index_text, 14);
            draw_text(index_text, (int)(cell_x + (FRAME_CELL_WIDTH - index_w) * 0.5f), (int)b.left.y + 2, 14, ray::WHITE);

            ray::DrawRectangleRec(b.right, ray::Fade(ray::WHITE, idx < frame_count - 1 ? 0.2f : 0.05f));
            draw_text(">", (int)b.right.x + 4, (int)b.right.y, 14, idx < frame_count - 1 ? ray::WHITE : ray::GRAY);
        }
    }
};

inline DebugMenu debug_menu;
