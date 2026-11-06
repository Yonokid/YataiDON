#include "ai_reward_flow.h"

AIRewardFlow::AIRewardFlow(const AIBattleProgress& p) {
    if (!script_manager.lua || !script_manager.has_lua_script("ai_reward_flow")) {
        return;
    }
    sol::table data = script_manager.lua->create_table_with(
        "rank_before", p.rank_before, "rank_after", p.rank_after, "tokens_after", p.tokens_after,
        "next_rank_after_tokens", p.next_rank_after_tokens, "wins_after", p.wins_after, "next_reward_wins",
        p.next_reward_wins, "next_reward_name", p.next_reward_name, "reward_kind", p.reward_kind, "reward_name",
        p.reward_name, "all_rewards", p.all_rewards);
    if (!load("AIRewardFlow", "ai_reward_flow", data)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_input = lua_object["input"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
}

void AIRewardFlow::update(double current_ms) {
    call(fn_update, "AIRewardFlow:update", current_ms);
}

bool AIRewardFlow::input(Action action) {
    return call_r<bool>(fn_input, "AIRewardFlow:input", static_cast<int>(action)).value_or(false);
}

void AIRewardFlow::draw() {
    call(fn_draw, "AIRewardFlow:draw");
}

bool AIRewardFlow::is_done() {
    return call_r<bool>(fn_is_done, "AIRewardFlow:is_done").value_or(true);
}
