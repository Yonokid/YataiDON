#pragma once

#include "../../libs/file/script.h"

class BattleOverlay : public LuaScript {
    sol::protected_function fn_reset;
    sol::protected_function fn_pop_enemy;
    sol::protected_function fn_boss;
    sol::protected_function fn_hit;
    sol::protected_function fn_roll_hit;
    sol::protected_function fn_kill;
    sol::protected_function fn_start_skill;
    sol::protected_function fn_drop_chest;
    sol::protected_function fn_song_end;
    sol::protected_function fn_set_status;
    sol::protected_function fn_draw_back;
    sol::protected_function fn_draw_cutin;
    sol::protected_function fn_draw_front;

public:
    BattleOverlay();
    bool loaded() const { return lua_object.valid(); }
    void reset(float judge_x, float judge_y, int stage);
    void pop_enemy(double ms, int kind);
    void boss(double ms, int hp, int hp_max);
    void hit(double ms, int damage, bool good, bool critical, bool kat, bool big);
    void roll_hit(double ms, int damage);
    void kill(double ms, bool skill);
    void start_skill(double ms, int skill);
    void drop_chest(double ms);
    void song_end(double ms, bool full_combo);
    void set_status(int battle_power, float skill_fill, int boss_hp);
    void draw_back(double ms);
    void draw_cutin(double ms);
    void draw_front(double ms);
};
