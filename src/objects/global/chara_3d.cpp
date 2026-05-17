#include "chara_3d.h"
#include "raylib.h"
#include <cmath>

extern "C" void rlEnableDepthTest(void);
extern "C" void rlDisableDepthTest(void);
extern "C" void rlDrawRenderBatchActive(void);
extern "C" void glClear(unsigned int mask);
static constexpr unsigned int GL_DEPTH_BUFFER_BIT = 0x00000100;

static ray::Matrix rotation_xyz(float ax, float ay, float az) {
    float cx = cosf(-ax), sx = sinf(-ax);
    float cy = cosf(-ay), sy = sinf(-ay);
    float cz = cosf(-az), sz = sinf(-az);
    ray::Matrix r = {};
    r.m0 = cz*cy;  r.m1 = (cz*sy*sx) - (sz*cx);  r.m2 = (cz*sy*cx) + (sz*sx);
    r.m4 = sz*cy;  r.m5 = (sz*sy*sx) + (cz*cx);   r.m6 = (sz*sy*cx) - (cz*sx);
    r.m8 = -sy;    r.m9 = cy*sx;                   r.m10 = cy*cx;
    r.m15 = 1.0f;
    return r;
}

Chara3D::Chara3D(fs::path& model_path) {
    model = ray::LoadModel(model_path.string().c_str());
    anims = ray::LoadModelAnimations(model_path.string().c_str(), &anim_count);
}

Chara3D::~Chara3D() {
    ray::UnloadModelAnimations(anims, anim_count);
    ray::UnloadModel(model);
}

void Chara3D::update(double current_ms) {
    if (anim_count > 0 && playing) {
        double ms_per_beat = 60000.0 / bpm;
        double ms_per_frame = ms_per_beat / anims[anim_index].keyframeCount;
        if (current_ms - last_frame_ms >= ms_per_frame) {
            int loop_frames = anims[anim_index].keyframeCount - 1;
            anim_frame = (anim_frame + 1) % loop_frames;
            ray::UpdateModelAnimation(model, anims[anim_index], anim_frame);
            last_frame_ms = current_ms;
        }
    }
}

void Chara3D::draw(float x, float y) {
    rlDrawRenderBatchActive();
    glClear(GL_DEPTH_BUFFER_BIT);
    rlEnableDepthTest();
    ray::Matrix saved = model.transform;
    model.transform = rotation_xyz(rot_x * DEG2RAD, rot_y * DEG2RAD, rot_z * DEG2RAD);
    ray::DrawModel(model, {x, y, 400.0f}, scale, ray::WHITE);
    model.transform = saved;
    rlDisableDepthTest();
}
