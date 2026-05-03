#include "combo.h"
#include "../../libs/texture.h"
#include <math.h>

Combo::Combo(int combo, double current_ms)
    : combo(combo) {
    stretch = (TextStretchAnimation*)tex.get_animation(5, true);
    color = {ray::Fade(ray::WHITE, 1), ray::Fade(ray::WHITE, 1), ray::Fade(ray::WHITE, 1)};
    glimmer_map[0] = 0;
    glimmer_map[1] = 0;
    glimmer_map[2] = 0;
    total_time = 250;
    cycle_time = total_time * 2;
    start_times = {
                current_ms,
                current_ms + (2.0f / 3.0f) * cycle_time,
                current_ms + (4.0f / 3.0f) * cycle_time
    };
}

void Combo::update_count(int curr_combo) {
    if (curr_combo != combo) {
        combo = curr_combo;
        stretch->start();
    }
}

void Combo::update(double current_ms, int curr_combo) {
    update_count(curr_combo);
    stretch->update(current_ms);

    for (size_t i = 0; i < 3; i++) {
        double elapsed_time = current_ms - start_times[i];
        if (elapsed_time > cycle_time) {
            double cycles_completed = std::floor(elapsed_time / cycle_time);
            start_times[i] += cycles_completed * cycle_time;
            elapsed_time = current_ms - start_times[i];
        }
        float fade;
        if (elapsed_time <= total_time) {
            glimmer_map[i] = -int(elapsed_time / 16.67);
            float fade_start_time = total_time - 164;
            if (elapsed_time >= fade_start_time) {
                fade = 1 - (elapsed_time - fade_start_time) / 164;
            } else {
                fade = 1;
            }
        } else {
            glimmer_map[i] = 0;
            fade = 0;
        }
        color[i] = ray::Fade(ray::WHITE, fade);
    }
}

void Combo::draw(float y) {
    if (combo < 3) return;

    std::string counter = std::to_string(combo);
    float margin;
    float total_width;
    if (combo < 100) {
        margin = tex.skin_config[SC::COMBO_MARGIN].x;
        total_width = counter.length() * margin;
        tex.draw_texture(COMBO::COMBO, {.y=y});
        for (int i = 0; i < counter.size(); i++) {
            char digit = counter[i];
            tex.draw_texture(COMBO::COUNTER, {.frame=digit - '0', .x=-(total_width / 2) + (i * margin), .y=y + (float)-stretch->attribute, .y2=(float)stretch->attribute});
        }

    } else {
        margin = tex.skin_config[SC::COMBO_MARGIN].y;
        total_width = counter.length() * margin;
        tex.draw_texture(COMBO::COMBO_100, {.y=y});
        for (int i = 0; i < counter.size(); i++) {
            char digit = counter[i];
            tex.draw_texture(COMBO::COUNTER_100, {.frame=digit - '0', .x=-(total_width / 2) + (i * margin), .y=y + (float)-stretch->attribute, .y2=(float)stretch->attribute});
        }
        std::vector<std::pair<float, float>> glimmer_positions = {
            {225 * tex.screen_scale, 210 * tex.screen_scale},
            {200 * tex.screen_scale, 230 * tex.screen_scale},
            {250 * tex.screen_scale, 230 * tex.screen_scale}
        };
        for (size_t j = 0; j < glimmer_positions.size(); j++) {
            auto [x, y_pos] = glimmer_positions[j];
            for (int i = 0; i < 3; i++) {
                tex.draw_texture(COMBO::GLEAM, {.color=color[j], .x=x+(i*tex.skin_config[SC::COMBO_MARGIN].x), .y=y+y_pos+glimmer_map[j]});
            }
        }
    }
}
