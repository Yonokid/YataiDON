#include "kinotake_result.h"

KinotakeResult::KinotakeResult(const KinotakeTally& tally) {
    if (!script_manager.lua) {
        return;
    }
    sol::state& lua = *script_manager.lua;
    sol::table players = lua.create_table();
    int64_t own[2] = {0, 0};
    for (int i = 0; i < 2; i++) {
        const KinotakeSeat& s = tally.seat[i];
        if (s.plays == 0 || s.team < 0) {
            continue;
        }
        own[s.team] += s.total;
        players.add(lua.create_table_with("seat", i + 1, "team", s.team, "base", s.base, "total", s.total, "mine",
                                          s.mine, "assist", s.assist, "duet", s.duet));
    }
    sol::table data =
        lua.create_table_with("players", players, "before",
                              lua.create_table_with("kinoko", std::max<int64_t>(0, tally.kinoko - own[0]), "takenoko",
                                                    std::max<int64_t>(0, tally.takenoko - own[1])),
                              "after", lua.create_table_with("kinoko", tally.kinoko, "takenoko", tally.takenoko));
    if (!load("KinotakeResult", "kinotake_result", data)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_skip = lua_object["skip"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
    fn_nameplate = lua_object["nameplate"];
}

void KinotakeResult::update(double current_ms) {
    call(fn_update, "KinotakeResult:update", current_ms);
}
bool KinotakeResult::skip() {
    return call_r<bool>(fn_skip, "KinotakeResult:skip").value_or(false);
}
void KinotakeResult::draw() {
    call(fn_draw, "KinotakeResult:draw");
}
bool KinotakeResult::is_done() {
    return call_r<bool>(fn_is_done, "KinotakeResult:is_done").value_or(true);
}
std::pair<float, float> KinotakeResult::nameplate(int seat) {
    if (!fn_nameplate.valid()) {
        return {0.0f, 0.0f};
    }
    sol::protected_function_result r = fn_nameplate(lua_object, seat + 1);
    if (!r.valid() || r.return_count() < 2) {
        return {0.0f, 0.0f};
    }
    return {r.get<float>(0), r.get<float>(1)};
}
