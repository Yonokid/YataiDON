#include "balloon_counter.h"

BalloonCounter::BalloonCounter(int count, PlayerNum player_num, bool is_2p)
 : balloon_count(count), balloon_total(count), is_popped(false), is_2p(is_2p), player_num(player_num) {
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
    fade->update(current_ms);

    if (count != 0) update_count(count);
}

void BalloonCounter::draw() {
    if (is_popped) {
        tex.draw_texture("balloon", "pop", {.frame=7, .y=is_2p*tex.skin_config["2p_offset"].y, .fade=fade->attribute});
    } else if (balloon_count >= 1) {
        int balloon_index = std::min(6, (balloon_count - 1) * 6 / balloon_total);
        tex.draw_texture("balloon", "pop", {.frame=balloon_index, .y=is_2p*tex.skin_config["2p_offset"].y, .fade=fade->attribute, .index=is_2p});
    }
    if (balloon_count > 0) {
        tex.draw_texture("balloon", "bubble", {.mirror=is_2p ? "vertical" : "", .y=is_2p*(410 * tex.screen_scale), .fade=fade->attribute});
        std::string counter = std::to_string(std::max(0, balloon_total - balloon_count + 1));
        int total_width = counter.length() * tex.skin_config["drumroll_counter_margin"].x;
        for (int i = 0; i < counter.size(); i++) {
            char digit = counter[i];
            tex.draw_texture("balloon", "counter", {.frame=digit - '0', .x=-(total_width / 2.0f) + (i * tex.skin_config["drumroll_counter_margin"].x), .y=-(float)stretch->attribute+(is_2p*435), .y2=(float)stretch->attribute, .fade=fade->attribute});
        }
    }
}

bool BalloonCounter::is_finished() const {
    return fade->is_finished;
}
