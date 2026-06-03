#pragma once
#include "global_data.h"
#include <algorithm>
#include <cmath>

inline ray::Camera2D compute_camera2d(int virtual_width, int virtual_height) {
    int sw = ray::GetScreenWidth();
    int sh = ray::GetScreenHeight();

    if (sw == 0 || sh == 0) {
        float ox = -(float)virtual_width  * 0.5f;
        float oy = -(float)virtual_height * 0.5f;
        return {{ox, oy}, {0, 0}, global_data.camera.rotation, 1.0f};
    }

    float scale        = std::min((float)sw / virtual_width, (float)sh / virtual_height);
    float base_ox      = (sw - virtual_width  * scale) * 0.5f;
    float base_oy      = (sh - virtual_height * scale) * 0.5f;
    float eff_zoom     = scale * global_data.camera.zoom;
    float zoom_ox      = (virtual_width  * scale * (global_data.camera.zoom - 1.0f)) * 0.5f;
    float zoom_oy      = (virtual_height * scale * (global_data.camera.zoom - 1.0f)) * 0.5f;
    float h_scale      = global_data.camera.h_scale;
    float v_scale      = global_data.camera.v_scale;
    float hscale_ox    = (virtual_width  * scale * (h_scale - 1.0f)) * 0.5f;
    float vscale_oy    = (virtual_height * scale * (v_scale - 1.0f)) * 0.5f;
    float offset_x     = base_ox - zoom_ox - hscale_ox + (global_data.camera.offset.x * scale);
    float offset_y     = base_oy - zoom_oy - vscale_oy + (global_data.camera.offset.y * scale);

    return {{offset_x, offset_y}, {0, 0}, global_data.camera.rotation, eff_zoom};
}

inline ray::Camera3D camera2d_to_3d(ray::Camera2D cam) {
    float sw  = (float)ray::GetScreenWidth();
    float sh  = (float)ray::GetScreenHeight();
    float rot = cam.rotation * DEG2RAD;
    float cx  = (sw * 0.5f - cam.offset.x) / cam.zoom;
    float cy  = (sh * 0.5f - cam.offset.y) / cam.zoom;
    return {
        {cx, cy, -100.0f},
        {cx, cy,    0.0f},
        {std::sin(rot), -std::cos(rot), 0.0f},
        sh / cam.zoom,
        ray::CAMERA_ORTHOGRAPHIC
    };
}
