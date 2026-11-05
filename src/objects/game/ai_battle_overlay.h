#pragma once

#include "../../libs/file/script.h"

class AIBattleOverlay : public LuaScript {
    sol::protected_function fn_reset;
    sol::protected_function fn_set_section_result;
    sol::protected_function fn_draw_back;
    sol::protected_function fn_draw_front;
    sol::protected_function fn_draw_scoreboard;

public:
    AIBattleOverlay();
    bool loaded() const { return lua_object.valid(); }
    void reset();
    void set_section_result(int section, bool won);
    void draw_back(double ms, int territory);
    void draw_front(double ms, int section, int section_count, float progress);
    void draw_scoreboard(bool is_ai, int good, int ok, int bad, int drumroll);
};
