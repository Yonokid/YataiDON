#include "dan_transition.h"
#include "../../libs/texture.h"
#include <algorithm>

DanTransition::DanTransition() {
    started = false;
}

void DanTransition::start() {
    slide_in = (MoveAnimation*)tex.get_animation(38);
    slide_in->start();
    started  = true;
    start_ms = 0;
    last_ms  = 0;
}

void DanTransition::update(double current_ms) {
    if (start_ms == 0) start_ms = current_ms;
    last_ms = current_ms;
    slide_in->update(current_ms);
}

bool DanTransition::is_started() {
    return started;
}

bool DanTransition::is_finished() {
    return slide_in->is_finished;
}

double DanTransition::duration() const {
    return (slide_in && slide_in->duration > 0) ? slide_in->duration : 0.0;
}

double DanTransition::progress() const {
    if (!started || start_ms == 0) return 0.0;
    double d = duration();
    if (d <= 0.0) return 1.0;
    return std::clamp((last_ms - start_ms) / d, 0.0, 1.0);
}

void DanTransition::draw() {
    tex.draw_texture(DAN_TRANSITION::BACKGROUND, {.x2=(float)slide_in->attribute});
}
