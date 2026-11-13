#include "battle_select_info.h"

BattleSelectInfo::BattleSelectInfo(const BattleStatus& status, int stage, int player) {
    if (!script_manager.lua) {
        return;
    }
    const auto boss = status.bosses.find(stage);
    const bool has_boss = boss != status.bosses.end();
    sol::table data = script_manager.lua->create_table_with(
        "player", player, "bond", status.bond, "exp", status.exp, "exp_next", status.exp_next, "best_power",
        status.best_power, "stones", status.stones, "move_cost", status.move_cost, "stage", stage);
    data["boss_hp"] = has_boss ? boss->second.hp : 0;
    data["boss_hp_max"] = has_boss ? boss->second.max : 0;
    if (!load("BattleSelectInfo", "battle_select", data)) {
        return;
    }
    fn_draw = lua_object["draw"];
}

void BattleSelectInfo::draw(double ms) {
    call(fn_draw, "BattleSelectInfo:draw", ms);
}
