#pragma once
#include <cassert>
#include <memory>
#include <optional>
#include <vector>

// clang-format off
#include "../gfx/sdl_compat.h" // before ray.h: raylib.h must not be the first to pull in <stdarg.h>
#include "../gfx.h"       // IWYU pragma: keep
// clang-format on

struct FaceBox {
    float x, y, w, h;
    int confidence;
};

class WebCamera {
public:
    WebCamera() = default;
    ~WebCamera();

    WebCamera(const WebCamera&) = delete;
    WebCamera& operator=(const WebCamera&) = delete;
    WebCamera(WebCamera&& other) noexcept;
    WebCamera& operator=(WebCamera&& other) noexcept;

    bool open(int device_index = 0);
    void close();
    void update();

    bool is_open() const { return m_camera != nullptr; }
    bool is_ready() const { return m_texture.has_value(); }
    const Texture& get_texture() const {
        assert(m_texture.has_value() && "WebCamera::get_texture() called before is_ready()");
        return m_texture.value();
    }
    int width() const { return m_width; }
    void detect_faces(bool on);
    std::vector<FaceBox> faces() const;
    int height() const { return m_height; }

private:
    SDL_Camera* m_camera = nullptr;
    std::optional<Texture> m_texture;
    int m_width = 0;
    int m_height = 0;
    bool m_permission_denied_logged = false;
    struct FaceTracking;
    std::unique_ptr<FaceTracking> m_faces;
    void submit_faces(const uint8_t* rgba, int width, int height);
};

extern WebCamera webcam;
