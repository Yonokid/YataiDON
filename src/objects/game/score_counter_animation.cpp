#include "score_counter_animation.h"

ScoreCounterAnimation::ScoreCounterAnimation(PlayerNum player_num, int counter, bool is_2p)
    : is_2p(is_2p), counter(counter) {

    direction = is_2p ? -1 : 1;

    counter_str = std::to_string(counter);
    margin = tex.skin_config["score_counter_margin"].x;
    total_width = counter_str.length() * margin;
    y_pos_list.resize(counter_str.length(), 0.0f);

    fade_animation_1 = (FadeAnimation*)tex.get_animation(35, true);
    move_animation_1 = (MoveAnimation*)tex.get_animation(36, true);
    fade_animation_2 = (FadeAnimation*)tex.get_animation(37, true);
    move_animation_2 = (MoveAnimation*)tex.get_animation(38, true);
    move_animation_3 = (MoveAnimation*)tex.get_animation(39, true);
    move_animation_4 = (MoveAnimation*)tex.get_animation(40, true);

    fade_animation_1->start();
    move_animation_1->start();
    fade_animation_2->start();
    move_animation_2->start();
    move_animation_3->start();
    move_animation_4->start();

    if (player_num == PlayerNum::P2) {
        base_color = ray::Color{84, 250, 238, 255};
    } else {
        base_color = ray::Color{254, 102, 0, 255};
    }
    color = ray::Fade(base_color, 1.0f);
}

void ScoreCounterAnimation::update(double current_ms) {
    fade_animation_1->update(current_ms);
    move_animation_1->update(current_ms);
    move_animation_2->update(current_ms);
    move_animation_3->update(current_ms);
    move_animation_4->update(current_ms);
    fade_animation_2->update(current_ms);

    float fade_value = fade_animation_1->is_finished ? fade_animation_2->attribute : fade_animation_1->attribute;
    color = ray::Fade(base_color, fade_value);

    // Cache y positions
    for (int i = 0; i < counter_str.length(); i++) {
        y_pos_list[i] = move_animation_4->attribute + (i + 1) * 5;
    }
}

void ScoreCounterAnimation::draw() {
    float x = move_animation_1->is_finished ? move_animation_2->attribute : move_animation_1->attribute;
    if (x == 0) {
        return;
    }

    float start_x = x - total_width;

    for (int i = 0; i < counter_str.length(); i++) {
        float y;
        if (move_animation_3->is_finished) {
            y = y_pos_list[i];
        } else if (move_animation_2->is_finished) {
            y = move_animation_3->attribute;
        } else {
            y = 148 * tex.screen_scale;
        }

        float y_offset = y * direction;

        tex.draw_texture("lane", "score_number", {
            .color = color,
            .frame = counter_str[i] - '0',
            .x = start_x + (i * margin),
            .y = y_offset + (is_2p * 680 * tex.screen_scale)
        });
    }
}

bool ScoreCounterAnimation::is_finished() const {
    return fade_animation_2->is_finished;
}
