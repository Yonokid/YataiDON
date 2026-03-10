#include "text.h"
#include "raylib.h"


FontManager::FontManager() {}

void FontManager::init(const fs::path& font_path) {
    std::lock_guard<std::mutex> lock(font_mutex);
    this->font_path = font_path;
    for (int i = 32; i < 127; i++)
        codepoint_cache.insert(i);
    std::vector<int> codepoints(codepoint_cache.begin(), codepoint_cache.end());
    font = ray::LoadFontEx(font_path.string().c_str(), max_font_size,
                           codepoints.data(), (int)codepoints.size());
    ray::SetTextureFilter(font.texture, ray::TEXTURE_FILTER_BILINEAR);
}

static ray::Font deep_copy_font(const ray::Font& src) {
    ray::Font dst = src;
    if (src.glyphCount > 0) {
        dst.glyphs = (ray::GlyphInfo*)RL_MALLOC(src.glyphCount * sizeof(ray::GlyphInfo));
        memcpy(dst.glyphs, src.glyphs, src.glyphCount * sizeof(ray::GlyphInfo));
        for (int i = 0; i < src.glyphCount; i++) {
            const ray::Image& si = src.glyphs[i].image;
            ray::Image& di = dst.glyphs[i].image;
            if (si.data && si.width > 0 && si.height > 0) {
                int dataSize = ray::GetPixelDataSize(si.width, si.height, si.format);
                di.data = RL_MALLOC(dataSize);
                memcpy(di.data, si.data, dataSize);
            } else {
                di.data = nullptr;
            }
        }
        dst.recs = (ray::Rectangle*)RL_MALLOC(src.glyphCount * sizeof(ray::Rectangle));
        memcpy(dst.recs, src.recs, src.glyphCount * sizeof(ray::Rectangle));
    }
    return dst;
}

ray::Font FontManager::get_font(const std::string& text, int font_size) {
    std::lock_guard<std::mutex> lock(font_mutex);

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
        font = ray::LoadFontEx(font_path.string().c_str(), max_font_size,
                               codepoints.data(), (int)codepoints.size());
        ray::SetTextureFilter(font.texture, ray::TEXTURE_FILTER_BILINEAR);
    }

    return font;
}

ray::Font FontManager::copy_font(const std::string& text, int font_size) {
    std::lock_guard<std::mutex> lock(font_mutex);

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
        font = ray::LoadFontEx(font_path.string().c_str(), max_font_size,
                               codepoints.data(), (int)codepoints.size());
        ray::SetTextureFilter(font.texture, ray::TEXTURE_FILTER_BILINEAR);
    }

    return deep_copy_font(font);
}

static bool in_rotate_set(const std::string& s) {
    static const std::unordered_set<std::string> rotate_set = {
        "-", "\xe2\x80\x90",
        "|",
        "/", "\\",
        "\xe3\x83\xbc",
        "\xef\xbd\x9e",
        "~",
        "\xef\xbc\x88", "\xef\xbc\x89",
        "(", ")",
        "\xe3\x80\x8c", "\xe3\x80\x8d",
        "[", "]",
        "\xef\xbc\xb3", "\xef\xbc\xb4",
        "\xe3\x80\x90", "\xe3\x80\x91",
        "\xe2\x80\xa6",
        "\xe2\x86\x92",
        ":", "\xef\xbc\x9a",
    };
    return rotate_set.count(s) > 0;
}


OutlinedText::OutlinedText(std::string text, int font_size,
                           ray::Color color, ray::Color outline_color,
                           bool is_vertical,
                           int outline_thickness,
                           float spacing)
    : text(std::move(text)),
      font_size(static_cast<float>(font_size)),
      outline_thickness(static_cast<float>(outline_thickness))
{
    worker_font = font_manager.copy_font(this->text, font_size);

    if (is_vertical) {
        float char_height    = ray::MeasureTextEx(worker_font, "A", font_size, spacing).y;
        float max_char_width = 0.0f;
        int   char_count     = 0;
        const char* ptr = this->text.c_str();
        while (*ptr) {
            int cp_size = 0;
            ray::GetCodepointNext(ptr, &cp_size);
            std::string s(ptr, cp_size);
            float w = ray::MeasureTextEx(worker_font, s.c_str(), font_size, spacing).x;
            if (w > max_char_width) max_char_width = w;
            ++char_count;
            ptr += cp_size;
        }
        int pad = (int)this->outline_thickness + 2;
        width  = max_char_width + pad * 2;
        height = char_height * char_count + pad * 2;
    } else {
        float sp = (spacing < 0) ? 0.0f : spacing;
        int pad  = (int)this->outline_thickness + 2;
        width    = ray::MeasureTextEx(worker_font, this->text.c_str(), font_size, sp).x + pad * 2;
        height   = ray::MeasureTextEx(worker_font, this->text.c_str(), font_size, sp).y + pad * 2;
    }

    if (is_vertical) {
        build_future = std::async(std::launch::async,
            [this, color, outline_color, spacing]() {
                auto data = build_vertical_text(color, outline_color, spacing);
                std::lock_guard<std::mutex> lock(pending_mutex);
                pending_image = std::move(data.img);
            });
    } else {
        build_future = std::async(std::launch::async,
            [this, color, outline_color, spacing]() {
                auto data = build_horizontal_text(color, outline_color, spacing);
                std::lock_guard<std::mutex> lock(pending_mutex);
                pending_image = std::move(data.img);
            });
    }
}

OutlinedText::BuildData OutlinedText::build_horizontal_text(
    ray::Color color, ray::Color outline_color, float spacing)
{
    float sp = (spacing < 0) ? 0.0f : spacing;
    float w  = ray::MeasureTextEx(worker_font, text.c_str(), font_size, sp).x;
    float h  = ray::MeasureTextEx(worker_font, text.c_str(), font_size, sp).y;

    int pad = (int)outline_thickness + 2;
    ray::Image img = ray::GenImageColor((int)w + pad * 2, (int)h + pad * 2, ray::BLANK);

    for (float angle = 0; angle < 2 * PI; angle += (PI / 8)) {
        float ox = cosf(angle) * outline_thickness;
        float oy = sinf(angle) * outline_thickness;
        ray::ImageDrawTextEx(&img, worker_font, text.c_str(),
                             {pad + ox, pad + oy}, font_size, spacing, outline_color);
    }
    ray::ImageDrawTextEx(&img, worker_font, text.c_str(),
                         {(float)pad, (float)pad}, font_size, spacing, color);

    return {img};
}

OutlinedText::BuildData OutlinedText::build_vertical_text(
    ray::Color color, ray::Color outline_color, float spacing)
{
    float char_height    = ray::MeasureTextEx(worker_font, "A", font_size, spacing).y;
    float max_char_width = 0.0f;

    std::vector<std::string> chars;
    const char* ptr = text.c_str();
    while (*ptr) {
        int cp_size = 0;
        ray::GetCodepointNext(ptr, &cp_size);
        chars.emplace_back(ptr, cp_size);
        ptr += cp_size;
    }
    for (const auto& s : chars) {
        float cw = ray::MeasureTextEx(worker_font, s.c_str(), font_size, spacing).x;
        if (cw > max_char_width) max_char_width = cw;
    }

    int pad   = (int)outline_thickness + 2;
    int img_w = (int)(max_char_width + pad * 2);
    int img_h = (int)(char_height * (float)chars.size() + pad * 2);

    ray::Image img = ray::GenImageColor(img_w, img_h, ray::BLANK);

    for (int i = 0; i < (int)chars.size(); i++) {
        const auto& s = chars[i];
        float y          = pad + i * char_height;
        float char_width = ray::MeasureTextEx(worker_font, s.c_str(), font_size, spacing).x;
        float x          = pad + (max_char_width - char_width) / 2.0f;

        if (in_rotate_set(s)) {
            int tmp_w = (int)char_width + pad * 2;
            int tmp_h = (int)char_height + pad * 2;
            ray::Image tmp = ray::GenImageColor(tmp_w, tmp_h, ray::BLANK);

            for (float angle = 0; angle < 2 * PI; angle += (PI / 8)) {
                float ox = cosf(angle) * outline_thickness;
                float oy = sinf(angle) * outline_thickness;
                ray::ImageDrawTextEx(&tmp, worker_font, s.c_str(),
                                     {(float)pad + ox, (float)pad + oy},
                                     font_size, spacing, outline_color);
            }
            ray::ImageDrawTextEx(&tmp, worker_font, s.c_str(),
                                 {(float)pad, (float)pad}, font_size, spacing, color);
            ray::ImageRotateCW(&tmp);

            float dx = x + (char_width  - (float)tmp.width)  / 2.0f;
            float dy = y + (char_height - (float)tmp.height) / 2.0f;
            ray::Rectangle src = {0, 0, (float)tmp.width, (float)tmp.height};
            ray::Rectangle dst = {dx, dy, (float)tmp.width, (float)tmp.height};
            ray::ImageDraw(&img, tmp, src, dst, ray::WHITE);
            ray::UnloadImage(tmp);
        } else {
            for (float angle = 0; angle < 2 * PI; angle += (PI / 8)) {
                float ox = cosf(angle) * outline_thickness;
                float oy = sinf(angle) * outline_thickness;
                ray::ImageDrawTextEx(&img, worker_font, s.c_str(),
                                     {x + ox, y + oy}, font_size, spacing, outline_color);
            }
            ray::ImageDrawTextEx(&img, worker_font, s.c_str(),
                                 {x, y}, font_size, spacing, color);
        }
    }

    return {img};
}

bool OutlinedText::upload_pending() {
    std::optional<ray::Image> img;
    {
        std::lock_guard<std::mutex> lock(pending_mutex);
        if (!pending_image.has_value()) return false;
        img = std::move(pending_image);
        pending_image.reset();
    }

    worker_font.texture = {};
    ray::UnloadFont(worker_font);
    worker_font = {};

    ray::Texture tex = ray::LoadTextureFromImage(*img);
    ray::SetTextureFilter(tex, ray::TEXTURE_FILTER_BILINEAR);
    ray::UnloadImage(*img);

    texture = tex;
    width   = (float)tex.width;
    height  = (float)tex.height;
    return true;
}

void OutlinedText::finish() {
    if (build_future.valid())
        build_future.get();
    upload_pending();
}

void OutlinedText::draw(const DrawTextureParams& params) {
    upload_pending();

    if (!texture.has_value()) return;

    ray::Rectangle src = {0, 0, (float)texture->width, (float)texture->height};
    ray::Rectangle dst = {
        params.x,
        params.y,
        (float)texture->width  + params.x2,
        (float)texture->height + params.y2
    };
    ray::DrawTexturePro(*texture, src, dst, {0, 0}, 0.0f,
                        ray::Fade(ray::WHITE, params.fade));
}

FontManager font_manager;
