#pragma once

#include <algorithm>

#include "../../libs/file/script.h"

// Practice-mode session countdown (Lumen tokkun_timer). The clock lives here, the skin only draws it.
// One instance spans practice select and practice game (global_data.practice_timer).
class PracticeTimer : public LuaScript {
    sol::protected_function fn_draw;
    double remaining_ms;
    double last_ms;
    double up_ms = -1; // ms since the time ran out, -1 while running
    int max_seconds;   // full gauge
public:
    bool hidden = false;
    PracticeTimer(int limit_seconds, double current_ms);
    // running = false freezes the clock (paused game)
    void update(double current_ms, bool running = true);
    void draw();
    bool is_up() const { return up_ms >= 0; }
    // Time-up banner has been on screen long enough to leave
    bool is_finished() const { return up_ms >= 3000; }

    void expire() { remaining_ms = std::min(remaining_ms, 0.0); }
    // Restart as the review-time countdown
    void start_review(int seconds, double current_ms);

    // Start the session clock if practice_time_limit > 0 and none is running yet
    static void begin_session(double current_ms);
    static void end_session();
    // Shared timer, or nullptr when there is no limit
    static PracticeTimer* active();
};
