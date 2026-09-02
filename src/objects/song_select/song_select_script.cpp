#include "song_select_script.h"
#include "file_navigator/box_song.h"
#include "file_navigator/box_folder.h"
#include "file_navigator/box_back.h"
#include "file_navigator/navigator.h"
#include "player.h"
#include "diff_sort.h"

SongSelectScript::SongSelectScript() {
    if (!load("SongSelect", "song_select")) return;
    fn_update            = lua_object["update"];
    fn_restart_text_fade = lua_object["restart_text_fade"];
    fn_draw_footer       = lua_object["draw_footer"];
    fn_draw_overlays     = lua_object["draw_overlays"];
    fn_draw_top          = lua_object["draw_top"];
    fn_draw_box          = lua_object["draw_box"];
    fn_draw_box_bg       = lua_object["draw_box_bg"];
    fn_draw_background   = lua_object["draw_background"];
    fn_draw_selector     = lua_object["draw_selector"];
    fn_draw_option_panel = lua_object["draw_option_panel"];
    fn_draw_sort_window  = lua_object["draw_sort_window"];
}

void SongSelectScript::update(double current_ms) { call(fn_update, "SongSelect:update", current_ms); }
void SongSelectScript::restart_text_fade()        { call(fn_restart_text_fade, "SongSelect:restart_text_fade"); }
void SongSelectScript::draw_footer()              { call(fn_draw_footer, "SongSelect:draw_footer"); }
void SongSelectScript::draw_overlays(int state)   { call(fn_draw_overlays, "SongSelect:draw_overlays", state); }

void SongSelectScript::draw_top(float dan_progress) {
    if (!fn_draw_top.valid()) return;
    call(fn_draw_top, "SongSelect:draw_top", dan_progress);
}

sol::object SongSelectScript::box_to_lua(BaseBox* box) {
    sol::state& lua = *script_manager.lua;
    if (auto* song = dynamic_cast<SongBox*>(box))     return sol::make_object(lua, song);
    if (auto* folder = dynamic_cast<FolderBox*>(box)) return sol::make_object(lua, folder);
    if (auto* back = dynamic_cast<BackBox*>(box))     return sol::make_object(lua, back);
    return sol::make_object(lua, box);
}

bool SongSelectScript::draw_box(BaseBox* box) {
    if (!fn_draw_box.valid()) return false;
    call(fn_draw_box, "SongSelect:draw_box", box_to_lua(box));
    return true;
}

bool SongSelectScript::draw_box_bg(BaseBox* box) {
    if (!fn_draw_box_bg.valid()) return false;
    call(fn_draw_box_bg, "SongSelect:draw_box_bg", box_to_lua(box));
    return true;
}

bool SongSelectScript::draw_background(Navigator* nav) {
    if (!fn_draw_background.valid()) return false;
    call(fn_draw_background, "SongSelect:draw_background", nav);
    return true;
}

bool SongSelectScript::draw_selector(SongSelectPlayer* player, bool is_half, float fade_in, int pass) {
    if (!fn_draw_selector.valid()) return false;
    call(fn_draw_selector, "SongSelect:draw_selector", player, is_half, fade_in, pass);
    return true;
}

bool SongSelectScript::draw_option_panel(SongSelectPlayer* player, int kind) {
    if (!fn_draw_option_panel.valid()) return false;
    call(fn_draw_option_panel, "SongSelect:draw_option_panel", player, kind);
    return true;
}

bool SongSelectScript::draw_sort_window(DiffSortSelect* window) {
    if (!fn_draw_sort_window.valid()) return false;
    call(fn_draw_sort_window, "SongSelect:draw_sort_window", window);
    return true;
}
