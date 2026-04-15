#include "balloon_counter.h"

BalloonCounter::BalloonCounter(int count)
 : balloon_count(count), balloon_total(count), is_popped(false) {
     fade = (FadeAnimation*)tex.get_animation(7);
     stretch = (TextStretchAnimation*)tex.get_animation(6);
}

void BalloonCounter::update_count(int count) {
    if (balloon_count != count) {
        balloon_count = count;
        fade->start();
        stretch->start();
        if (balloon_count == balloon_total) {
            is_popped = true;
        }
    }
}

void BalloonCounter::update(double current_ms, int count) {
    stretch->update(current_ms);
    if (is_popped) fade->update(current_ms);

    if (count != 0) update_count(count);
}

void BalloonCounter::draw(float y) {
    if (is_popped) {
        tex.draw_texture(BALLOON::POP, {.frame=7, .y=y, .fade=fade->attribute});
    } else if (balloon_count >= 1) {
        int balloon_index = std::min(6, (balloon_count - 1) * 6 / balloon_total);
        tex.draw_texture(BALLOON::POP, {.frame=balloon_index, .y=y, .fade=fade->attribute});
    }
    if (balloon_count > 0) {
        tex.draw_texture(BALLOON::BUBBLE, {.y=y, .fade=fade->attribute});
        std::string counter = std::to_string(std::max(0, balloon_total - balloon_count + 1));
        int total_width = counter.length() * tex.skin_config[SC::DRUMROLL_COUNTER_MARGIN].x;
        for (int i = 0; i < counter.size(); i++) {
            char digit = counter[i];
            tex.draw_texture(BALLOON::COUNTER, {.frame=digit - '0', .x=-(total_width / 2.0f) + (i * tex.skin_config[SC::DRUMROLL_COUNTER_MARGIN].x), .y=y - (float)stretch->attribute, .y2=(float)stretch->attribute, .fade=fade->attribute});
        }
    }
}

bool BalloonCounter::is_finished() const {
    return fade->is_finished;
}
