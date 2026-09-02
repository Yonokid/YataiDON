#pragma once

#include "file_navigator/navigator.h"

class SongSelectScript;

class DiffSortSelect {
private:
    int selected_box;
    int selected_level;
    bool in_level_select;
    bool confirmation;
    int confirm_index;
    int num_boxes;
    int prev_diff;
    int prev_level;

    std::vector<int> limits;

    Statistics statistics;
    std::map<int, std::array<int, 3>> diff_sort_sum_stat;

    TextureResizeAnimation* bg_resize;
    FadeAnimation* diff_fade_in;
    FadeAnimation* box_flicker;
    MoveAnimation* bounce_up_1;
    MoveAnimation* bounce_down_1;
    MoveAnimation* bounce_up_2;
    MoveAnimation* bounce_down_2;
    FadeAnimation* blue_arrow_fade;
    MoveAnimation* blue_arrow_move;

    void draw_statistics();
    void draw_diff_select();
    void draw_level_select();

    bool one_menu_sort = false;
    SongSelectScript* script = nullptr;
    int session = 0;
    int sort_param[3] = {1, 1, 1};
    int diff_count = 5;
    int star_min = 1, star_max = 10;
    int order_count = 4;
    int song_num = 0;
    double t_open = 0.0;
    double t_end  = 0.0;
    double t_out  = 0.0;
    double now_ms = 0.0;
    double arrow_t0[3] = {0.0, 0.0, 0.0};
    int    arrow_dir[3] = {0, 0, 0};     // +1 = right arrow bounced, -1 = left
    bool   finished = false;
    std::optional<std::array<int, 3>> one_menu_sort_result;

    void one_menu_sort_refresh_song_num();
    bool one_menu_sort_input_locked() const;
    void one_menu_sort_change_param(int move);

public:
    DiffSortSelect(Statistics statistics, int prev_diff, int prev_level,
                   SongSelectScript* script = nullptr, int prev_order = 1);

    void update(double current_ms);
    std::optional<std::pair<int, int>> input_select();
    void input_left();
    void input_right();
    void draw();

    std::optional<std::array<int, 3>> take_result();

    int  lua_session() const     { return session; }         // 0..2 (3 = all decided)
    int  lua_diff() const        { return sort_param[0]; }   // 1..5 (5 = oni-ura)
    int  lua_star() const        { return sort_param[1]; }   // 1..10
    int  lua_order() const       { return sort_param[2]; }   // 1..4
    int  lua_diff_count() const  { return diff_count; }
    int  lua_order_count() const { return order_count; }
    int  lua_song_num() const    { return song_num; }
    // 0 = fading in, 1 = open, 2 = the 2 s hold after the last row, 3 = fading out
    int   lua_phase() const;
    float lua_alpha() const;
    float lua_arrow_offset(int row) const;
};
