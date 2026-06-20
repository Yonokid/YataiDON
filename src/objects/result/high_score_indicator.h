#pragma once

#include "../../libs/script.h"

class HighScoreIndicator : public LuaScript {
    sol::protected_function fn_update, fn_draw;
public:
    HighScoreIndicator() = default;
    HighScoreIndicator(int old_score, int new_score, bool is_2p);
    void update(double current_ms);
    void draw();
};
