#pragma once

#include <vector>

#include "../../libs/file/script.h"

class SkillPicker : public LuaScript {
    sol::protected_function fn_draw, fn_open, fn_move, fn_decide;
    std::vector<int> items;
    std::vector<int> cursor;
    int slot = 0;
    int moves = 1;
    bool choosing = false;
    double closed_ms = -1e9;

    int number(int index) const;

public:
    SkillPicker(int moves, bool hacchake, int player);
    void open(double ms);
    bool handle_input(double ms);
    bool is_shown(double ms) const { return choosing || ms < closed_ms + 20 * 1000.0 / 60.0; }
    void draw(double ms);
};
