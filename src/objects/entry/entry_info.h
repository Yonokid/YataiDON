#pragma once

#include "../../libs/file/script.h"

class EntryInfo : public LuaScript {
public:
    EntryInfo();
    bool unavailable() const { return !fn_input.valid(); }
    void update(double current_ms);
    bool input(); // false: ignored
    void draw();
    bool is_done();

private:
    sol::protected_function fn_update;
    sol::protected_function fn_input;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_done;
};
