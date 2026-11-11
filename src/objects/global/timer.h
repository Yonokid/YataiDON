#pragma once

#include <functional>

#include "../../libs/file/script.h"

class Timer : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_draw;

public:
    Timer(int time, double current_time_ms, const std::function<void()>& confirm_func);
    void update(double current_ms);
    void draw(float x = 0, float y = 0);
    void hold(double current_ms); // call instead of update() while paused, so resuming does not skip a second

    int time() const;
};
