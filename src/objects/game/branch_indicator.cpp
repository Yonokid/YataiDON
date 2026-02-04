#include "branch_indicator.h"

BranchIndicator::BranchIndicator(bool is_2p)
    : is_2p(is_2p), difficulty(BranchDifficulty::NORMAL), diff_2(BranchDifficulty::NORMAL), direction(1) {

    diff_down = (MoveAnimation*)tex.get_animation(41);
    diff_up = (MoveAnimation*)tex.get_animation(42);
    diff_fade = (FadeAnimation*)tex.get_animation(43);
    level_fade = (FadeAnimation*)tex.get_animation(44);
    level_scale = (TextureResizeAnimation*)tex.get_animation(45);
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

void BranchIndicator::draw() {
    if (difficulty == BranchDifficulty::EXPERT) {
        tex.draw_texture("branch", "expert_bg", {.fade = std::min(0.5f, (float)(1 - diff_fade->attribute)), .index = (int)is_2p});
    }
    if (difficulty == BranchDifficulty::MASTER) {
        tex.draw_texture("branch", "master_bg", {.fade = std::min(0.5f, (float)(1 - diff_fade->attribute)), .index = (int)is_2p});
    }

    std::string level_texture = direction == -1 ? "level_down" : "level_up";
    tex.draw_texture("branch", level_texture, {.scale = (float)level_scale->attribute, .center = true, .fade = level_fade->attribute, .index = (int)is_2p});

    tex.draw_texture("branch", branch_diff_to_string(diff_2), {.y = (float)(diff_down->attribute - diff_up->attribute) * direction, .fade = diff_fade->attribute, .index = (int)is_2p});

    tex.draw_texture("branch", branch_diff_to_string(difficulty), {.y = (float)(diff_up->attribute * (direction * -1)) - ((70 * tex.screen_scale) * direction * -1), .fade = 1 - diff_fade->attribute, .index = (int)is_2p});
}
