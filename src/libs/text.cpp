#include "text.h"
#include <raylib.h>
#include <spdlog/spdlog.h>

FontManager::FontManager() {
}

void FontManager::init(const std::string& font_path) {
    this->font_path = font_path;
    for (int i = 32; i < 127; i++)
        codepoint_cache.insert(i);
    std::vector<int> codepoints(codepoint_cache.begin(), codepoint_cache.end());
    font = ray::LoadFontEx(font_path.c_str(), max_font_size, codepoints.data(), codepoints.size());
    ray::SetTextureFilter(font.texture, ray::TEXTURE_FILTER_BILINEAR);
}

ray::Font FontManager::get_font(const std::string& text, int font_size) {
    bool reload = false;

    const char* ptr = text.c_str();
    while (*ptr) {
        int cp_size = 0;
        int codepoint = ray::GetCodepointNext(ptr, &cp_size);
        if (!codepoint_cache.count(codepoint)) {
            codepoint_cache.insert(codepoint);
            reload = true;
        }
        ptr += cp_size;
    }

    if (font_size > max_font_size) {
        reload = true;
        max_font_size = font_size;
    }

    if (reload) {
        ray::UnloadFont(font);

        std::vector<int> codepoints(codepoint_cache.begin(), codepoint_cache.end());

        font = ray::LoadFontEx(
            font_path.c_str(),
            max_font_size,
            codepoints.data(),
            static_cast<int>(codepoints.size())
        );

        ray::SetTextureFilter(font.texture, ray::TEXTURE_FILTER_BILINEAR);
    }

    return font;
}

OutlinedText::OutlinedText(std::string text, int font_size, ray::Color color, ray::Color outline_color, int outline_thickness)
    : text(text), font_size(font_size), outline_thickness(outline_thickness) {
    sdf_font = font_manager.get_font(text, font_size);

    width  = ray::MeasureTextEx(sdf_font, text.c_str(), font_size, 2).x;
    height = ray::MeasureTextEx(sdf_font, text.c_str(), font_size, 2).y;

    int pad = this->outline_thickness + 2;
    ray::Image img = ray::GenImageColor((int)width + pad*2, (int)height + pad*2, ray::BLANK);

    for (float angle = 0; angle < 2 * PI; angle += (PI/8)) {
        float ox = cosf(angle) * this->outline_thickness;
        float oy = sinf(angle) * this->outline_thickness;
        ray::ImageDrawTextEx(&img, sdf_font, text.c_str(), {pad + ox, pad + oy}, font_size, 2, outline_color);
    }
    ray::ImageDrawTextEx(&img, sdf_font, text.c_str(), {(float)pad, (float)pad}, font_size, 2, ray::WHITE);

    texture = ray::LoadTextureFromImage(img);
    ray::UnloadImage(img);
}

void OutlinedText::draw(float x, float y, float fade) {
    ray::Rectangle src = {
        0, 0,
        (float)texture.width,
        (float)texture.height
    };
    int pad = outline_thickness + 2;
    ray::Rectangle dst = {x - pad, y - pad, (float)texture.width, (float)texture.height};
    ray::DrawTexturePro(texture, src, dst, {0, 0}, 0.0f, ray::Fade(ray::WHITE, fade));
}

FontManager font_manager;
