#include "text.h"
#include "raylib.h"


FontManager::FontManager() {}

void FontManager::init(const fs::path& font_path) {
    std::lock_guard<std::mutex> lock(font_mutex);
    this->font_path = font_path;
    for (int i = 32; i < 127; i++)
        codepoint_cache.insert(i);
    std::vector<int> codepoints(codepoint_cache.begin(), codepoint_cache.end());
    if (!exists(font_path)) {
        throw std::runtime_error("Failed to load font: " + font_path.string());
    }
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

// Characters placed to the right of the preceding glyph at the same y,
// rather than stacked below it (terminal punctuation, etc.).
static bool is_beside_prev_char(const std::string& s) {
    static const std::unordered_set<std::string> beside_set = {
        ".", ",", "'", "\"",
        "\xe3\x80\x82",  // 。
        "\xe3\x80\x81",  // 、
        "\xe3\x83\xbb",  // ・
    };
    return beside_set.count(s) > 0;
}

// Extra downward draw offset for beside chars whose glyphs sit near the top
// of their cell (e.g. apostrophes), so they appear vertically centred.
static float beside_y_offset(const std::string& s, float char_height) {
    if (s == "'" || s == "\"") return char_height * 0.7f;
    return 0.0f;
}

static bool needs_less_spacing_above(const std::string& s) {
    static const std::unordered_set<std::string> alphabet = {
        " ", "a", "c", "e", "g", "m", "n", "o", "p", "q", "r", "s", "u", "v", "w", "x", "y", "z",
    };
    static const std::unordered_set<std::string> sutegana = {
        // Small hiragana
        "ぁ", "ぃ", "ぅ", "ぇ", "ぉ", "っ", "ゃ", "ゅ", "ょ", "ゎ", "ゕ", "ゖ",
        // Small katakana
        "ァ", "ィ", "ゥ", "ェ", "ォ", "ッ", "ャ", "ュ", "ョ", "ヮ", "ヵ", "ヶ",
    };
    return sutegana.count(s) > 0 || alphabet.count(s) > 0;
}

// Consecutive runs of 2+ of these are drawn horizontally side-by-side.
static bool is_hgroup_char(const std::string& s) {
    static const std::unordered_set<std::string> hgroup_set = {
        "!", "?",
        "\xef\xbc\x81",  // ！
        "\xef\xbc\x9f",  // ？
        "\xe2\x80\xa0", // chaos time the dark
    };
    return hgroup_set.count(s) > 0;
}

static std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0, pos;
    while ((pos = text.find('\n', start)) != std::string::npos) {
        lines.push_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    lines.push_back(text.substr(start));
    return lines;
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
      outline_thickness(static_cast<float>(outline_thickness * global_tex.screen_scale))
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
        auto lines = split_lines(this->text);
        float line_h      = ray::MeasureTextEx(worker_font, "A", font_size, sp).y;
        float line_advance = line_h + this->outline_thickness * 2;
        float max_w  = 0.0f;
        for (const auto& line : lines) {
            float lw = ray::MeasureTextEx(worker_font, line.c_str(), font_size, sp).x;
            if (lw > max_w) max_w = lw;
        }
        width  = max_w + pad * 2;
        height = line_advance * (float)(lines.size() - 1) + line_h + pad * 2;
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
    auto lines = split_lines(text);
    float line_h       = ray::MeasureTextEx(worker_font, "A", font_size, sp).y;
    float line_advance = line_h + outline_thickness * 2;
    float max_w  = 0.0f;
    for (const auto& line : lines)  {
        float lw = ray::MeasureTextEx(worker_font, line.c_str(), font_size, sp).x;
        if (lw > max_w) max_w = lw;
    }

    int pad = (int)outline_thickness + 2;
    ray::Image img = ray::GenImageColor(
        (int)max_w + pad * 2,
        (int)(line_advance * (float)(lines.size() - 1) + line_h) + pad * 2,
        ray::BLANK);

    for (int li = 0; li < (int)lines.size(); li++) {
        float y = pad + li * line_advance;
        for (float angle = 0; angle < 2 * PI; angle += (PI / 8)) {
            float ox = cosf(angle) * outline_thickness;
            float oy = sinf(angle) * outline_thickness;
            ray::ImageDrawTextEx(&img, worker_font, lines[li].c_str(),
                                 {pad + ox, y + oy}, font_size, sp, outline_color);
        }
        ray::ImageDrawTextEx(&img, worker_font, lines[li].c_str(),
                             {(float)pad, y}, font_size, sp, color);
    }

    return {img};
}

OutlinedText::BuildData OutlinedText::build_vertical_text(
    ray::Color color, ray::Color outline_color, float spacing)
{
    float char_height = ray::MeasureTextEx(worker_font, "A", font_size, spacing).y;

    // Parse the text into items. Consecutive runs of 2+ hgroup chars (!, ?)
    // become a single item drawn horizontally; everything else is one item.
    struct TextItem {
        std::vector<std::string> chars;
        bool  is_hgroup;
        float width;  // total render width
    };

    std::vector<std::string> raw_chars;
    {
        const char* ptr = text.c_str();
        while (*ptr) {
            int cp_size = 0;
            ray::GetCodepointNext(ptr, &cp_size);
            raw_chars.emplace_back(ptr, cp_size);
            ptr += cp_size;
        }
    }

    std::vector<TextItem> items;
    for (int i = 0; i < (int)raw_chars.size(); ) {
        if (is_hgroup_char(raw_chars[i])) {
            int j = i;
            while (j < (int)raw_chars.size() && is_hgroup_char(raw_chars[j])) j++;
            TextItem item;
            item.is_hgroup = (j - i >= 2);
            item.chars.assign(raw_chars.begin() + i, raw_chars.begin() + j);
            item.width = 0.0f;
            for (const auto& s : item.chars)
                item.width += ray::MeasureTextEx(worker_font, s.c_str(), font_size, spacing).x;
            items.push_back(std::move(item));
            i = j;
        } else {
            TextItem item;
            item.is_hgroup = false;
            item.chars     = { raw_chars[i] };
            item.width     = ray::MeasureTextEx(worker_font, raw_chars[i].c_str(), font_size, spacing).x;
            items.push_back(std::move(item));
            ++i;
        }
    }

    float max_item_width = 0.0f;
    for (const auto& item : items)
        if (item.width > max_item_width) max_item_width = item.width;

    int pad   = (int)outline_thickness + 2;
    int img_w = (int)(max_item_width + pad * 2);

    // Pre-compute y position for each item.
    static constexpr float small_char_advance_factor = 0.8f;
    static constexpr float beside_advance_factor     = 0.3f;
    std::vector<float> y_positions(items.size(), (float)pad);
    float current_y = (float)pad;
    for (int i = 1; i < (int)items.size(); i++) {
        float advance;
        const auto& first = items[i].chars[0];
        if (items[i].is_hgroup)
            advance = char_height;
        else if (is_beside_prev_char(first))
            advance = char_height * beside_advance_factor;
        else if (needs_less_spacing_above(first))
            advance = char_height * small_char_advance_factor;
        else
            advance = char_height;
        current_y     += advance;
        y_positions[i] = current_y;
    }
    int img_h = items.empty()
                ? pad * 2
                : (int)(y_positions.back() + char_height + pad);

    ray::Image img         = ray::GenImageColor(img_w, img_h, ray::BLANK);
    ray::Image outline_img = ray::GenImageColor(img_w, img_h, ray::BLANK);

    // Two passes: 0 = outlines onto outline_img, 1 = fills onto img.
    for (int pass = 0; pass < 2; pass++) {
        ray::Image* target = (pass == 0) ? &outline_img : &img;

        for (int i = 0; i < (int)items.size(); i++) {
            const auto& item = items[i];
            float y = y_positions[i];

            if (item.is_hgroup) {
                // Chars drawn left-to-right, group centred in max_item_width.
                float cx = pad + (max_item_width - item.width) / 2.0f;
                for (const auto& s : item.chars) {
                    float cw = ray::MeasureTextEx(worker_font, s.c_str(), font_size, spacing).x;
                    if (pass == 0) {
                        for (float angle = 0; angle < 2 * PI; angle += (PI / 8)) {
                            float ox = cosf(angle) * outline_thickness;
                            float oy = sinf(angle) * outline_thickness;
                            ray::ImageDrawTextEx(target, worker_font, s.c_str(),
                                                 {cx + ox, y + oy}, font_size, spacing, outline_color);
                        }
                    } else {
                        ray::ImageDrawTextEx(target, worker_font, s.c_str(),
                                             {cx, y}, font_size, spacing, color);
                    }
                    cx += cw;
                }
            } else {
                const auto& s    = item.chars[0];
                float char_width = item.width;
                float x = is_beside_prev_char(s)
                          ? pad + (max_item_width - char_width)
                          : pad + (max_item_width - char_width) / 2.0f;
                float draw_y = y + beside_y_offset(s, char_height);

                if (!is_beside_prev_char(s) && in_rotate_set(s)) {
                    int tmp_w = (int)char_width + pad * 2;
                    int tmp_h = (int)char_height + pad * 2;
                    ray::Image tmp = ray::GenImageColor(tmp_w, tmp_h, ray::BLANK);

                    if (pass == 0) {
                        for (float angle = 0; angle < 2 * PI; angle += (PI / 8)) {
                            float ox = cosf(angle) * outline_thickness;
                            float oy = sinf(angle) * outline_thickness;
                            ray::ImageDrawTextEx(&tmp, worker_font, s.c_str(),
                                                 {(float)pad + ox, (float)pad + oy},
                                                 font_size, spacing, outline_color);
                        }
                    } else {
                        ray::ImageDrawTextEx(&tmp, worker_font, s.c_str(),
                                             {(float)pad, (float)pad}, font_size, spacing, color);
                    }
                    ray::ImageRotateCW(&tmp);

                    float dx = x + (char_width  - (float)tmp.width)  / 2.0f;
                    float dy = draw_y + (char_height - (float)tmp.height) / 2.0f;
                    ray::Rectangle src = {0, 0, (float)tmp.width, (float)tmp.height};
                    ray::Rectangle dst = {dx, dy, (float)tmp.width, (float)tmp.height};
                    ray::ImageDraw(target, tmp, src, dst, ray::WHITE);
                    ray::UnloadImage(tmp);
                } else {
                    if (pass == 0) {
                        for (float angle = 0; angle < 2 * PI; angle += (PI / 8)) {
                            float ox = cosf(angle) * outline_thickness;
                            float oy = sinf(angle) * outline_thickness;
                            ray::ImageDrawTextEx(target, worker_font, s.c_str(),
                                                 {x + ox, draw_y + oy}, font_size, spacing, outline_color);
                        }
                    } else {
                        ray::ImageDrawTextEx(target, worker_font, s.c_str(),
                                             {x, draw_y}, font_size, spacing, color);
                    }
                }
            }
        }

        if (pass == 0) {
            ray::ImageDraw(&img, outline_img,
                           {0, 0, (float)img_w, (float)img_h},
                           {0, 0, (float)img_w, (float)img_h}, ray::WHITE);
            ray::UnloadImage(outline_img);
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
