#pragma once

#include <optional>

#include "../../libs/file/script.h"

class TutorialOverlay : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_hit;
    sol::protected_function fn_gauge;
    sol::protected_function fn_finished;
    sol::protected_function fn_draw_back;
    sol::protected_function fn_draw;

public:
    TutorialOverlay();
    bool loaded() const { return lua_object.valid(); }
    void update(double chart_ms);
    void hit(bool kat, bool right, double chart_ms);
    std::optional<double> gauge(double chart_ms);
    bool is_finished(double chart_ms);
    void draw_back(double chart_ms);
    void draw(double chart_ms, int rim_count);
};
