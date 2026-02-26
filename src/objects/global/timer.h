#pragma once
#include "../../libs/config.h"
#include "../../libs/texture.h"
#include "../../libs/animation.h"
#include "../../libs/audio.h"
#include <functional>
#include <string>

class Timer {
private:
    int time;
    double last_time;
    std::string counter;
    TextureResizeAnimation* num_resize;
    TextureResizeAnimation* highlight_resize;
    FadeAnimation* highlight_fade;
    std::function<void()> confirm_func;
    bool is_finished;
    bool is_frozen;
public:
    Timer(int time, double current_time_ms, std::function<void()> confirm_func);
    void update(double current_ms);
    void draw(float x = 0, float y = 0);
};
