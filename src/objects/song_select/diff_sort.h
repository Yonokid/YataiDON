#pragma once

#include "file_navigator/navigator.h"

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

public:
    DiffSortSelect(Statistics statistics, int prev_diff, int prev_level);

    void update(double current_ms);
    std::optional<std::pair<int, int>> input_select();
    void input_left();
    void input_right();
    void draw();
};
