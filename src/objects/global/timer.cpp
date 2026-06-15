#include "timer.h"
#include "../../libs/global_data.h"

Timer::Timer(int time, double current_time_ms, std::function<void()> confirm_func) {
    bool is_frozen = global_data.config->general.timer_frozen;
    if (!load("Timer", "timer", time, current_time_ms, confirm_func, is_frozen)) return;
    fn_update = lua_object["update"];
    fn_draw   = lua_object["draw"];
}

void Timer::update(double current_ms) { call(fn_update, "Timer:update", current_ms); }
void Timer::draw(float x, float y)    { call(fn_draw,   "Timer:draw", x, y); }
