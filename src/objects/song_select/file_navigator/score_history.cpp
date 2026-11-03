#include "score_history.h"

#include <algorithm>

#include "../../../libs/network/network.h"

ScoreHistory::ScoreHistory(const std::array<std::optional<Score>, 5>& scores, const std::array<std::string, 5>& hashes,
                           double current_ms) {
    if (!script_manager.lua) {
        return;
    }

    sol::state& lua = *script_manager.lua;
    sol::table available = lua.create_table();
    int n = 1;
    for (int i = 0; i < 5; i++) {
        if (!scores[i].has_value()) {
            continue;
        }
        const Score& s = *scores[i];
        sol::table entry = lua.create_table();
        entry["diff"] = i;
        entry["score"] = s.score;
        entry["good"] = s.good;
        entry["ok"] = s.ok;
        entry["bad"] = s.bad;
        entry["drumroll"] = s.drumroll;
        available[n++] = entry;
    }

    bool is_shinuchi = global_data.config->general.score_method == ScoreMethod::SHINUCHI;

    for (int i : {4, 3, 2, 1, 0}) {
        if (scores[i].has_value() && !hashes[i].empty()) {
            rank_hash = hashes[i];
            rank_diff = i;
            break;
        }
    }
    for (int i : {3, 2, 1, 0, 4}) {
        if (rank_hash.empty() && !hashes[i].empty()) {
            rank_hash = hashes[i];
            rank_diff = i;
        }
    }

    if (!load("ScoreHistory", "score_history", available, is_shinuchi, current_ms)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_draw = lua_object["draw"];
    fn_set_ranking = lua_object["set_ranking"];
}

void ScoreHistory::update(double current_ms) {
    if (!rank_hash.empty() && fn_set_ranking.valid()) {
        if (auto board = network.song_leaderboard(rank_hash)) {
            rank_hash.clear();
            sol::state& lua = *script_manager.lua;
            sol::table entries = lua.create_table();
            for (int i = 0; i < std::min<int>(3, std::ssize(*board)); i++) {
                entries[i + 1] = lua.create_table_with("name", (*board)[i].username, "score", (*board)[i].score);
            }
            call(fn_set_ranking, "ScoreHistory:set_ranking",
                 lua.create_table_with("diff", rank_diff, "entries", entries), current_ms);
        }
    }
    call(fn_update, "ScoreHistory:update", current_ms);
}
void ScoreHistory::draw() {
    call(fn_draw, "ScoreHistory:draw");
}
