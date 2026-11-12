#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../../../libs/gfx.h"
#include "../../../libs/gfx/color_utils.h"
#include "../../../libs/gfx/text.h"
#include "../../../libs/gfx/texture.h"

class GradientTextures {
public:
    GradientTextures() = default;
    explicit GradientTextures(std::vector<Color> stops) : stops_(std::move(stops)) {}

    bool active() const { return stops_.size() > 1; }
    const std::vector<Color>& stops() const { return stops_; }

    std::shared_ptr<TextureObject> get(const std::string& name) {
        if (!active()) {
            return nullptr;
        }
        if (auto it = cache.find(name); it != cache.end()) {
            return it->second;
        }
        std::shared_ptr<TextureObject> result;
        TextureObject* src = tex.get_texture("box/" + name);
        const Texture* src_tex = src ? src->frame_texture(0) : nullptr;
        if (src_tex) {
            constexpr float base_luma = 0.299f * 142 + 0.587f * 212 + 0.114f * 30;
            Image img = load_image_from_texture(*src_tex);
            image_format(&img, pixel_format::RGBA8);
            auto* px = static_cast<unsigned char*>(img.data);
            const float last = static_cast<float>(stops_.size() - 1);
            for (int y = 0; y < img.height; y++) {
                const float pos = std::clamp((y + 0.5f) / img.height, 0.0f, 1.0f) * last;
                const size_t i = std::min(static_cast<size_t>(pos), stops_.size() - 2);
                const float t = pos - i;
                for (int x = 0; x < img.width; x++) {
                    unsigned char* p = px + (static_cast<size_t>(y) * img.width + x) * 4;
                    const float shade = (0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2]) / base_luma;
                    const Color& a = stops_[i];
                    const Color& b = stops_[i + 1];
                    p[0] = static_cast<unsigned char>(std::clamp((a.r + (b.r - a.r) * t) * shade, 0.0f, 255.0f));
                    p[1] = static_cast<unsigned char>(std::clamp((a.g + (b.g - a.g) * t) * shade, 0.0f, 255.0f));
                    p[2] = static_cast<unsigned char>(std::clamp((a.b + (b.b - a.b) * t) * shade, 0.0f, 255.0f));
                }
            }
            auto texture = std::shared_ptr<Texture>(new Texture(load_texture_from_image(img)), [](Texture* t) {
                unload_texture(*t);
                delete t;
            });
            unload_image(img);
            auto obj = std::make_shared<SingleTexture>(src->name, std::move(texture));
            obj->x = src->x;
            obj->y = src->y;
            obj->x2 = src->x2;
            obj->y2 = src->y2;
            obj->crop_data = src->crop_data;
            obj->opaque_right = src->opaque_right;
            result = std::move(obj);
        }
        cache[name] = result;
        return result;
    }

private:
    std::vector<Color> stops_;
    std::map<std::string, std::shared_ptr<TextureObject>> cache;
};
