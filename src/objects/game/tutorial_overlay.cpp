#include "tutorial_overlay.h"

TutorialOverlay::TutorialOverlay() {
    if (!load("TutorialOverlay", "tutorial_overlay")) {
        return;
    }
    fn_update = lua_object["update"];
    fn_hit = lua_object["hit"];
    fn_gauge = lua_object["gauge"];
    fn_finished = lua_object["is_finished"];
    fn_draw_back = lua_object["draw_back"];
    fn_draw = lua_object["draw"];
}

void TutorialOverlay::update(double chart_ms) {
    call(fn_update, "TutorialOverlay:update", chart_ms);
}

void TutorialOverlay::hit(bool kat, bool right, double chart_ms) {
    call(fn_hit, "TutorialOverlay:hit", kat ? "kat" : "don", right ? "r" : "l", chart_ms);
}

std::optional<double> TutorialOverlay::gauge(double chart_ms) {
    auto v = call_r<double>(fn_gauge, "TutorialOverlay:gauge", chart_ms);
    return v ? std::optional<double>(*v) : std::nullopt;
}

bool TutorialOverlay::is_finished(double chart_ms) {
    return call_r<bool>(fn_finished, "TutorialOverlay:is_finished", chart_ms).value_or(false);
}

void TutorialOverlay::draw_back(double chart_ms) {
    call(fn_draw_back, "TutorialOverlay:draw_back", chart_ms);
}

void TutorialOverlay::draw(double chart_ms, int rim_count) {
    call(fn_draw, "TutorialOverlay:draw", chart_ms, rim_count);
}
