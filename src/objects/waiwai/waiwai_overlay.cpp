#include "waiwai_overlay.h"

WaiwaiOverlay::WaiwaiOverlay(const std::string& class_name, const std::string& script_name) {
    if (!load(class_name, script_name)) {
        return;
    }
    fn_update = lua_object["update"];
    fn_draw = lua_object["draw"];
    fn_draw_front = lua_object["draw_front"];
    fn_set_result = lua_object["set_result"];
    fn_set_gauge = lua_object["set_gauge"];
    fn_finished = lua_object["is_finished"];
}

void WaiwaiOverlay::update(double ms, int state) {
    call(fn_update, "WaiwaiOverlay:update", ms, state);
}

void WaiwaiOverlay::draw(double ms) {
    call(fn_draw, "WaiwaiOverlay:draw", ms);
}

void WaiwaiOverlay::draw_front(double ms) {
    call(fn_draw_front, "WaiwaiOverlay:draw_front", ms);
}

void WaiwaiOverlay::set_result(int gauge_point, int duet_percent, int message, bool revive) {
    call(fn_set_result, "WaiwaiOverlay:set_result", gauge_point, duet_percent, message, revive);
}

void WaiwaiOverlay::set_gauge(int point) {
    call(fn_set_gauge, "WaiwaiOverlay:set_gauge", point);
}

int WaiwaiOverlay::motion() const {
    sol::optional<int> value = lua_object["motion"];
    return value.value_or(-1);
}

bool WaiwaiOverlay::is_finished() {
    return call_r<bool>(fn_finished, "WaiwaiOverlay:is_finished").value_or(false);
}
