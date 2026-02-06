#include "fps_counter.h"

FPSCounter::FPSCounter() {
    lastTime = get_current_ms();
    for (int i = 0; i < SAMPLE_SIZE; i++) {
        frameTimes[i] = 16.67f;
    }
}

void FPSCounter::update() {
    long currentTime = get_current_ms();
    float deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    frameTimes[currentFrame % SAMPLE_SIZE] = deltaTime;
    currentFrame++;
}

float FPSCounter::get_fps() {
    float sum = 0;
    for (int i = 0; i < SAMPLE_SIZE; i++) {
        sum += frameTimes[i];
    }
    float avgFrameTime = sum / SAMPLE_SIZE;
    return 1000.0f / avgFrameTime;
}

void FPSCounter::draw() {
    int curr_fps = get_fps();
    float pos = 20.0f * global_tex.screen_scale;

    ray::Color color;
    ray::Font font = global_data.font;

    if (curr_fps < 30) color = ray::RED;
    else if (curr_fps < 60) color = ray::YELLOW;
    else color = ray::LIME;

    DrawTextEx(font, ray::TextFormat("%d FPS", curr_fps), ray::Vector2{ pos, pos }, pos, 1.0f, color);
}
