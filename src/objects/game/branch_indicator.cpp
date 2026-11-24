#include "branch_indicator.h"

#include <algorithm>
#include <stdexcept>

namespace {
    constexpr int BRANCH_ANIM_DIFF_DOWN = 41;
    constexpr int BRANCH_ANIM_DIFF_UP = 42;
    constexpr int BRANCH_ANIM_DIFF_FADE = 43;
    constexpr int BRANCH_ANIM_LEVEL_FADE = 44;
    constexpr int BRANCH_ANIM_LEVEL_SCALE = 45;

    template<class T>
    T* require_anim(int id) {
        auto* a = dynamic_cast<T*>(tex.get_animation(id));
        if (!a) {
            throw std::runtime_error("animation " + std::to_string(id) + " has unexpected type");
        }
        return a;
    }

    std::unique_ptr<OutlinedText> make_caption(const char* key, float* dx, float* dy) {
        const SkinInfo* cfg = font_manager.get_text_info(key);
        const std::string label = font_manager.get_text_label(key);
        if (!cfg || label.empty()) {
            return nullptr;
        }
        *dx = cfg->x;
        *dy = cfg->y;
        return std::make_unique<OutlinedText>(label, cfg->font_size, Color::WHITE, Color::BLACK, false,
                                              cfg->outline >= 0 ? cfg->outline : 4.0f);
    }
} // namespace

BranchIndicator::BranchIndicator() {

    diff_down = require_anim<MoveAnimation>(BRANCH_ANIM_DIFF_DOWN);
    diff_up = require_anim<MoveAnimation>(BRANCH_ANIM_DIFF_UP);
    diff_fade = require_anim<FadeAnimation>(BRANCH_ANIM_DIFF_FADE);
    level_fade = require_anim<FadeAnimation>(BRANCH_ANIM_LEVEL_FADE);
    level_scale = require_anim<TextureResizeAnimation>(BRANCH_ANIM_LEVEL_SCALE);

    t_expert_bg = tex.get_texture("branch/expert_bg");
    t_master_bg = tex.get_texture("branch/master_bg");
    t_level_up = tex.get_texture("branch/level_up");
    t_level_down = tex.get_texture("branch/level_down");
    t_diff[static_cast<int>(BranchDifficulty::NORMAL)] = tex.get_texture("branch/normal");
    t_diff[static_cast<int>(BranchDifficulty::EXPERT)] = tex.get_texture("branch/expert");
    t_diff[static_cast<int>(BranchDifficulty::MASTER)] = tex.get_texture("branch/master");

    caption_up = make_caption("branch_level_up", &caption_dx, &caption_dy);
    caption_down = make_caption("branch_level_down", &caption_dx, &caption_dy);
}

void BranchIndicator::update(double current_ms) {
    diff_down->update(current_ms);
    diff_up->update(current_ms);
    diff_fade->update(current_ms);
    level_fade->update(current_ms);
    level_scale->update(current_ms);
}

void BranchIndicator::level_up(BranchDifficulty difficulty) {
    diff_2 = this->difficulty;
    this->difficulty = difficulty;
    diff_down->start();
    diff_up->start();
    diff_fade->start();
    level_fade->start();
    level_scale->start();
    direction = 1;
}

void BranchIndicator::level_down(BranchDifficulty difficulty) {
    diff_2 = this->difficulty;
    this->difficulty = difficulty;
    diff_down->start();
    diff_up->start();
    diff_fade->start();
    level_fade->start();
    level_scale->start();
    direction = -1;
}

void BranchIndicator::draw(float y) {
    if (difficulty == BranchDifficulty::EXPERT) {
        tex.draw_texture(t_expert_bg,
                         {.y = y, .fade = std::clamp(1.0f - static_cast<float>(diff_fade->attribute), 0.0f, 0.5f)});
    } else if (difficulty == BranchDifficulty::MASTER) {
        tex.draw_texture(t_master_bg,
                         {.y = y, .fade = std::clamp(1.0f - static_cast<float>(diff_fade->attribute), 0.0f, 0.5f)});
    }

    tex.draw_texture(
        direction == -1 ? t_level_down : t_level_up,
        {.scale = static_cast<float>(level_scale->attribute), .center = true, .y = y, .fade = level_fade->attribute});

    OutlinedText* caption = direction == -1 ? caption_down.get() : caption_up.get();
    if (caption && t_level_up) {
        const float s = static_cast<float>(level_scale->attribute);
        const float cx = static_cast<float>(t_level_up->x[0]) + caption_dx * s;
        const float cy = y + caption_dy * s;
        caption->draw({.scale = s,
                       .x = cx - caption->width * s / 2.0f,
                       .y = cy - caption->height * s / 2.0f,
                       .fade = level_fade->attribute});
    }

    tex.draw_texture(t_diff[static_cast<int>(diff_2)],
                     {.y = y + static_cast<float>(diff_down->attribute - diff_up->attribute) * direction,
                      .fade = diff_fade->attribute});

    tex.draw_texture(t_diff[static_cast<int>(difficulty)],
                     {.y = y + static_cast<float>(diff_up->attribute * (direction * -1)) -
                           (tex.get_skin_config(SC::GAME_BRANCH_INDICATOR_Y_OFFSET)->y * direction * -1),
                      .fade = 1 - diff_fade->attribute});
}
