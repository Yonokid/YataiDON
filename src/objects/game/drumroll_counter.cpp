#include "drumroll_counter.h"

DrumrollCounter::DrumrollCounter() {
     drumroll_count = 0;
     fade = (FadeAnimation*)tex.get_animation(8);
     stretch = (TextStretchAnimation*)tex.get_animation(9);
}

void DrumrollCounter::update_count(int count) {
    if (drumroll_count != count) {
        drumroll_count = count;
        fade->start();
        stretch->start();
    }
}

void DrumrollCounter::update(double current_ms, int count) {
    fade->update(current_ms);
    stretch->update(current_ms);

    if (count != 0) update_count(count);
}

void DrumrollCounter::draw(float y) {
    tex.draw_texture(DRUMROLL_COUNTER::BUBBLE, {.y=y, .fade=fade->attribute});
    std::string counter = std::to_string(drumroll_count);
    int total_width = counter.length() * tex.skin_config[SC::DRUMROLL_COUNTER_MARGIN].x;
    for (int i = 0; i < counter.size(); i++) {
        char digit = counter[i];
        tex.draw_texture(DRUMROLL_COUNTER::COUNTER, {.frame=digit - '0', .x=-(total_width/2.0f)+(i*tex.skin_config[SC::DRUMROLL_COUNTER_MARGIN].x), .y=y -(float)stretch->attribute, .y2=(float)stretch->attribute, .fade=fade->attribute});
    }
}

bool DrumrollCounter::is_finished() const {
    return fade->is_finished;
}
