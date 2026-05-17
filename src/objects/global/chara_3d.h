#pragma once

#include "../../libs/filesystem.h"
#include "../../libs/ray.h"

class Chara3D {
private:
    ray::Model model;

    ray::ModelAnimation* anims;
    int anim_count = 0;
    int anim_frame = 0;
    int anim_index = 7;
    bool playing = true;

    float bpm = 100;
    double last_frame_ms = 0;

    float scale = 800.0f;
    float rot_x = 180.0f;
    float rot_y = 28.0f;
    float rot_z = 0.0f;
public:
    Chara3D(fs::path& model_path);

    ~Chara3D();

    void update(double current_ms);

    void draw(float x, float y);
};
