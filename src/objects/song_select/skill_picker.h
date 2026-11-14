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

    int number(int index) const;

public:
    SkillPicker(int moves, int player);
    void open(double ms);
    bool handle_input(double ms);
    void draw(double ms);
};
