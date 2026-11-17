#include "genre_bg.h"

#include <cmath>

#include "../../../libs/color_shader.h"

namespace {
    constexpr int MAX_SIDE_BOXES = 5;

    double ease_in(double t) {
        return t * t;
    }
    double ease_out(double t) {
        return 1.0 - (1.0 - t) * (1.0 - t);
    }
} // namespace

GenreBG::GenreBG(const std::string& text_name, std::vector<Color> colors, int texture_index)
    : texture_index(texture_index), gradients(colors) {
    float base_font_size = tex.get_skin_config(SC::SONG_SELECT_BOX_NAME)->font_size;
    float font_size = base_font_size;
    float name_outline = 5.0f;
    if (utf8_char_count(text_name) >= 30) {
        font_size = base_font_size - 10.0f * tex.screen_scale;
        name_outline = 5.0f * (font_size / base_font_size);
    }
    name = make_unique<OutlinedText>(text_name, static_cast<int>(font_size), Color::WHITE, Color::BLACK, false,
                                     name_outline);

    if (!colors.empty()) {
        shader_owner = load_color_shader(colors.front());
        shader = *shader_owner;
        shader_loaded = is_shader_valid(shader);
    }

    duration_scale = global_data.config->general.fast_transitions ? 0.2 : 1.0;
    scale_x = 0.96f;
    scale_y = 0.92f;
}

void GenreBG::enter(Phase phase) {
    phase_ = phase;
    phase_ms = -1.0;
}

void GenreBG::open_sides() {
    if (phase_ == Phase::WAIT) {
        enter(Phase::OPENING);
    }
}

void GenreBG::close_sides() {
    enter(Phase::CLOSING);
    duration_scale = 1.0;
    close_from = {closed_.left + std::clamp(left - closed_.left, -reach_, reach_),
                  closed_.right + std::clamp(right - closed_.right, -reach_, reach_)};
}

void GenreBG::pop_out() {
    enter(Phase::POP_OUT);
}

void GenreBG::settle() {
    enter(Phase::OPEN);
    pop_alpha = 1.0f;
    scale_x = 1.0f;
    scale_y = 1.0f;
}

void GenreBG::fade_out() {
    dim = std::make_unique<FadeAnimation>(300);
    dim->start();
}

void GenreBG::fade_in() {
    dim = std::make_unique<FadeAnimation>(300, 0.0, false, false, 1.0);
    dim->start();
}

float GenreBG::alpha() const {
    return pop_alpha * (dim ? static_cast<float>(dim->attribute) : 1.0f);
}

double GenreBG::side_ms(float distance, float pitch) const {
    int boxes = std::min(MAX_SIDE_BOXES, static_cast<int>(std::ceil(std::abs(distance) / pitch - 0.01f)));
    return std::min(boxes, 4) * 125.0 * duration_scale;
}

void GenreBG::update(double current_ms) {
    now_ms = current_ms;
    if (phase_ms < 0.0) {
        phase_ms = current_ms;
    }
    if (dim) {
        dim->update(current_ms);
    }
    const double elapsed = current_ms - phase_ms;

    if (phase_ == Phase::POP_IN) {
        const double grow = 200.0 * duration_scale;
        const double end = 450.0 * duration_scale;
        if (elapsed < grow) {
            const auto q = static_cast<float>(ease_out(elapsed / grow));
            scale_x = 0.96f + 0.04f * q;
            scale_y = 0.92f + 0.109f * q;
            pop_alpha = q;
        } else if (elapsed < end) {
            scale_x = 1.0f;
            scale_y = 1.029f - 0.029f * static_cast<float>((elapsed - grow) / (end - grow));
            pop_alpha = 1.0f;
        } else {
            scale_x = scale_y = pop_alpha = 1.0f;
            enter(Phase::WAIT);
        }
    } else if (phase_ == Phase::POP_OUT) {
        const double shrink = 450.0 * duration_scale;
        const double gone = 500.0 * duration_scale;
        const auto p = static_cast<float>(std::min(1.0, elapsed / shrink));
        scale_x = 1.0f - 0.04f * p;
        scale_y = 1.0f - 0.08f * p;
        pop_alpha =
            elapsed < shrink ? 1.0f : std::max(0.0f, 1.0f - static_cast<float>((elapsed - shrink) / (gone - shrink)));
        if (elapsed >= gone) {
            enter(Phase::DONE);
        }
    }
}

void GenreBG::set_edges(Span closed, Span live, float pitch) {
    center = (closed.left + closed.right) / 2.0f;
    const double elapsed = phase_ms < 0.0 ? 0.0 : now_ms - phase_ms;
    const float reach = MAX_SIDE_BOXES * pitch;
    closed_ = closed;
    reach_ = reach;

    auto side = [&](float from, float to, bool opening, bool& done) {
        const float distance = std::clamp(to - from, -reach, reach);
        const double duration = side_ms(distance, pitch);
        const double t = duration <= 0.0 ? 1.0 : std::min(1.0, elapsed / duration);
        done = done && t >= 1.0;
        return from + distance * static_cast<float>(opening ? ease_in(t) : ease_out(t));
    };

    switch (phase_) {
    case Phase::OPENING: {
        bool done = true;
        left = side(closed.left, live.left, true, done);
        right = side(closed.right, live.right, true, done);
        if (done) {
            enter(Phase::OPEN);
            left = live.left;
            right = live.right;
        }
        break;
    }
    case Phase::OPEN:
        left = live.left;
        right = live.right;
        break;
    case Phase::CLOSING: {
        bool done = true;
        left = side(close_from.left, closed.left, false, done);
        right = side(close_from.right, closed.right, false, done);
        if (done) {
            enter(Phase::SHUT);
        }
        break;
    }
    default:
        left = closed.left;
        right = closed.right;
        break;
    }

    left = std::clamp(left, -pitch, tex.screen_width + pitch);
    right = std::clamp(right, -pitch, tex.screen_width + pitch);
}
