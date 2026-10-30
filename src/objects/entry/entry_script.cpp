#include "entry_script.h"

EntryScript::EntryScript() {
    if (!load("Entry", "entry")) {
        return;
    }
    fn_update = lua_object["update"];
    fn_start_side_select = lua_object["start_side_select"];
    fn_restart_side_select = lua_object["restart_side_select"];
    fn_get_side_select_fade = lua_object["get_side_select_fade"];
    fn_draw_background = lua_object["draw_background"];
    fn_draw_side_select = lua_object["draw_side_select"];
    fn_draw_side_select_buttons = lua_object["draw_side_select_buttons"];
    fn_decide_side_select = lua_object["decide_side_select"];
    fn_draw_footer = lua_object["draw_footer"];
    fn_draw_waiting = lua_object["draw_waiting"];
    fn_wait_chara_pos = lua_object["wait_chara_pos"];
    fn_draw_player_entry = lua_object["draw_player_entry"];
}

void EntryScript::update(double current_ms) {
    call(fn_update, "Entry:update", current_ms);
}
void EntryScript::start_side_select() {
    call(fn_start_side_select, "Entry:start_side_select");
}
void EntryScript::restart_side_select() {
    call(fn_restart_side_select, "Entry:restart_side_select");
}
void EntryScript::draw_background() {
    call(fn_draw_background, "Entry:draw_background");
}
void EntryScript::draw_side_select() {
    call(fn_draw_side_select, "Entry:draw_side_select");
}
void EntryScript::draw_side_select_buttons(int side) {
    call(fn_draw_side_select_buttons, "Entry:draw_side_select_buttons", side);
}
void EntryScript::decide_side_select(int side) {
    call(fn_decide_side_select, "Entry:decide_side_select", side);
}
void EntryScript::draw_footer(bool p1_joined, bool p2_joined) {
    call(fn_draw_footer, "Entry:draw_footer", p1_joined, p2_joined);
}
void EntryScript::draw_waiting(double idle_ms) {
    call(fn_draw_waiting, "Entry:draw_waiting", idle_ms);
}
void EntryScript::draw_player_entry() {
    call(fn_draw_player_entry, "Entry:draw_player_entry");
}

std::optional<std::pair<float, float>> EntryScript::wait_chara_pos(bool p2) {
    auto pos = call_r<sol::table>(fn_wait_chara_pos, "Entry:wait_chara_pos", p2);
    if (!pos) {
        return std::nullopt;
    }
    sol::optional<float> x = (*pos)[1];
    sol::optional<float> y = (*pos)[2];
    if (!x || !y) {
        return std::nullopt;
    }
    return std::make_pair(*x, *y);
}

float EntryScript::get_side_select_fade() {
    return call_r<float>(fn_get_side_select_fade, "Entry:get_side_select_fade").value_or(1.0f);
}
