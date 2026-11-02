// font code (stb_truetype) ported from raylib 6.2's rtext.c

#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>

#include "../gfx.h"
#include "glyph_lookup.h"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

static constexpr int TTF_GLYPH_PADDING = 4; // raylib FONT_TTF_DEFAULT_CHARS_PADDING
static constexpr int TEXT_LINE_SPACING = 2; // raylib's default textLineSpacing

std::vector<GlyphInfo> load_font_data(std::span<const unsigned char> data, int font_size,
                                      std::span<const int> codepoints) {
    std::vector<GlyphInfo> glyphs;
    stbtt_fontinfo info{};
    if (data.empty() || !stbtt_InitFont(&info, data.data(), 0)) {
        spdlog::warn("FONT: Failed to process TTF font data");
        return glyphs;
    }
    const float scale = stbtt_ScaleForPixelHeight(&info, font_size);
    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    for (int cp : codepoints) {
        if (stbtt_FindGlyphIndex(&info, cp) <= 0) {
            continue;
        }
        GlyphInfo& g = glyphs.emplace_back();
        g.value = cp;
        stbtt_GetCodepointHMetrics(&info, cp, &g.advanceX, nullptr);
        g.advanceX = static_cast<int>(g.advanceX * scale);
        // Blank glyphs (spaces) get no bitmap: width/height stay 0, only advanceX matters.
        g.image.data =
            stbtt_GetCodepointBitmap(&info, scale, scale, cp, &g.image.width, &g.image.height, &g.offsetX, &g.offsetY);
        g.image.mipmaps = 1;
        g.image.format = pixel_format::GRAYSCALE;
        g.offsetY += static_cast<int>(ascent * scale);
    }
    return glyphs;
}

// raylib's basic (packMethod 0) row packer.
Image gen_image_font_atlas(const GlyphInfo* glyphs, Rect** recs_out, int glyph_count, int font_size, int padding) {
    Image atlas;
    *recs_out = nullptr;
    if (!glyphs) {
        return atlas;
    }
    glyph_count = glyph_count > 0 ? glyph_count : 95;
    auto* recs = static_cast<Rect*>(std::malloc(glyph_count * sizeof(Rect)));

    int total_width = 0;
    for (int i = 0; i < glyph_count; i++) {
        total_width += glyphs[i].image.width + 2 * padding;
    }
    const float total_area = total_width * (font_size + 2 * padding) * 1.2f;
    const int size = static_cast<int>(powf(2, ceilf(logf(sqrtf(total_area)) / std::numbers::ln2_v<float>)));
    atlas.width = size;
    atlas.height = total_area < (size * size) / 2 ? size / 2 : size;
    int data_size = atlas.width * atlas.height;
    auto* gray = static_cast<unsigned char*>(std::calloc(data_size, 1));

    int ox = padding, oy = padding;
    for (int i = 0; i < glyph_count; i++) {
        const Image& im = glyphs[i].image;
        if (ox >= atlas.width - im.width - 2 * padding) {
            ox = padding;
            oy += font_size + 2 * padding;
            if (oy > atlas.height - font_size - padding) {
                auto* grown = static_cast<unsigned char*>(std::calloc(atlas.width * atlas.height * 2, 1));
                std::memcpy(grown, gray, data_size);
                std::free(gray);
                gray = grown;
                atlas.height *= 2;
                data_size = atlas.width * atlas.height;
            }
        }
        for (int y = 0; y < im.height; y++) {
            for (int x = 0; x < im.width; x++) {
                const int dx = ox + x, dy = oy + y;
                if (dx >= 0 && dx < atlas.width && dy >= 0 && dy < atlas.height) {
                    gray[dy * atlas.width + dx] = static_cast<const unsigned char*>(im.data)[y * im.width + x];
                }
            }
        }
        recs[i] = {static_cast<float>(ox), static_cast<float>(oy), static_cast<float>(im.width),
                   static_cast<float>(im.height)};
        ox += im.width + 2 * padding;
    }

    auto* gray_alpha = static_cast<unsigned char*>(std::malloc(data_size * 2));
    for (int i = 0; i < data_size; i++) {
        gray_alpha[i * 2] = 255;
        gray_alpha[i * 2 + 1] = gray[i];
    }
    std::free(gray);
    atlas.data = gray_alpha;
    atlas.mipmaps = 1;
    atlas.format = pixel_format::GRAY_ALPHA;
    *recs_out = recs;
    return atlas;
}

extern const unsigned char default_font_ttf_data[];
extern const size_t default_font_ttf_data_size;

std::span<const unsigned char> default_font_ttf() {
    return {default_font_ttf_data, default_font_ttf_data_size};
}

Font load_font(const char* path, int font_size) {
    std::vector<unsigned char> data = load_file_bytes(path);
    if (data.empty()) {
        spdlog::warn("FONT: [{}] no glyphs loaded, reverted to default font", path);
        return get_font_default();
    }
    return load_font_from_memory(data, font_size);
}

Font load_font_from_memory(std::span<const unsigned char> data, int font_size) {
    static constexpr auto ascii = [] {
        std::array<int, 95> cps{};
        for (int i = 0; i < 95; i++) {
            cps[i] = 32 + i;
        }
        return cps;
    }();
    std::vector<GlyphInfo> glyphs = load_font_data(data, font_size, ascii);
    if (glyphs.empty()) {
        spdlog::warn("FONT: no glyphs loaded, reverted to default font");
        return get_font_default();
    }
    Font font;
    font.baseSize = font_size;
    font.glyphCount = static_cast<int>(glyphs.size());
    font.glyphPadding = TTF_GLYPH_PADDING;
    font.glyphs = static_cast<GlyphInfo*>(std::malloc(glyphs.size() * sizeof(GlyphInfo)));
    std::memcpy(font.glyphs, glyphs.data(), glyphs.size() * sizeof(GlyphInfo));
    Image atlas = gen_image_font_atlas(font.glyphs, &font.recs, font.glyphCount, font.baseSize, font.glyphPadding);
    font.texture = load_texture_from_image(atlas);
    unload_image(atlas);
    return font;
}

void unload_font(Font font) {
    if (font.texture.id != 0 && font.texture.id == get_font_default().texture.id) {
        return;
    }
    for (int i = 0; font.glyphs && i < font.glyphCount; i++) {
        unload_image(font.glyphs[i].image);
    }
    std::free(font.glyphs);
    std::free(font.recs);
    if (font.texture.id != 0) { // CPU-only copies (FontManager::copy_font) have no texture
        unload_texture(font.texture);
    }
}

int get_glyph_index(const Font& font, int codepoint) {
    if (font.baseSize <= 0 || font.glyphCount <= 0 || !font.recs || !font.glyphs) { // raylib IsFontValid
        return 0;
    }
    int fallback = 0; // '?'
    for (int i = 0; i < font.glyphCount; i++) {
        if (font.glyphs[i].value == 63) {
            fallback = i;
        }
        if (font.glyphs[i].value == codepoint) {
            return i;
        }
    }
    return font.glyphs[0].value == codepoint ? 0 : fallback;
}

int get_codepoint_next(const char* text, int* codepoint_size) {
    const auto* p = reinterpret_cast<const unsigned char*>(text);
    *codepoint_size = 1;
    if (!text) {
        return '?';
    }
    auto cont = [&](int i) { return (p[i] & 0xC0) == 0x80; };
    if ((p[0] & 0xF8) == 0xF0) {
        if (!cont(1) || !cont(2) || !cont(3)) {
            return '?';
        }
        *codepoint_size = 4;
        return ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    }
    if ((p[0] & 0xF0) == 0xE0) {
        if (!cont(1) || !cont(2)) {
            return '?';
        }
        *codepoint_size = 3;
        return ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
    }
    if ((p[0] & 0xE0) == 0xC0) {
        if (!cont(1)) {
            return '?';
        }
        *codepoint_size = 2;
        return ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    }
    return (p[0] & 0x80) == 0 ? p[0] : '?';
}

const char* codepoint_to_utf8(int codepoint, int* utf8_size) {
    static char utf8[6];
    std::memset(utf8, 0, sizeof(utf8));
    int size = 0;
    if (codepoint <= 0x7f) {
        utf8[0] = static_cast<char>(codepoint);
        size = 1;
    } else if (codepoint <= 0x7ff) {
        utf8[0] = static_cast<char>(((codepoint >> 6) & 0x1f) | 0xc0);
        utf8[1] = static_cast<char>((codepoint & 0x3f) | 0x80);
        size = 2;
    } else if (codepoint <= 0xffff) {
        utf8[0] = static_cast<char>(((codepoint >> 12) & 0x0f) | 0xe0);
        utf8[1] = static_cast<char>(((codepoint >> 6) & 0x3f) | 0x80);
        utf8[2] = static_cast<char>((codepoint & 0x3f) | 0x80);
        size = 3;
    } else if (codepoint <= 0x10ffff) {
        utf8[0] = static_cast<char>(((codepoint >> 18) & 0x07) | 0xf0);
        utf8[1] = static_cast<char>(((codepoint >> 12) & 0x3f) | 0x80);
        utf8[2] = static_cast<char>(((codepoint >> 6) & 0x3f) | 0x80);
        utf8[3] = static_cast<char>((codepoint & 0x3f) | 0x80);
        size = 4;
    }
    *utf8_size = size;
    return utf8;
}

Vec2 measure_text_ex(const Font& font, const char* text, float font_size, float spacing) {
    if (!font.glyphs || !font.recs || font.baseSize <= 0 || !text || !text[0]) {
        return {};
    }
    const int size = static_cast<int>(std::strlen(text));
    int max_bytes = 0, bytes = 0;
    float width = 0.0f, max_width = 0.0f, height = font_size;
    GlyphLookup glyph_lookup(font, text);
    for (int i = 0; i < size;) {
        bytes++;
        int n = 0;
        const int cp = get_codepoint_next(&text[i], &n);
        i += n;
        if (cp != '\n') {
            const int gi = glyph_lookup.index(cp);
            width +=
                font.glyphs[gi].advanceX > 0 ? font.glyphs[gi].advanceX : font.recs[gi].width + font.glyphs[gi].offsetX;
        } else {
            max_width = width > max_width ? width : max_width;
            bytes = 0;
            width = 0;
            height += font_size + TEXT_LINE_SPACING;
        }
        max_bytes = bytes > max_bytes ? bytes : max_bytes;
    }
    max_width = width > max_width ? width : max_width;
    return {max_width * (font_size / font.baseSize) + ((max_bytes - 1) * spacing), height};
}
