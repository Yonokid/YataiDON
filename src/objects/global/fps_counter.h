#pragma once

#include <string>

class FPSCounter {
private:
    static const int SAMPLE_SIZE = 60;
    float frameTimes[SAMPLE_SIZE] = {};
    int currentFrame = 0;
    double lastTime = 0.0;
    double next_readout_ms = 0.0;
    int displayed_fps = 60;
    std::string readout;

public:
    FPSCounter();
    void update();
    float get_fps() const;
    void draw();
};
