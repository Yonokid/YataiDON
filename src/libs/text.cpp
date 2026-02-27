#include "text.h"
#include "raylib.h"

FontManager::FontManager() {
}

void FontManager::init(const fs::path& font_path) {
    this->font_path = font_path;
    for (int i = 32; i < 127; i++)
        codepoint_cache.insert(i);
    std::vector<int> codepoints(codepoint_cache.begin(), codepoint_cache.end());
    font = ray::LoadFontEx(font_path.string().c_str(), max_font_size, codepoints.data(), codepoints.size());
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
            font_path.string().c_str(),
            max_font_size,
            codepoints.data(),
            static_cast<int>(codepoints.size())
        );

        ray::SetTextureFilter(font.texture, ray::TEXTURE_FILTER_BILINEAR);
    }

    return font;
}

static bool in_rotate_set(const std::string& s) {
    static const std::unordered_set<std::string> rotate_set = {
        "-", "\xe2\x80\x90",        // -, ‐
        "|",                         // |
        "/", "\\",                   // /, backslash
        "\xe3\x83\xbc",             // ー
        "\xef\xbd\x9e",             // ～
        "~",                         // ~
        "\xef\xbc\x88", "\xef\xbc\x89", // （ ）
        "(", ")",                    // ( )
        "\xe3\x80\x8c", "\xe3\x80\x8d", // 「 」
        "[", "]",                    // [ ]
        "\xef\xbc\xb3", "\xef\xbc\xb4", // ［ ］
        "\xe3\x80\x90", "\xe3\x80\x91", // 【 】
        "\xe2\x80\xa6",             // …
        "\xe2\x86\x92",             // →
        ":", "\xef\xbc\x9a",        // :, ：
    };
    return rotate_set.count(s) > 0;
}

OutlinedText::OutlinedText(std::string text, int font_size, ray::Color color, ray::Color outline_color, bool is_vertical, int outline_thickness, float spacing)
    : text(text), font_size(font_size), outline_thickness(outline_thickness) {
    sdf_font = font_manager.get_font(text, font_size);
    if (is_vertical) {
        create_vertical_text(color, outline_color, spacing);
    } else {
        create_horizontal_text(color, outline_color, spacing);
    }
}

void OutlinedText::create_horizontal_text(ray::Color color, ray::Color outline_color, float spacing) {
    if (spacing < 0) {
        width  = ray::MeasureTextEx(sdf_font, text.c_str(), font_size, 0).x;
        height = ray::MeasureTextEx(sdf_font, text.c_str(), font_size, 0).y;
    } else {
        width  = ray::MeasureTextEx(sdf_font, text.c_str(), font_size, spacing).x;
        height = ray::MeasureTextEx(sdf_font, text.c_str(), font_size, spacing).y;
    }

    int pad = this->outline_thickness + 2;
    ray::Image img = ray::GenImageColor((int)width + pad*2, (int)height + pad*2, ray::BLANK);

    for (float angle = 0; angle < 2 * PI; angle += (PI/8)) {
        float ox = cosf(angle) * this->outline_thickness;
        float oy = sinf(angle) * this->outline_thickness;
        ray::ImageDrawTextEx(&img, sdf_font, text.c_str(), {pad + ox, pad + oy}, font_size, spacing, outline_color);
    }
    ray::ImageDrawTextEx(&img, sdf_font, text.c_str(), {(float)pad, (float)pad}, font_size, spacing, color);

    texture = ray::LoadTextureFromImage(img);
    ray::SetTextureFilter(texture, ray::TEXTURE_FILTER_BILINEAR);
    this->width = texture.width;
    this->height = texture.height;
    ray::UnloadImage(img);
}

void OutlinedText::create_vertical_text(ray::Color color, ray::Color outline_color, float spacing) {
    float char_height = ray::MeasureTextEx(sdf_font, "A", font_size, spacing).y;
    float max_char_width = 0;

    std::vector<std::string> chars;
    const char* ptr = text.c_str();
    while (*ptr) {
        int cp_size = 0;
        ray::GetCodepointNext(ptr, &cp_size);
        chars.emplace_back(ptr, cp_size);
        ptr += cp_size;
    }

    for (const auto& s : chars) {
        float w = ray::MeasureTextEx(sdf_font, s.c_str(), font_size, spacing).x;
        if (w > max_char_width) max_char_width = w;
    }

    int pad = outline_thickness + 2;
    width  = max_char_width + pad * 2;
    height = (char_height * chars.size()) + pad * 2;

    ray::Image img = ray::GenImageColor((int)width, (int)height, ray::BLANK);

    for (int i = 0; i < (int)chars.size(); i++) {
        const auto& s = chars[i];
        float y = pad + i * char_height;
        float char_width = ray::MeasureTextEx(sdf_font, s.c_str(), font_size, spacing).x;
        float x = pad + (max_char_width - char_width) / 2.0f;

        bool is_rotated = in_rotate_set(s);

        if (is_rotated) {
            int tmp_w = (int)char_width + pad * 2;
            int tmp_h = (int)char_height + pad * 2;

            ray::Image tmp = ray::GenImageColor(tmp_w, tmp_h, ray::BLANK);

            for (float angle = 0; angle < 2 * PI; angle += (PI / 8)) {
                float ox = cosf(angle) * outline_thickness;
                float oy = sinf(angle) * outline_thickness;
                ray::ImageDrawTextEx(&tmp, sdf_font, s.c_str(), {(float)pad + ox, (float)pad + oy}, font_size, spacing, outline_color);
            }
            ray::ImageDrawTextEx(&tmp, sdf_font, s.c_str(), {(float)pad, (float)pad}, font_size, spacing, color);

            ray::ImageRotateCW(&tmp);

            ray::Rectangle src = {0, 0, (float)tmp.width, (float)tmp.height};
            float dx = x + (char_width - tmp.width) / 2.0f;
            float dy = y + (char_height - tmp.height) / 2.0f;
            ray::Rectangle dst = {dx, dy, (float)tmp.width, (float)tmp.height};
            ray::ImageDraw(&img, tmp, src, dst, ray::WHITE);

            ray::UnloadImage(tmp);
        } else {
            for (float angle = 0; angle < 2 * PI; angle += (PI / 8)) {
                float ox = cosf(angle) * outline_thickness;
                float oy = sinf(angle) * outline_thickness;
                ray::ImageDrawTextEx(&img, sdf_font, s.c_str(), {x + ox, y + oy}, font_size, spacing, outline_color);
            }
            ray::ImageDrawTextEx(&img, sdf_font, s.c_str(), {x, y}, font_size, spacing, color);
        }
    }

    texture = ray::LoadTextureFromImage(img);
    this->width = texture.width;
    this->height = texture.height;
    ray::UnloadImage(img);
}

void OutlinedText::draw(const DrawTextureParams& params) {
    ray::Rectangle src = {
        0, 0,
        (float)texture.width,
        (float)texture.height
    };
    int pad = outline_thickness + 2;
    ray::Rectangle dst = {params.x, params.y, (float)texture.width + params.x2, (float)texture.height + params.y2};
    ray::DrawTexturePro(texture, src, dst, {0, 0}, 0.0f, ray::Fade(ray::WHITE, params.fade));
}

FontManager font_manager;
