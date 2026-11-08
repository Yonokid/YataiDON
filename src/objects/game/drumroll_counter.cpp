#include "drumroll_counter.h"

#include <algorithm>
#include <stdexcept>

static constexpr int DRUMROLL_COUNTER_FADE_ANIM_ID = 8;
static constexpr int DRUMROLL_COUNTER_STRETCH_ANIM_ID = 9;

DrumrollCounter::DrumrollCounter() {
    drumroll_count = 0;
    fade = dynamic_cast<FadeAnimation*>(tex.get_animation(DRUMROLL_COUNTER_FADE_ANIM_ID));
    stretch = dynamic_cast<TextStretchAnimation*>(tex.get_animation(DRUMROLL_COUNTER_STRETCH_ANIM_ID));
    if (!fade || !stretch) {
        throw std::runtime_error("DrumrollCounter: animation 8/9 has an unexpected type");
    }
    t_bubble = tex.get_texture("drumroll_counter/bubble");
    if (tex.has_texture("drumroll_counter/bubble_ai")) {
        t_bubble_ai = tex.get_texture("drumroll_counter/bubble_ai");
    }
    t_counter = tex.get_texture("drumroll_counter/counter");
    if (tex.has_texture("drumroll_counter/bubble_unfold")) {
        t_unfold = tex.get_texture("drumroll_counter/bubble_unfold");
    }
    if (tex.has_texture("drumroll_counter/bubble_unfold_ai")) {
        t_unfold_ai = tex.get_texture("drumroll_counter/bubble_unfold_ai");
    }
}

void DrumrollCounter::update_count(int count) {
    count = std::max(count, 0);
    if (drumroll_count != count) {
        drumroll_count = count;
        fade->start();
        stretch->start();
    }
}

void DrumrollCounter::update(double current_ms, int count) {
    update_count(count);
    update_animations(current_ms);
}

void DrumrollCounter::update_animations(double current_ms) {
    if (start_ms < 0.0) {
        start_ms = current_ms;
    }
    now_ms = current_ms;
    fade->update(current_ms);
    stretch->update(current_ms);
}

void DrumrollCounter::draw(float y, bool ai_stage, bool ai_lane) {
    float x = 0.0f;
    bool use_ai_art = false;
    float fan_scale = 1.0f;
    const SkinInfo* pos =
        ai_stage ? tex.get_skin_config(ai_lane ? SC::AI_GAME_DRUMROLL_AI : SC::AI_GAME_DRUMROLL) : nullptr;
    if (pos) {
        x = pos->x;
        y += pos->y;
        use_ai_art = t_bubble_ai != nullptr;
        if (const SkinInfo* s = tex.get_skin_config(SC::GAME_AI_DRUMROLL_COUNTER_SCALE); s && s->x > 0.0f) {
            fan_scale = s->x;
        }
    }
    // the fan opens over its first 12 frames at 60 fps; the digits fade in on frames 8-12
    static constexpr int UNFOLD_FRAMES = 12;
    static constexpr int UNFOLD_SHAPE[UNFOLD_FRAMES] = {0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 3};
    static constexpr double UNFOLD_ALPHA[5] = {0.0, 0.332, 0.668, 1.0, 1.0};
    double bubble_alpha = 1.0;
    double digit_alpha = 1.0;
    int unfold_shape = -1;
    if (pos && t_unfold != nullptr && start_ms >= 0.0) {
        const int k = static_cast<int>(std::max(now_ms - start_ms, 0.0) * 60.0 / 1000.0);
        if (k < UNFOLD_FRAMES) {
            unfold_shape = UNFOLD_SHAPE[k];
            bubble_alpha = k < 5 ? UNFOLD_ALPHA[k] : 1.0;
            digit_alpha = k < 8 ? 0.0 : 0.2 * (k - 7);
        }
    }
    if (unfold_shape >= 0) {
        tex.draw_texture((use_ai_art && t_unfold_ai) ? t_unfold_ai : t_unfold,
                         {.frame = unfold_shape,
                          .scale = fan_scale,
                          .center = true,
                          .x = x,
                          .y = y,
                          .fade = fade->attribute * bubble_alpha});
    } else {
        tex.draw_texture(use_ai_art ? t_bubble_ai : t_bubble,
                         {.scale = fan_scale, .center = true, .x = x, .y = y, .fade = fade->attribute});
    }
    if (digit_alpha <= 0.0) {
        return;
    }
    std::string counter = std::to_string(drumroll_count);
    const float margin = tex.get_skin_config(SC::GAME_DRUMROLL_COUNTER_MARGIN)->x;
    float total_width = static_cast<float>(counter.length()) * margin;
    // the digits scale with the fan, about the fan's centre (with fan_scale 1 these are the plain offsets)
    const float fan_cx = t_bubble->x[0] + (t_bubble->x2[0] * 0.5f);
    const float fan_cy = t_bubble->y[0] + (t_bubble->y2[0] * 0.5f);
    const float digit_cx = t_counter->x[0] + (t_counter->x2[0] * 0.5f);
    const float digit_cy = t_counter->y[0] + (t_counter->y2[0] * 0.5f);
    const auto scaled = [fan_scale](float value, float origin, float centre, float fan) {
        return origin + fan - centre + (fan_scale * (centre - fan + value - origin));
    };
    for (size_t i = 0; i < counter.size(); i++) {
        char digit = counter[i];
        if (digit < '0' || digit > '9') {
            continue;
        }
        tex.draw_texture(t_counter, {.frame = digit - '0',
                                     .scale = fan_scale,
                                     .center = true,
                                     .x = scaled(x - (total_width / 2.0f) + (i * margin), x, digit_cx, fan_cx),
                                     .y = scaled(y - static_cast<float>(stretch->attribute), y, digit_cy, fan_cy),
                                     .y2 = static_cast<float>(stretch->attribute) * fan_scale,
                                     .fade = fade->attribute * digit_alpha});
    }
}

bool DrumrollCounter::is_finished() const {
    return fade->is_finished;
}
