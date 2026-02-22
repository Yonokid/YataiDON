#include "../libs/texture.h"
#include "../libs/text.h"

class FPSCounter {
private:
    static const int SAMPLE_SIZE = 60;
    float frameTimes[SAMPLE_SIZE];
    int currentFrame = 0;
    long lastTime;

public:
    FPSCounter();

    void update();

    float get_fps();

    void draw();
};
