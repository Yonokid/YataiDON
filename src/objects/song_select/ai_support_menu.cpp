#include "ai_support_menu.h"

AISupportMenu::AISupportMenu() {
    if (!load("AISupportMenu", "ai_support_menu")) {
        return;
    }
    fn_draw = lua_object["draw"];
}

void AISupportMenu::set_status(const AIBattleStatus& status) {
    sol::protected_function fn = lua_object["set_status"];
    if (!status.answered || !fn.valid() || !script_manager.lua) {
        return;
    }
    call(fn, "AISupportMenu:set_status",
         script_manager.lua->create_table_with("rank", status.rank, "tokens", status.tokens, "next_rank_tokens",
                                               status.next_rank_tokens, "wins", status.wins, "next_reward_wins",
                                               status.next_reward_wins));
}

void AISupportMenu::draw(double ms, int course, int cursor, bool choosing, bool branching, int player, int tension) {
    call(fn_draw, "AISupportMenu:draw", ms, course, cursor, choosing, branching, player, tension);
}
