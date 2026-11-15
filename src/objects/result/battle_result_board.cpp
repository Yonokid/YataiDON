#include "battle_result_board.h"

#include "../../libs/file/scores.h"

BattleResultBoard::BattleResultBoard(const BattleTally& song, int song_number, int bond) {
    if (!script_manager.lua) {
        return;
    }
    sol::state& lua = *script_manager.lua;
    sol::table data = lua.create_table_with("stage", song.stage, "difficulty", song.difficulty, "boss", song.boss,
                                            "boss_hp", song.boss_hp, "boss_hp_max", song.boss_hp_max, "kills",
                                            song.kills, "damage", song.damage, "power", song.power);
    data["orb_chests"] = song.orb_chests;
    data["player"] = global_data.player_num == PlayerNum::P2 ? 2 : 1;
    data["song_number"] = song_number;
    data["bond"] = bond;
    if (!load("BattleResultBoard", "battle_result", data)) {
        return;
    }
    fn_begin = lua_object["begin"];
    fn_update = lua_object["update"];
    fn_decide = lua_object["decide"];
    fn_draw_background = lua_object["draw_background"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
    fn_nameplate_pos = lua_object["nameplate_pos"];
    const auto pd = scores_manager.get_player_data(get_player_id(global_data.player_num));
    nameplate = Nameplate(pd ? pd->username : "", pd ? pd->title : "", global_data.player_num, pd ? pd->dan : -1,
                          pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
}

static sol::table status_table(sol::state& lua, const BattleStatus& s) {
    return lua.create_table_with("bond", s.bond, "exp", s.exp, "exp_next", s.exp_next, "best_power", s.best_power,
                                 "stones", s.stones, "moves", s.moves, "move_cost", s.move_cost, "orbs", s.orbs,
                                 "orb_rewards", s.orb_rewards, "orb_cost", s.orb_cost, "stage", s.stage);
}

void BattleResultBoard::begin(const BattleProgress& progress) {
    sol::state& lua = *script_manager.lua;
    sol::object answer = sol::lua_nil;
    if (progress.answered) {
        sol::table t = lua.create_table_with(
            "before", status_table(lua, progress.before), "after", status_table(lua, progress.after), "chests",
            sol::as_table(progress.chests), "orb_chests", sol::as_table(progress.orb_chests));
        if (progress.boss_before) {
            t["boss"] = lua.create_table_with("hp_before", progress.boss_before->hp, "hp_after", progress.boss_hp_after,
                                              "max", progress.boss_before->max);
        }
        if (!progress.reward_kind.empty()) {
            t["reward"] = lua.create_table_with("kind", progress.reward_kind, "name", progress.reward_name);
        }
        if (!progress.orb_reward_kind.empty()) {
            t["orb_reward"] = lua.create_table_with("kind", progress.orb_reward_kind, "name", progress.orb_reward_name);
        }
        answer = t;
    }
    call(fn_begin, "BattleResultBoard:begin", answer);
}

void BattleResultBoard::update(double current_ms) {
    call(fn_update, "BattleResultBoard:update", current_ms);
    nameplate.update(current_ms);
}

bool BattleResultBoard::decide() {
    return call_r<bool>(fn_decide, "BattleResultBoard:decide").value_or(false);
}

void BattleResultBoard::draw_background() {
    call(fn_draw_background, "BattleResultBoard:draw_background");
}

void BattleResultBoard::draw() {
    call(fn_draw, "BattleResultBoard:draw");
    const auto pos = call_r<sol::table>(fn_nameplate_pos, "BattleResultBoard:nameplate_pos");
    if (!pos) {
        return;
    }
    const sol::optional<float> x = (*pos)[1], y = (*pos)[2], fade = (*pos)[3];
    if (x && y) {
        nameplate.draw(*x, *y, fade.value_or(1.0f));
    }
}

bool BattleResultBoard::is_done() {
    return call_r<bool>(fn_is_done, "BattleResultBoard:is_done").value_or(true);
}
