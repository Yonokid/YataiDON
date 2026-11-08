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
    fn_draw_clash = lua_object["draw_clash"];
    fn_draw_finale = lua_object["draw_finale"];
    fn_set_match_result = lua_object["set_match_result"];
    fn_border_x = lua_object["border_x"];
    fn_handle_skip = lua_object["handle_skip"];
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

void AIBattleOverlay::draw_clash(double ms, int territory, bool angry) {
    call(fn_draw_clash, "AIBattleOverlay:draw_clash", ms, territory, angry);
}

void AIBattleOverlay::draw_finale(double ms) {
    call(fn_draw_finale, "AIBattleOverlay:draw_finale", ms);
}

void AIBattleOverlay::set_match_result(bool won, bool full_combo) {
    call(fn_set_match_result, "AIBattleOverlay:set_match_result", won, full_combo);
}

void AIBattleOverlay::handle_skip(int count, int total, bool skipped) {
    call(fn_handle_skip, "AIBattleOverlay:handle_skip", count, total, skipped);
}

float AIBattleOverlay::border_x(int territory) {
    return call_r<float>(fn_border_x, "AIBattleOverlay:border_x", territory).value_or(640.0f);
}
