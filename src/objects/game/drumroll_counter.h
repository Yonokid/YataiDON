#include "../../libs/animation.h"
#include "../../libs/texture.h"

class DrumrollCounter {
private:
    bool is_2p;
    int drumroll_count;
    FadeAnimation* fade;
    TextStretchAnimation* stretch;

public:
    DrumrollCounter(bool is_2p);

    void update_count(int count);

    void update(double current_ms, int count);

    void draw();

    bool is_finished() const;
};
