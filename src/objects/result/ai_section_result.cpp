#include "ai_section_result.h"

AISectionBoard::AISectionBoard(const AIMatchResult& match) {
    if (!script_manager.lua) {
        return;
    }
    sol::state& lua = *script_manager.lua;
    sol::table sections = lua.create_table();
    for (const AISectionTally& s : match.sections) {
        sections.add(
            lua.create_table_with("good", s.good, "ok", s.ok, "bad", s.bad, "drumroll", s.drumroll, "won", s.won));
    }
    if (!load("AISectionBoard", "ai_section_result", lua.create_table_with("sections", sections, "won", match.won))) {
        return;
    }
    fn_begin = lua_object["begin"];
    fn_update = lua_object["update"];
    fn_decide = lua_object["decide"];
    fn_toggle = lua_object["toggle"];
    fn_draw_background = lua_object["draw_background"];
    fn_draw = lua_object["draw"];
    fn_is_done = lua_object["is_done"];
}

void AISectionBoard::begin(const AIBattleProgress& progress) {
    sol::state& lua = *script_manager.lua;
    sol::table bests = lua.create_table();
    for (const AIBattleBest& b : progress.bests) {
        bests.add(lua.create_table_with("good", b.good, "ok", b.ok, "bad", b.bad, "drumroll", b.drumroll));
    }
    sol::object answer = sol::lua_nil;
    if (progress.answered) {
        answer = lua.create_table_with(
            "rank_before", progress.rank_before, "rank_after", progress.rank_after, "tokens_before",
            progress.tokens_before, "tokens_after", progress.tokens_after, "next_rank_tokens",
            progress.next_rank_tokens, "wins_before", progress.wins_before, "wins_after", progress.wins_after,
            "fulfilled", progress.fulfilled, "first", progress.first, "challenge", progress.challenge, "bests", bests);
    }
    call(fn_begin, "AISectionBoard:begin", answer);
}

void AISectionBoard::update(double current_ms) {
    call(fn_update, "AISectionBoard:update", current_ms);
}

bool AISectionBoard::decide() {
    return call_r<bool>(fn_decide, "AISectionBoard:decide").value_or(false);
}

bool AISectionBoard::toggle() {
    return call_r<bool>(fn_toggle, "AISectionBoard:toggle").value_or(false);
}

void AISectionBoard::draw_background() {
    call(fn_draw_background, "AISectionBoard:draw_background");
}

void AISectionBoard::draw() {
    call(fn_draw, "AISectionBoard:draw");
}

bool AISectionBoard::is_done() {
    return call_r<bool>(fn_is_done, "AISectionBoard:is_done").value_or(true);
}
