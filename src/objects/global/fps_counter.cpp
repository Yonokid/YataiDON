#include "fps_counter.h"

#include <algorithm>
#include <format>
#include <numeric>

#include "../../libs/gfx/text.h"
#include "../../libs/gfx/texture.h"

FPSCounter::FPSCounter() {
    lastTime = get_current_ms();
    std::ranges::fill(frameTimes, 16.67f);
}

void FPSCounter::update() {
    double currentTime = get_current_ms();
    double deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    frameTimes[currentFrame] = std::clamp(deltaTime, 0.0, 1000.0);
    currentFrame = (currentFrame + 1) % SAMPLE_SIZE;
}

float FPSCounter::get_fps() const {
    const float sum = std::accumulate(std::begin(frameTimes), std::end(frameTimes), 0.0f);
    const float avgFrameTime = sum / SAMPLE_SIZE;
    return avgFrameTime > 0.0f ? 1000.0f / avgFrameTime : 0.0f;
}

void FPSCounter::draw() {
    const double now = get_current_ms();
    if (readout.empty() || now >= next_readout_ms) {
        displayed_fps = static_cast<int>(get_fps());
        readout = std::format("{} FPS", displayed_fps);
        next_readout_ms = now + 250;
    }
    const int size = static_cast<int>(20.0f * global_tex.screen_scale);
    const float pos = size;
    const Color color = displayed_fps < 30 ? Color::RED : displayed_fps < 60 ? Color::YELLOW : Color::LIME;
    Font font = font_manager.get_font(readout, size);
    draw_text_ex(font, readout.c_str(), {pos, pos}, pos, 1.0f, color);
}
