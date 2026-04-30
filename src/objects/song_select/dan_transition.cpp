#include "dan_transition.h"

DanTransition::DanTransition() {
    started = false;
}

void DanTransition::start() {
    slide_in = (MoveAnimation*)tex.get_animation(38);
    slide_in->start();
    started = true;
}

void DanTransition::update(double current_ms) {
    slide_in->update(current_ms);
}

bool DanTransition::is_started() {
    return started;
}

bool DanTransition::is_finished() {
    return slide_in->is_finished;
}

void DanTransition::draw() {
    tex.draw_texture(DAN_TRANSITION::BACKGROUND, {.x2=(float)slide_in->attribute});
}
