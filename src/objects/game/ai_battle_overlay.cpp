#include "ai_battle_overlay.h"

AIBattleOverlay::AIBattleOverlay() {
    if (!load("AIBattleOverlay", "ai_battle")) {
        return;
    }
    fn_reset = lua_object["reset"];
    fn_set_section_result = lua_object["set_section_result"];
    fn_draw_back = lua_object["draw_back"];
    fn_draw_front = lua_object["draw_front"];
    fn_draw_scoreboard = lua_object["draw_scoreboard"];
}

void AIBattleOverlay::reset() {
    call(fn_reset, "AIBattleOverlay:reset");
}

void AIBattleOverlay::set_section_result(int section, bool won) {
    call(fn_set_section_result, "AIBattleOverlay:set_section_result", section, won);
}

void AIBattleOverlay::draw_back(double ms, int territory) {
    call(fn_draw_back, "AIBattleOverlay:draw_back", ms, territory);
}

void AIBattleOverlay::draw_front(double ms, int section, int section_count, float progress) {
    call(fn_draw_front, "AIBattleOverlay:draw_front", ms, section, section_count, progress);
}

void AIBattleOverlay::draw_scoreboard(bool is_ai, int good, int ok, int bad, int drumroll) {
    call(fn_draw_scoreboard, "AIBattleOverlay:draw_scoreboard", is_ai, good, ok, bad, drumroll);
}
