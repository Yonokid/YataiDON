#pragma once

#include "../../libs/file/script.h"
#include "../../libs/global_data.h"

// The score on the lane cover plus the points that pop up as they are earned. The skin's
// ScoreCounter (game/score_counter.lua) owns the animations and the drawing.
class ScoreCounter : public LuaScript {
private:
    sol::protected_function fn_update, fn_add_popup, fn_draw, fn_set_best;

public:
    ScoreCounter(bool is_2p, PlayerNum player_num) {
        if (!load("ScoreCounter", "score_counter", is_2p, static_cast<int>(player_num))) {
            return;
        }
        fn_update = lua_object["update"];
        fn_add_popup = lua_object["add_popup"];
        fn_draw = lua_object["draw"];
        fn_set_best = lua_object["set_best"];
    }

    // `limited`: skip the popup if too many are already showing
    void add_popup(int value, bool limited = false) { call(fn_add_popup, "ScoreCounter:add_popup", value, limited); }
    void update(double current_ms, int score) { call(fn_update, "ScoreCounter:update", current_ms, score); }
    // the player's best score on this chart so far; the skin shows a popup when the run passes it
    void set_best(int score) {
        if (fn_set_best.valid()) {
            call(fn_set_best, "ScoreCounter:set_best", score);
        }
    }
    void draw(float y) { call(fn_draw, "ScoreCounter:draw", y); }
};
