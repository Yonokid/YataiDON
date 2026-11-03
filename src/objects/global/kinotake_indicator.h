#pragma once

#include <string>

#include "../../libs/file/script.h"

class KinotakeIndicator : public LuaScript {
    sol::protected_function fn_set;
    sol::protected_function fn_update;
    sol::protected_function fn_draw;

public:
    KinotakeIndicator() = default;
    explicit KinotakeIndicator(const std::string& mode);
    void set(int leader, int assist);
    void update(double current_ms);
    void draw(float fade = 1.0f);

    bool refresh();
};
