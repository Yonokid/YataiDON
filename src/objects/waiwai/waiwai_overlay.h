#pragma once

#include <string>

#include "../../libs/file/script.h"

class WaiwaiOverlay : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_draw;
    sol::protected_function fn_draw_front;
    sol::protected_function fn_set_result;
    sol::protected_function fn_set_gauge;
    sol::protected_function fn_finished;

public:
    WaiwaiOverlay(const std::string& class_name, const std::string& script_name);
    bool loaded() const { return lua_object.valid(); }
    void update(double ms, int state);
    void draw(double ms);
    void draw_front(double ms);
    void set_result(int gauge_point, int duet_percent, int message, bool revive);
    void set_gauge(int point);
    int motion() const;
    bool is_finished();
};
