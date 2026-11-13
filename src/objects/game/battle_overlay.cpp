#include "battle_overlay.h"

BattleOverlay::BattleOverlay() {
    if (!load("EnsoBattleOverlay", "enso_battle")) {
        return;
    }
    fn_reset = lua_object["reset"];
    fn_pop_enemy = lua_object["pop_enemy"];
    fn_boss = lua_object["boss"];
    fn_hit = lua_object["hit"];
    fn_roll_hit = lua_object["roll_hit"];
    fn_kill = lua_object["kill"];
    fn_start_skill = lua_object["start_skill"];
    fn_drop_chest = lua_object["drop_chest"];
    fn_song_end = lua_object["song_end"];
    fn_set_status = lua_object["set_status"];
    fn_draw_back = lua_object["draw_back"];
    fn_draw_cutin = lua_object["draw_cutin"];
    fn_draw_front = lua_object["draw_front"];
}

void BattleOverlay::reset(float judge_x, float judge_y) {
    call(fn_reset, "EnsoBattleOverlay:reset", judge_x, judge_y);
}

void BattleOverlay::pop_enemy(double ms, int kind) {
    call(fn_pop_enemy, "EnsoBattleOverlay:pop_enemy", ms, kind);
}

void BattleOverlay::boss(double ms, int hp, int hp_max) {
    call(fn_boss, "EnsoBattleOverlay:boss", ms, hp, hp_max);
}

void BattleOverlay::hit(double ms, int damage, bool good, bool critical, bool kat, bool big) {
    call(fn_hit, "EnsoBattleOverlay:hit", ms, damage, good, critical, kat, big);
}

void BattleOverlay::roll_hit(double ms, int damage) {
    call(fn_roll_hit, "EnsoBattleOverlay:roll_hit", ms, damage);
}

void BattleOverlay::kill(double ms, bool skill) {
    call(fn_kill, "EnsoBattleOverlay:kill", ms, skill);
}

void BattleOverlay::start_skill(double ms, int skill) {
    call(fn_start_skill, "EnsoBattleOverlay:start_skill", ms, skill);
}

void BattleOverlay::drop_chest(double ms) {
    call(fn_drop_chest, "EnsoBattleOverlay:drop_chest", ms);
}

void BattleOverlay::song_end(double ms) {
    call(fn_song_end, "EnsoBattleOverlay:song_end", ms);
}

void BattleOverlay::set_status(int battle_power, float skill_fill, int boss_hp) {
    call(fn_set_status, "EnsoBattleOverlay:set_status", battle_power, skill_fill, boss_hp);
}

void BattleOverlay::draw_back(double ms) {
    call(fn_draw_back, "EnsoBattleOverlay:draw_back", ms);
}

void BattleOverlay::draw_cutin(double ms) {
    call(fn_draw_cutin, "EnsoBattleOverlay:draw_cutin", ms);
}

void BattleOverlay::draw_front(double ms) {
    call(fn_draw_front, "EnsoBattleOverlay:draw_front", ms);
}
