#include "webcam.h"

#include "../gfx.h"
#include "../gfx/sdl_compat.h"
#if YATAIDON_SDL_CAMERA
#include <SDL3/SDL_camera.h>
#endif
#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <utility>
#include <vector>

#include "../latest_worker.h"

#if YATAIDON_HAS_FACE_DETECTION
#include <facedetectcnn.h>
#endif

namespace {
    constexpr int kDetectWidth = 320;
    constexpr int kMinConfidence = 70;
} // namespace

struct WebCamera::FaceTracking {
    std::atomic<bool> busy{false};
    mutable std::mutex mutex;
    std::vector<FaceBox> faces;
    LatestWorker worker;
};

std::vector<FaceBox> WebCamera::faces() const {
    if (!m_faces) {
        return {};
    }
    std::scoped_lock lock(m_faces->mutex);
    return m_faces->faces;
}

#if YATAIDON_HAS_FACE_DETECTION
void WebCamera::submit_faces(const uint8_t* rgba, int width, int height) {
    if (!m_faces || width <= 0 || height <= 0 || m_faces->busy.exchange(true)) {
        return;
    }
    const int dw = std::min(kDetectWidth, width);
    const int dh = std::max(1, height * dw / width);
    auto bgr = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(dw) * dh * 3);
    for (int y = 0; y < dh; ++y) {
        const uint8_t* row = rgba + static_cast<size_t>(y * height / dh) * width * 4;
        uint8_t* out = bgr->data() + static_cast<size_t>(y) * dw * 3;
        for (int x = 0; x < dw; ++x) {
            const uint8_t* px = row + static_cast<size_t>(x * width / dw) * 4;
            out[x * 3] = px[2];
            out[x * 3 + 1] = px[1];
            out[x * 3 + 2] = px[0];
        }
    }
    m_faces->worker.submit([state = m_faces.get(), bgr, dw, dh] {
        std::vector<unsigned char> buffer(FACEDETECTION_RESULT_BUFFER_SIZE);
        const int* results = facedetect_cnn(buffer.data(), bgr->data(), dw, dh, dw * 3);
        std::vector<FaceBox> found;
        for (int i = 0; results && i < *results; ++i) {
            const short* p = reinterpret_cast<const short*>(results + 1) + FACEDETECTION_RESULT_STRIDE_SHORTS * i;
            if (p[0] >= kMinConfidence) {
                found.push_back({static_cast<float>(p[1]) / dw, static_cast<float>(p[2]) / dh,
                                 static_cast<float>(p[3]) / dw, static_cast<float>(p[4]) / dh, p[0]});
            }
        }
        {
            std::scoped_lock lock(state->mutex);
            state->faces = std::move(found);
        }
        state->busy = false;
    });
}
#else
void WebCamera::submit_faces(const uint8_t*, int, int) {}
#endif

WebCamera webcam;

WebCamera::~WebCamera() {
    close();
}

void WebCamera::detect_faces(bool on) {
    if (!on) {
        m_faces.reset();
    } else if (!m_faces) {
        m_faces = std::make_unique<FaceTracking>();
    }
}

WebCamera::WebCamera(WebCamera&& other) noexcept
    : m_camera(other.m_camera), m_texture(other.m_texture), m_width(other.m_width), m_height(other.m_height),
      m_faces(std::move(other.m_faces)) {
    other.m_camera = nullptr;
    other.m_texture.reset();
    other.m_width = 0;
    other.m_height = 0;
}

WebCamera& WebCamera::operator=(WebCamera&& other) noexcept {
    if (this != &other) {
        close();
        m_camera = other.m_camera;
        m_texture = other.m_texture;
        m_width = other.m_width;
        m_height = other.m_height;
        m_faces = std::move(other.m_faces);
        other.m_camera = nullptr;
        other.m_texture.reset();
        other.m_width = 0;
        other.m_height = 0;
    }
    return *this;
}

#if !YATAIDON_SDL_CAMERA
bool WebCamera::open(int) {
    spdlog::warn("WebCamera: unsupported on SDL2");
    return false;
}

void WebCamera::close() {
    m_faces.reset();
    if (m_texture.has_value()) {
        unload_texture(m_texture.value());
        m_texture.reset();
    }
    m_width = m_height = 0;
}

void WebCamera::update() {}
#else
bool WebCamera::open(int device_index) {
    if (m_camera) {
        close();
    }

    if (!SDL_InitSubSystem(SDL_INIT_CAMERA)) {
        spdlog::error("WebCamera: failed to init SDL camera subsystem: {}", SDL_GetError());
        return false;
    }

    int count = 0;
    SDL_CameraID* ids = SDL_GetCameras(&count);
    if (!ids || count == 0) {
        spdlog::warn("WebCamera: no camera devices found");
        SDL_free(ids);
        SDL_QuitSubSystem(SDL_INIT_CAMERA);
        return false;
    }
    if (device_index < 0 || device_index >= count) {
        spdlog::warn("WebCamera: device index {} out of range ({} found)", device_index, count);
        SDL_free(ids);
        SDL_QuitSubSystem(SDL_INIT_CAMERA);
        return false;
    }

    SDL_Camera* cam = SDL_OpenCamera(ids[device_index], nullptr);
    SDL_free(ids);

    if (!cam) {
        spdlog::error("WebCamera: failed to open device {}: {}", device_index, SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_CAMERA);
        return false;
    }

    m_camera = cam;
    m_permission_denied_logged = false;
    spdlog::info("WebCamera: opened device {}", device_index);
    return true;
}

void WebCamera::close() {
    m_faces.reset();
    if (m_texture.has_value()) {
        unload_texture(m_texture.value());
        m_texture.reset();
    }
    if (m_camera) {
        SDL_CloseCamera(m_camera);
        m_camera = nullptr;
        SDL_QuitSubSystem(SDL_INIT_CAMERA);
    }
    m_width = 0;
    m_height = 0;
    m_permission_denied_logged = false;
}

void WebCamera::update() {
    if (!m_camera) {
        return;
    }

    // 0 = pending, -1 = denied
    int perm = SDL_GetCameraPermissionState(m_camera);
    if (perm == -1) {
        if (!m_permission_denied_logged) {
            spdlog::warn("WebCamera: permission denied");
            m_permission_denied_logged = true;
            if (m_texture.has_value()) {
                unload_texture(m_texture.value());
                m_texture.reset();
                m_width = m_height = 0;
            }
        }
        return;
    }
    if (perm == 0) {
        return;
    }

    Uint64 timestamp = 0;
    SDL_Surface* frame = SDL_AcquireCameraFrame(m_camera, &timestamp);
    if (!frame) {
        return;
    }

    SDL_Surface* rgba = SDL_ConvertSurface(frame, SDL_PIXELFORMAT_RGBA32);
    SDL_ReleaseCameraFrame(m_camera, frame);

    if (!rgba) {
        return;
    }

    std::vector<uint8_t> packed;
    void* pixels = rgba->pixels;
    if (rgba->pitch != rgba->w * 4) {
        const auto row_bytes = static_cast<size_t>(rgba->w) * 4;
        packed.resize(row_bytes * rgba->h);
        const uint8_t* src = static_cast<const uint8_t*>(rgba->pixels);
        for (int y = 0; y < rgba->h; y++) {
            const auto row = static_cast<size_t>(y);
            memcpy(packed.data() + row * row_bytes, src + row * rgba->pitch, row_bytes);
        }
        pixels = packed.data();
    }

    if (!m_texture.has_value()) {
        m_width = rgba->w;
        m_height = rgba->h;

        Image img{};
        img.data = pixels;
        img.width = m_width;
        img.height = m_height;
        img.mipmaps = 1;
        img.format = pixel_format::RGBA8;

        Texture tex = load_texture_from_image(img);
        if (tex.id == 0) {
            spdlog::error("WebCamera: failed to create texture {}x{}", m_width, m_height);
            m_width = m_height = 0;
            SDL_DestroySurface(rgba);
            return;
        }
        set_texture_filter(tex, texture_filter::BILINEAR);
        m_texture = tex;
    } else if (rgba->w != m_width || rgba->h != m_height) {
        // frame geometry changed - recreate the texture
        unload_texture(m_texture.value());
        m_texture.reset();
        m_width = rgba->w;
        m_height = rgba->h;
        Image img{};
        img.data = pixels;
        img.width = m_width;
        img.height = m_height;
        img.mipmaps = 1;
        img.format = pixel_format::RGBA8;
        Texture tex = load_texture_from_image(img);
        if (tex.id == 0) {
            spdlog::error("WebCamera: failed to create texture {}x{}", m_width, m_height);
            m_width = m_height = 0;
            SDL_DestroySurface(rgba);
            return;
        }
        set_texture_filter(tex, texture_filter::BILINEAR);
        m_texture = tex;
    } else {
        update_texture(m_texture.value(), pixels);
    }
    submit_faces(static_cast<const uint8_t*>(pixels), rgba->w, rgba->h);

    SDL_DestroySurface(rgba);
}
#endif // YATAIDON_SDL_CAMERA
