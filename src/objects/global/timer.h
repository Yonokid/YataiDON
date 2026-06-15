#pragma once

#include "../../libs/script.h"
#include <functional>

class Timer : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_draw;
public:
    Timer(int time, double current_time_ms, std::function<void()> confirm_func);
    void update(double current_ms);
    void draw(float x = 0, float y = 0);
};
