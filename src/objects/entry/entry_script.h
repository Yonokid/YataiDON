#pragma once
#include <sol/sol.hpp>

class EntryScript {
private:
    sol::table lua_object;
    sol::protected_function fn_update;
    sol::protected_function fn_start_side_select;
    sol::protected_function fn_restart_side_select;
    sol::protected_function fn_get_side_select_fade;
    sol::protected_function fn_draw_background;
    sol::protected_function fn_draw_side_select;
    sol::protected_function fn_draw_footer;
    sol::protected_function fn_draw_player_entry;

public:
    EntryScript();
    void update(double current_ms);
    void start_side_select();
    void restart_side_select();
    float get_side_select_fade();
    void draw_background();
    void draw_side_select(int side);
    void draw_footer(bool p1_joined, bool p2_joined);
    void draw_player_entry();
};
