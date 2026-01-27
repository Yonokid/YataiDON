#include "enums.h"
#include "../libs/animation.h"
#include "../libs/texture.h"

class DrumHitEffect {
private:
    DrumType type;
    Side side;
    bool is_2p;
    FadeAnimation* fade;

public:
    DrumHitEffect(DrumType type, Side side, bool is_2p);

    void update(double current_ms);

    void draw();

    bool is_finished() const;

};
