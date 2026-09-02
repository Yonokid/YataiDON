#pragma once
#include "../../libs/script.h"

class BaseBox;
class SongSelectPlayer;
class Navigator;
class DiffSortSelect;

class SongSelectScript : public LuaScript {
private:
    sol::protected_function fn_update;
    sol::protected_function fn_restart_text_fade;
    sol::protected_function fn_draw_footer;
    sol::protected_function fn_draw_overlays;
    sol::protected_function fn_draw_top;
    sol::protected_function fn_draw_box;
    sol::protected_function fn_draw_box_bg;
    sol::protected_function fn_draw_background;
    sol::protected_function fn_draw_selector;
    sol::protected_function fn_draw_option_panel;
    sol::protected_function fn_draw_sort_window;

    sol::object box_to_lua(BaseBox* box);

public:
    SongSelectScript();
    void update(double current_ms);
    void restart_text_fade();
    void draw_footer();
    void draw_overlays(int state);
    void draw_top(float dan_progress);

    bool draw_box(BaseBox* box);
    bool draw_box_bg(BaseBox* box);
    bool has_box_bg() const { return fn_draw_box_bg.valid(); }
    bool draw_background(Navigator* nav);
    bool draw_selector(SongSelectPlayer* player, bool is_half, float fade_in, int pass);
    bool draw_option_panel(SongSelectPlayer* player, int kind);

    bool has_sort_window() const { return fn_draw_sort_window.valid(); }
    bool draw_sort_window(DiffSortSelect* window);
};
