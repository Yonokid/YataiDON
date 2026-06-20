#include "high_score_indicator.h"

HighScoreIndicator::HighScoreIndicator(int old_score, int new_score, bool is_2p) {
    int score_diff = new_score - old_score;
    if (!load("HighScoreIndicator", "high_score_indicator", score_diff, is_2p)) return;
    fn_update = lua_object["update"];
    fn_draw   = lua_object["draw"];
}

void HighScoreIndicator::update(double current_ms) {
    call(fn_update, "HighScoreIndicator:update", current_ms);
}

void HighScoreIndicator::draw() {
    call(fn_draw, "HighScoreIndicator:draw");
}
