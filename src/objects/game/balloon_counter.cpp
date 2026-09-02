#include "balloon_counter.h"
#include "../../libs/texture.h"
#include <algorithm>

BalloonCounter::BalloonCounter(int count, bool is_2p)
 : balloon_count(0), balloon_total(count), is_popped(false), is_2p(is_2p) {
     fade = (FadeAnimation*)tex.get_animation(7);
     stretch = (TextStretchAnimation*)tex.get_animation(6);
     fade->reset();
     stretch->reset();
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
    const SkinInfo* p2 = tex.skin_entry("balloon_counter_2p_offset");
    const bool have_p2_key = is_2p && p2;

    const float legacy_y = is_2p ? 230 * tex.screen_scale : 0.0f;
    const float rig_x = have_p2_key ? p2->x : 0.0f;
    const float rig_y = have_p2_key ? p2->y : 0.0f;

    const float x_offset = rig_x;
    const float y_offset = have_p2_key ? rig_y : legacy_y;
    const float digit_y_offset = have_p2_key ? rig_y : (legacy_y * 1.1f);
    const float body_x = rig_x;
    const float body_y = have_p2_key ? rig_y : 0.0f;
    const Mirror bubble_mirror = (is_2p && !have_p2_key) ? Mirror::VERTICAL
                                                         : Mirror::NONE;

    if (is_popped) {
        tex.draw_texture(BALLOON::POP, {.frame=7, .x=body_x, .y=y + body_y, .fade=fade->attribute});
    } else if (balloon_count >= 1) {
        static const int GEKI[6] = {0, 2, 3, 4, 5, 6};
        const int step = (balloon_total > 0) ? (balloon_count * 6 / balloon_total) : 0;
        const int balloon_index = GEKI[std::min(5, std::max(0, step))];
        tex.draw_texture(BALLOON::POP, {.frame=balloon_index, .x=body_x, .y=y + body_y, .fade=fade->attribute});
    }
    if (balloon_count > 0) {
        tex.draw_texture(BALLOON::BUBBLE, {.mirror = bubble_mirror, .x=x_offset, .y=y + y_offset, .fade=fade->attribute});
        std::string counter = std::to_string(std::max(0, balloon_total - balloon_count));
        float margin = tex.skin_config[SC::DRUMROLL_COUNTER_MARGIN].x;
        if (const SkinInfo* m = tex.skin_entry("balloon_counter_margin"); m && m->x > 0)
            margin = m->x;
        float total_width = counter.length() * margin;
        for (int i = 0; i < counter.size(); i++) {
            char digit = counter[i];
            tex.draw_texture(BALLOON::COUNTER, {.frame=digit - '0', .x=x_offset - (total_width / 2.0f) + (i * margin), .y=y - (float)stretch->attribute + digit_y_offset, .y2=(float)stretch->attribute, .fade=fade->attribute});
        }
    }
}

bool BalloonCounter::is_finished() const {
    return fade->is_finished;
}

bool BalloonCounter::has_popped() const {
    return is_popped;
}
