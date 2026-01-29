#include "drumroll_counter.h"

DrumrollCounter::DrumrollCounter(bool is_2p)
 : is_2p(is_2p) {
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

void DrumrollCounter::draw() {
    tex.draw_texture("drumroll_counter", "bubble", {.fade=fade->attribute, .index=is_2p});
    std::string counter = std::to_string(drumroll_count);
    int total_width = counter.length() * tex.skin_config["drumroll_counter_margin"].x;
    for (int i = 0; i < counter.size(); i++) {
        char digit = counter[i];
        tex.draw_texture("drumroll_counter", "counter", {.frame=digit - '0', .x=-(total_width/2.0f)+(i*tex.skin_config["drumroll_counter_margin"].x), .y=-(float)stretch->attribute, .y2=(float)stretch->attribute, .fade=fade->attribute, .index=is_2p});
    }
}

bool DrumrollCounter::is_finished() const {
    return fade->is_finished;
}
