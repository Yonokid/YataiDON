#include "song_select_script.h"

SongSelectScript::SongSelectScript() {
    if (!load("SongSelect", "song_select")) return;
    fn_update            = lua_object["update"];
    fn_restart_text_fade = lua_object["restart_text_fade"];
    fn_draw_footer       = lua_object["draw_footer"];
    fn_draw_overlays     = lua_object["draw_overlays"];
}

void SongSelectScript::update(double current_ms) { call(fn_update, "SongSelect:update", current_ms); }
void SongSelectScript::restart_text_fade()        { call(fn_restart_text_fade, "SongSelect:restart_text_fade"); }
void SongSelectScript::draw_footer()              { call(fn_draw_footer, "SongSelect:draw_footer"); }
void SongSelectScript::draw_overlays(int state)   { call(fn_draw_overlays, "SongSelect:draw_overlays", state); }
