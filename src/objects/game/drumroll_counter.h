#pragma once

#include "../../libs/gfx/texture.h"
#include "../../libs/math/animation.h"

class DrumrollCounter {
private:
    int drumroll_count;
    FadeAnimation* fade = nullptr;
    TextStretchAnimation* stretch = nullptr;
    TextureObject* t_bubble = nullptr;
    TextureObject* t_bubble_ai = nullptr;
    TextureObject* t_unfold = nullptr; // optional: the fan opening
    TextureObject* t_unfold_ai = nullptr;
    double start_ms = -1.0;
    double now_ms = 0.0;
    TextureObject* t_counter = nullptr;

public:
    DrumrollCounter();

    void update_count(int count);

    void update(double current_ms, int count);

    void update_animations(double current_ms);

    // ai_stage: the AI battle's stage (skin_config ai_game_drumroll / ai_game_drumroll_ai place it); ai_lane: the AI's
    // own
    void draw(float y, bool ai_stage = false, bool ai_lane = false);

    bool is_finished() const;
};
