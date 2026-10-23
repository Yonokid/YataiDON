#include "song_select_script.h"

#include "../../libs/ime_ui.h"
#include "diff_sort.h"
#include "file_navigator/box_back.h"
#include "file_navigator/box_folder.h"
#include "file_navigator/box_song.h"
#include "file_navigator/genre_bg.h"
#include "file_navigator/navigator.h"
#include "player.h"

SongSelectScript::SongSelectScript() {
    if (!load("SongSelect", "song_select")) {
        return;
    }
    fn_update = lua_object["update"];
    fn_restart_text_fade = lua_object["restart_text_fade"];
    fn_draw_footer = lua_object["draw_footer"];
    fn_draw_overlays = lua_object["draw_overlays"];
    fn_draw_top = lua_object["draw_top"];
    fn_draw_box = lua_object["draw_box"];
    fn_draw_box_bg = lua_object["draw_box_bg"];
    fn_draw_background = lua_object["draw_background"];
    fn_draw_genre_bg = lua_object["draw_genre_bg"];
    fn_draw_selector = lua_object["draw_selector"];
    fn_draw_option_panel = lua_object["draw_option_panel"];
    fn_draw_sort_window = lua_object["draw_sort_window"];
    fn_default_option_panel = load_default_hook("song_select/option_panel", "draw_option_panel");
    fn_default_sort_window = load_default_hook("song_select/sort_window", "draw_sort_window");
    fn_draw_background_diffs = lua_object["draw_background_diffs"];
    fn_draw_search_box = lua_object["draw_search_box"];
    fn_default_search_box = load_default_hook("song_select/player_ui", "draw_search_box");
    fn_default_selector = load_default_hook("song_select/player_ui", "draw_selector");
    fn_default_background_diffs = load_default_hook("song_select/player_ui", "draw_background_diffs");
}

sol::protected_function load_skin_module_fn(const char* module, const char* fn_name) {
    sol::protected_function require = (*script_manager.lua)["require"];
    auto result = require(module);
    if (!result.valid()) {
        sol::error err = result;
        spdlog::warn("module {} unavailable: {}", module, err.what());
        return {};
    }
    sol::table t = result;
    return t[fn_name];
}

sol::protected_function cached_skin_module_fn(const char* module, const char* fn_name, const char* global_name) {
    sol::state& lua = *script_manager.lua;
    sol::protected_function fn = lua[global_name];
    if (fn.valid()) {
        return fn;
    }
    fn = load_skin_module_fn(module, fn_name);
    if (fn.valid()) {
        lua[global_name] = fn;
    }
    return fn;
}

float option_panel_offset(const MoveAnimation* move, bool confirmed) {
    float v = (float)move->attribute;
    if (!confirmed) {
        return -v;
    }
    sol::protected_function slide_out =
        cached_skin_module_fn("song_select/option_panel", "slide_out", "__option_panel_slide_out");
    auto r = slide_out.valid() ? slide_out() : sol::protected_function_result();
    return v + (r.valid() ? r.get<float>() : 0.0f);
}

sol::protected_function SongSelectScript::load_default_hook(const char* module, const char* fn_name) {
    return load_skin_module_fn(module, fn_name);
}

double SongSelectScript::animation_ms() {
    const double now = get_current_ms();

    if (fast_open_started_ms) {
        const double elapsed = std::clamp(now - *fast_open_started_ms, 0.0, 500.0);

        if (elapsed < 500.0) {
            return now + clock_offset_ms + elapsed * 4.0;
        }

        clock_offset_ms += 2000.0;
        fast_open_started_ms.reset();
    }

    return now + clock_offset_ms;
}

void SongSelectScript::start_folder_open() {
    finish_folder_open();

    if (!global_data.config->general.fast_transitions) {
        return;
    }

    fast_open_started_ms = get_current_ms();
}

void SongSelectScript::finish_folder_open() {
    if (!fast_open_started_ms) {
        return;
    }

    // Retain the elapsed offset so skin timers never run backwards when acceleration ends.
    clock_offset_ms += std::clamp(get_current_ms() - *fast_open_started_ms, 0.0, 500.0) * 4.0;
    fast_open_started_ms.reset();
}

void SongSelectScript::update(double current_ms) {
    call(fn_update, "SongSelect:update", current_ms);
}
void SongSelectScript::restart_text_fade() {
    call(fn_restart_text_fade, "SongSelect:restart_text_fade");
}
void SongSelectScript::draw_footer() {
    call(fn_draw_footer, "SongSelect:draw_footer");
}
void SongSelectScript::draw_overlays(int state) {
    call(fn_draw_overlays, "SongSelect:draw_overlays", state);
}

void SongSelectScript::draw_top(float dan_progress) {
    if (!fn_draw_top.valid()) {
        return;
    }
    call(fn_draw_top, "SongSelect:draw_top", dan_progress);
}

sol::object SongSelectScript::box_to_lua(BaseBox* box) {
    sol::state& lua = *script_manager.lua;
    if (auto* song = dynamic_cast<SongBox*>(box)) {
        return sol::make_object(lua, song);
    }
    if (auto* folder = dynamic_cast<FolderBox*>(box)) {
        return sol::make_object(lua, folder);
    }
    if (auto* back = dynamic_cast<BackBox*>(box)) {
        return sol::make_object(lua, back);
    }
    return sol::make_object(lua, box);
}

bool SongSelectScript::draw_box(BaseBox* box) {
    if (!fn_draw_box.valid()) {
        return false;
    }
    call(fn_draw_box, "SongSelect:draw_box", box_to_lua(box));
    return true;
}

bool SongSelectScript::draw_box_bg(BaseBox* box) {
    if (!fn_draw_box_bg.valid()) {
        return false;
    }
    call(fn_draw_box_bg, "SongSelect:draw_box_bg", box_to_lua(box));
    return true;
}

bool SongSelectScript::draw_background(Navigator* nav) {
    if (!fn_draw_background.valid()) {
        return false;
    }
    call(fn_draw_background, "SongSelect:draw_background", nav);
    return true;
}

bool SongSelectScript::draw_genre_bg(GenreBG* genre_bg, float start_pos, float end_pos, BaseBox* folder) {
    if (!fn_draw_genre_bg.valid()) {
        return false;
    }
    call(fn_draw_genre_bg, "SongSelect:draw_genre_bg", genre_bg, start_pos, end_pos, box_to_lua(folder));
    return true;
}

bool SongSelectScript::draw_selector(SongSelectPlayer* player, bool is_half, float fade_in, int pass) {
    if (fn_draw_selector.valid() &&
        call_handled(fn_draw_selector, "SongSelect:draw_selector", player, is_half, fade_in, pass)) {
        return true;
    }
    call(fn_default_selector, "player_ui.draw_selector", player, is_half, fade_in, pass);
    return false;
}

bool SongSelectScript::draw_background_diffs(SongSelectPlayer* player) {
    if (fn_draw_background_diffs.valid() &&
        call_handled(fn_draw_background_diffs, "SongSelect:draw_background_diffs", player)) {
        return true;
    }
    call(fn_default_background_diffs, "player_ui.draw_background_diffs", player);
    return false;
}

bool SongSelectScript::draw_option_panel(SongSelectPlayer* player, int kind) {
    if (fn_draw_option_panel.valid() &&
        call_handled(fn_draw_option_panel, "SongSelect:draw_option_panel", player, kind)) {
        return true;
    }
    call(fn_default_option_panel, "option_panel.draw_option_panel", player, kind);
    return false;
}

bool SongSelectScript::draw_sort_window(DiffSortSelect* window) {
    if (fn_draw_sort_window.valid() && call_handled(fn_draw_sort_window, "SongSelect:draw_sort_window", window)) {
        return true;
    }
    call(fn_default_sort_window, "sort_window.draw_sort_window", window);
    return false;
}

bool SongSelectScript::draw_search_box(const std::string& search, double bg_resize, double fade_in) {
    sol::table box = script_manager.lua->create_table();
    box["search"] = search;
    box["bg_resize"] = bg_resize;
    box["fade_in"] = fade_in;
    { // the IME overlay anchors to the text box player_ui.draw_search_box draws (skin's search_box_layout)
        const float S = tex.screen_scale;
        float lx = 0;
        float ly = 0;
        float lw = 400 * S;
        float lh = 60 * S;
        if (lua_object.valid()) {
            sol::optional<sol::table> layout = lua_object["search_box_layout"];
            if (layout) {
                lx = layout->get_or<float>("x", lx);
                ly = layout->get_or<float>("y", ly);
                lw = layout->get_or<float>("width", lw);
                lh = layout->get_or<float>("height", lh);
            }
        }
        const float cx = tex.screen_width / 2.0f;
        const float cy = tex.screen_height / 2.0f;
        ime_ui_set_field({cx - lw / 2 + lx, cy - lh / 2 + ly, lw, lh});
    }
    if (fn_draw_search_box.valid() && call_handled(fn_draw_search_box, "SongSelect:draw_search_box", box)) {
        return true;
    }
    call(fn_default_search_box, "player_ui.draw_search_box", box);
    return false;
}
