#pragma once

#include <algorithm>
#include <memory>
#include <string>

#include "../../libs/audio.h"
#include "../../libs/global_data.h"
#include "../../libs/screen.h"
#include "../../libs/text.h"
#include "../../libs/texture.h"

class DanBetween {
public:
    static constexpr double FPS     = 60.0;
    static constexpr double F_START = 5.0;
    static constexpr double F_STAY  = 123.0;
    static constexpr double F_OPEN  = 204.0;
    static constexpr double F_END   = 369.0;
    static constexpr double CLOSE_TO_OPEN_FRAMES = 360.0;

    static constexpr double SONG_OPEN_MS = CLOSE_TO_OPEN_FRAMES * 1000.0 / FPS;
    static constexpr double TOTAL_MS = SONG_OPEN_MS + (F_END - F_OPEN) * 1000.0 / FPS;

    void start(double now, const std::string& title, const std::string& subtitle,
               bool show_subtitle);
    void stop();

    void update(double now);
    void draw(float lane_y);

    bool is_active() const { return active; }
    bool song_may_start() const { return !active || elapsed >= SONG_OPEN_MS; }

private:
    bool   active     = false;
    bool   open_fired = false;
    double start_ms   = 0.0;
    double elapsed    = 0.0;
    std::unique_ptr<OutlinedText> title_text;
    std::unique_ptr<OutlinedText> subtitle_text;

    double clip_frame() const;
    double text_alpha() const;
    double door_tx() const;
};


namespace dan_between_detail {

inline float skin_outline(const SkinInfo& s) { return s.outline >= 0 ? s.outline : 3.0f; }

inline double fallback_tx(double f) {
    if (f <= DanBetween::F_START) return 1071.0;
    if (f < 67.0)  return 1071.0 + (360.0 - 1071.0) * (f - 5.0) / 62.0;
    if (f <= DanBetween::F_OPEN) return 360.0;
    if (f < 219.0) return 360.0 + (1071.0 - 360.0) * (f - 204.0) / 15.0;
    return 1071.0;
}

}  // namespace dan_between_detail

inline void DanBetween::start(double now, const std::string& title,
                              const std::string& subtitle, bool show_subtitle) {
    active     = true;
    open_fired = false;
    start_ms   = now;
    elapsed    = 0.0;

    const SkinInfo* t = tex.skin_entry("dan_between_title");
    const SkinInfo* s = tex.skin_entry("dan_between_sub");
    title_text.reset();
    subtitle_text.reset();
    if (t && !title.empty()) {
        title_text = std::make_unique<OutlinedText>(
            title, t->font_size > 0 ? t->font_size : 54, ray::WHITE, ray::BLACK,
            false, dan_between_detail::skin_outline(*t));
    }
    if (s && show_subtitle && !subtitle.empty()) {
        subtitle_text = std::make_unique<OutlinedText>(
            subtitle, s->font_size > 0 ? s->font_size : 30, ray::WHITE, ray::BLACK,
            false, dan_between_detail::skin_outline(*s));
    }

    audio.play_sound("fusuma_close", VolumePreset::SOUND);
}

inline void DanBetween::stop() {
    active     = false;
    open_fired = false;
    elapsed    = 0.0;
    title_text.reset();
    subtitle_text.reset();
}

inline void DanBetween::update(double now) {
    if (!active) return;
    elapsed = now - start_ms;
    if (!open_fired && elapsed >= SONG_OPEN_MS) {
        open_fired = true;
        audio.play_sound("fusuma_open", VolumePreset::SOUND);
    }
    if (elapsed >= TOTAL_MS) stop();
}

inline double DanBetween::clip_frame() const {
    if (elapsed < SONG_OPEN_MS) {
        return std::min(F_STAY, F_START + elapsed * FPS / 1000.0);
    }
    return std::min(F_END, F_OPEN + (elapsed - SONG_OPEN_MS) * FPS / 1000.0);
}

inline double DanBetween::door_tx() const {
    return dan_between_detail::fallback_tx(clip_frame());
}

inline double DanBetween::text_alpha() const {
    const double f = clip_frame();
    if (f < F_STAY) return 0.0;
    if (f < 340.0)  return 1.0;
    return std::clamp((368.0 - f) / 28.0, 0.0, 1.0);
}

inline void DanBetween::draw(float lane_y) {
    if (!active) return;
    const double f = clip_frame();
    if (f < F_START || f >= F_END) return;
    if (!tex.has_texture("lane/dan_between_fusuma")) return;

    const uint32_t door_id = (uint32_t)tex.get_enum("lane/dan_between_fusuma");
    auto dit = tex.textures.find(door_id);
    if (dit == tex.textures.end()) return;
    const TextureObject& door = *dit->second;

    const double a = text_alpha();
    if (a > 0.0) {
        if (title_text) {
            const SkinInfo& c = *tex.skin_entry("dan_between_title");
            title_text->draw({.x = c.x - title_text->width / 2.0f,
                              .y = c.y - title_text->height / 2.0f + lane_y,
                              .fade = a});
        }
        if (subtitle_text) {
            const SkinInfo& c = *tex.skin_entry("dan_between_sub");
            subtitle_text->draw({.x = c.x - subtitle_text->width / 2.0f,
                                 .y = c.y - subtitle_text->height / 2.0f + lane_y,
                                 .fade = a});
        }
    }

    float mask_x = 498.0f, mask_y = 12.0f, mask_h = 195.0f;
    if (tex.has_texture("lane/dan_between_fill")) {
        auto mit = tex.textures.find((uint32_t)tex.get_enum("lane/dan_between_fill"));
        if (mit != tex.textures.end()) {
            mask_x = (float)mit->second->x[0];
            mask_y = (float)mit->second->y[0];
            mask_h = (float)mit->second->y2[0];
        }
    }

    const int scissor_x = virtual_to_screen_x(mask_x);
    const int win_w = ray::GetScreenWidth();
    ray::BeginScissorMode(scissor_x, 0, win_w - scissor_x, ray::GetScreenHeight());

    const float door_y = (float)door.y[0];
    const float door_h = (float)door.y2[0];
    const float crop_top = std::max(0.0f, mask_y - door_y);
    const float crop_h =
        std::max(0.0f, std::min(door_h - crop_top, (mask_y + mask_h) - (door_y + crop_top)));

    const float sc = (door.height > 0 && door_h > 0) ? door_h / (float)door.height : 1.0f;
    const float src_top = crop_top / sc;
    const float src_h   = crop_h / sc;

    const float tx = (float)door_tx();
    tex.draw_texture(door_id, {.x = tx, .y = lane_y + crop_top,
                               .y2 = crop_h - door_h,
                               .src = ray::Rectangle{0, src_top,
                                                     (float)door.width, src_h}});
    tex.draw_texture(door_id, {.x = -tx, .y = lane_y + crop_top,
                               .y2 = crop_h - door_h,
                               .src = ray::Rectangle{0, src_top,
                                                     -(float)door.width, src_h}});

    ray::EndScissorMode();
}
