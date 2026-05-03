#pragma once

#include "../../libs/animation.h"
#include "../../libs/parsers/tja.h"

class GaugeHitEffect {
private:
    NoteType note_type;
    bool is_big;
    bool is_2p;
    TextureChangeAnimation* texture_change;
    FadeAnimation* circle_fadein;
    TextureResizeAnimation* resize;
    FadeAnimation* fade_out;
    FadeAnimation* rotation;
    ray::Color color;

    float width;

    float dest_width;
    float dest_height;
    float x2_pos;
    float y2_pos;
    ray::Vector2 origin;
    float rotation_angle;

public:
    GaugeHitEffect(NoteType note_type, bool big, bool is_2p);

    void update(double current_ms);

    void draw(float y);

    bool is_finished() const;
};
