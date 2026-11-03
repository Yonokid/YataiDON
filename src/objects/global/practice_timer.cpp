#include "practice_timer.h"

#include <algorithm>
#include <cmath>

#include "../../libs/global_data.h"

PracticeTimer::PracticeTimer(int limit_seconds, double current_ms)
    : remaining_ms(limit_seconds * 1000.0), last_ms(current_ms), max_seconds(limit_seconds) {
    if (!load("PracticeTimer", "practice_timer", limit_seconds)) {
        return;
    }
    fn_draw = lua_object["draw"];
}

void PracticeTimer::update(double current_ms, bool running) {
    bool frozen = global_data.config && global_data.config->limits.timer_frozen;
    double dt = running && !frozen ? current_ms - last_ms : 0.0;
    last_ms = current_ms;
    if (is_up()) {
        up_ms += dt;
        return;
    }
    remaining_ms -= dt;
    if (remaining_ms <= 0) {
        up_ms = 0;
    }
}

void PracticeTimer::start_review(int seconds, double current_ms) {
    remaining_ms = seconds * 1000.0;
    max_seconds = seconds;
    last_ms = current_ms;
    up_ms = -1;
}

void PracticeTimer::draw() {
    if (hidden) {
        return;
    }
    call(fn_draw, "PracticeTimer:draw", static_cast<int>(std::ceil(std::max(remaining_ms, 0.0) / 1000.0)), up_ms,
         max_seconds);
}

void PracticeTimer::begin_session(double current_ms) {
    int limit = global_data.config->limits.practice_time_limit;
    if (limit > 0 && !global_data.practice_timer) {
        global_data.practice_timer = std::make_shared<PracticeTimer>(limit, current_ms);
    }
}

void PracticeTimer::end_session() {
    global_data.practice_timer.reset();
}

PracticeTimer* PracticeTimer::active() {
    return global_data.practice_timer.get();
}
