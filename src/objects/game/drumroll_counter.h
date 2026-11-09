#pragma once

#include "../../libs/file/script.h"
#include "../../libs/gfx/texture.h"
#include "../../libs/global_data.h"
#include "../../libs/math/animation.h"

class DrumrollCounter : public LuaScript {
private:
    int drumroll_count;
    FadeAnimation* fade = nullptr;
    TextStretchAnimation* stretch = nullptr;
    TextureObject* t_bubble = nullptr;
    TextureObject* t_counter = nullptr;

    sol::protected_function fn_draw;

public:
    explicit DrumrollCounter(PlayerNum player_num);

    void update_count(int count);

    void update(double current_ms, int count);

    void update_animations(double current_ms);

    void draw(float y);

    bool is_finished() const;
};
