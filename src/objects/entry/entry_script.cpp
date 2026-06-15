#include "entry_script.h"

EntryScript::EntryScript() {
    if (!load("Entry", "entry")) return;
    fn_update                = lua_object["update"];
    fn_start_side_select     = lua_object["start_side_select"];
    fn_restart_side_select   = lua_object["restart_side_select"];
    fn_get_side_select_fade  = lua_object["get_side_select_fade"];
    fn_draw_background       = lua_object["draw_background"];
    fn_draw_side_select      = lua_object["draw_side_select"];
    fn_draw_side_select_buttons = lua_object["draw_side_select_buttons"];
    fn_draw_footer           = lua_object["draw_footer"];
    fn_draw_player_entry     = lua_object["draw_player_entry"];
}

void EntryScript::update(double current_ms)              { call(fn_update, "Entry:update", current_ms); }
void EntryScript::start_side_select()                    { call(fn_start_side_select, "Entry:start_side_select"); }
void EntryScript::restart_side_select()                  { call(fn_restart_side_select, "Entry:restart_side_select"); }
void EntryScript::draw_background()                      { call(fn_draw_background, "Entry:draw_background"); }
void EntryScript::draw_side_select()                     { call(fn_draw_side_select, "Entry:draw_side_select"); }
void EntryScript::draw_side_select_buttons(int side)     { call(fn_draw_side_select_buttons, "Entry:draw_side_select_buttons", side); }
void EntryScript::draw_footer(bool p1_joined, bool p2_joined) { call(fn_draw_footer, "Entry:draw_footer", p1_joined, p2_joined); }
void EntryScript::draw_player_entry()                    { call(fn_draw_player_entry, "Entry:draw_player_entry"); }

float EntryScript::get_side_select_fade() {
    return call_r<float>(fn_get_side_select_fade, "Entry:get_side_select_fade").value_or(1.0f);
}
