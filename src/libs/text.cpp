#include "text.h"
#include "han_fold_table.h"
#include "script.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <math.h>

FontManager::FontManager() {}

void FontManager::init(const fs::path& font_path) {
    std::lock_guard<std::mutex> lock(font_mutex);
    if (!exists(font_path)) {
        throw std::runtime_error("Failed to load font: " + font_path.string());
    }
    this->font_path = font_path;
    for (auto& [size, entry] : fonts) { release_font(entry); release_cache(entry); }
    fonts.clear();
    {
        int size = 0;
        unsigned char* data = ray::LoadFileData(font_path.string().c_str(), &size);
        if (!data || size <= 0) {
            if (data) ray::UnloadFileData(data);
            throw std::runtime_error("Failed to read font data: " + font_path.string());
        }
        font_data.assign(data, data + size);
        ray::UnloadFileData(data);
    }

    if (sentinel_texture.id == 0) {
        ray::Image px = ray::GenImageColor(1, 1, ray::BLANK);
        sentinel_texture = ray::LoadTextureFromImage(px);
        ray::UnloadImage(px);
    }
}

void FontManager::unload() {
    std::lock_guard<std::mutex> lock(font_mutex);
    for (auto& [size, entry] : fonts) { release_font(entry); release_cache(entry); }
    fonts.clear();
    if (sentinel_texture.id != 0) {
        ray::UnloadTexture(sentinel_texture);
        sentinel_texture = {};
    }
}

void FontManager::evict_lru(int keep_size) {
    while (fonts.size() > MAX_SIZED_FONTS) {
        auto victim = fonts.end();
        for (auto it = fonts.begin(); it != fonts.end(); ++it) {
            if (it->first == keep_size) continue;
            if (victim == fonts.end() || it->second.last_used < victim->second.last_used)
                victim = it;
        }
        if (victim == fonts.end()) break;
        spdlog::debug("font: evicting {}px atlas ({} codepoints), {} resident",
                      victim->first, victim->second.codepoints.size(), fonts.size());
        release_font(victim->second);
        release_cache(victim->second);
        fonts.erase(victim);
    }
}

// Rasterize only the codepoints not yet in the cache (stb_truetype via LoadFontData).
bool FontManager::rasterize_new(SizedFont& entry, int font_size, const std::vector<int>& cps) {
    if (cps.empty()) return true;
    int count = 0;
    ray::GlyphInfo* g = ray::LoadFontData(font_data.data(), (int)font_data.size(), font_size,
                                          cps.data(), (int)cps.size(), ray::FONT_DEFAULT, &count);
    if (!g) return false;
    std::unordered_set<int> got;
    for (int i = 0; i < count; i++) { entry.cache.push_back(g[i]); got.insert(g[i].value); }   // take ownership of the glyph images
    RL_FREE(g);                                                    // array only; images now live in `cache`
    entry.atlas_dirty = true;

    std::vector<int> missing, alts;
    for (int cp : cps) {
        if (got.count(cp)) continue;
        int alt = (int)han_fold_codepoint((uint32_t)cp);
        if (alt != cp) { missing.push_back(cp); alts.push_back(alt); }
    }
    if (missing.empty()) return true;
    int alt_count = 0;
    ray::GlyphInfo* ag = ray::LoadFontData(font_data.data(), (int)font_data.size(), font_size,
                                           alts.data(), (int)alts.size(), ray::FONT_DEFAULT, &alt_count);
    if (!ag) {
        for (int cp : missing) entry.codepoints.erase(cp);
        return true;
    }
    std::vector<bool> assigned(alts.size(), false);
    for (int i = 0; i < alt_count; i++) {
        bool used = false;
        for (size_t k = 0; k < alts.size(); k++) {
            if (assigned[k] || alts[k] != ag[i].value) continue;
            ray::GlyphInfo gi = ag[i];
            gi.value = missing[k];
            if (used) gi.image = ray::ImageCopy(ag[i].image);   // two originals folding to one form
            entry.cache.push_back(gi);
            used = true;
            assigned[k] = true;
        }
        if (!used) ray::UnloadImage(ag[i].image);
    }
    RL_FREE(ag);
    // Any alt LoadFontData silently dropped never got assigned; retry those too.
    for (size_t k = 0; k < alts.size(); k++)
        if (!assigned[k]) entry.codepoints.erase(missing[k]);
    return true;
}

// Pack the cached glyphs into a fresh atlas texture. Glyph images stay in `cache`
// (raylib's LoadFontEx would crop them out of the atlas; nothing here reads them).
void FontManager::rebuild_atlas(SizedFont& entry, int font_size) {
    release_font(entry);
    const int n = (int)entry.cache.size();
    ray::Font f{};
    f.baseSize     = font_size;
    f.glyphCount   = n;
    f.glyphPadding = 4;   // FONT_TTF_DEFAULT_CHARS_PADDING, same as LoadFontEx
    if (n == 0) { entry.atlas_dirty = false; return; }   // nothing to pack; keep loaded == false
    f.glyphs = (ray::GlyphInfo*)RL_MALLOC(sizeof(ray::GlyphInfo) * n);
    if (!f.glyphs) return;
    for (int i = 0; i < n; i++) f.glyphs[i] = entry.cache[i];
    ray::Image atlas = ray::GenImageFontAtlas(f.glyphs, &f.recs, n, font_size, f.glyphPadding, 0);
    if (!atlas.data || !f.recs) {
        if (atlas.data) ray::UnloadImage(atlas);
        if (f.recs) RL_FREE(f.recs);
        RL_FREE(f.glyphs);
        return;
    }
    f.texture = ray::LoadTextureFromImage(atlas);
    // Like LoadFontEx: the Font's own glyph images are GRAY_ALPHA crops of the atlas
    // (ImageDrawTextEx / OutlinedText composite from them). The cache keeps the raw
    // GRAYSCALE bitmaps GenImageFontAtlas needs for the next repack.
    for (int i = 0; i < n; i++) f.glyphs[i].image = ray::ImageFromImage(atlas, f.recs[i]);
    ray::UnloadImage(atlas);
    ray::SetTextureFilter(f.texture, ray::TEXTURE_FILTER_BILINEAR);
    entry.font = f;
    entry.loaded = true;
    entry.atlas_dirty = false;
}

// Free texture/recs/glyph-struct array of a font built by rebuild_atlas. Never
// UnloadFont(): that would free the glyph images we still own in `cache`.
void FontManager::release_font(SizedFont& entry) {
    if (!entry.loaded) return;
    if (entry.font.texture.id != 0) ray::UnloadTexture(entry.font.texture);
    if (entry.font.glyphs)
        for (int i = 0; i < entry.font.glyphCount; i++) ray::UnloadImage(entry.font.glyphs[i].image);
    if (entry.font.recs)   RL_FREE(entry.font.recs);
    if (entry.font.glyphs) RL_FREE(entry.font.glyphs);
    entry.font = {};
    entry.loaded = false;
}

void FontManager::release_cache(SizedFont& entry) {
    for (auto& g : entry.cache) ray::UnloadImage(g.image);
    entry.cache.clear();
    entry.codepoints.clear();
}

bool FontManager::register_codepoints(SizedFont& entry, int font_size, const std::string& text) {
    std::vector<int> fresh;
    if (entry.codepoints.empty()) {
        // "A" is what OutlinedText measures line height with; keep it (and '?') always present.
        for (int cp : {'A', '?', ' '}) { entry.codepoints.insert(cp); fresh.push_back(cp); }
    }
    const char* ptr = text.c_str();
    while (*ptr) {
        int cp_size = 0;
        int codepoint = ray::GetCodepointNext(ptr, &cp_size);
        if (cp_size <= 0) break;
        if (codepoint > 0 && entry.codepoints.insert(codepoint).second) fresh.push_back(codepoint);
        ptr += cp_size;
    }
    if (fresh.empty()) return false;
    if (!rasterize_new(entry, font_size, fresh)) {
        for (int cp : fresh) entry.codepoints.erase(cp);   // allow a later retry
        spdlog::warn("font: failed to rasterize {} codepoints at {}px", fresh.size(), font_size);
        return false;
    }
    return true;
}

void FontManager::register_text(const std::string& text, int font_size) {
    std::lock_guard<std::mutex> lock(font_mutex);
    if (font_size < 1) font_size = 1;
    register_codepoints(fonts[font_size], font_size, text);
}

FontManager::SizedFont& FontManager::acquire(const std::string& text, int font_size) {
    if (font_size < 1) font_size = 1;

    SizedFont& entry = fonts[font_size];
    register_codepoints(entry, font_size, text);
    if (!entry.loaded || entry.atlas_dirty) rebuild_atlas(entry, font_size);

    entry.last_used = ++use_clock;
    evict_lru(font_size);   // never evicts `font_size` itself, so `entry` stays valid
    return entry;
}

static ray::Color clear_of(ray::Color c) { return ray::Color{c.r, c.g, c.b, 0}; }

static void blit_over(ray::Image* dst, const ray::Image& src, int x0, int y0) {
    if (!dst->data || !src.data) return;
    if (dst->format != ray::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 || src.format != ray::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        ray::Rectangle sr = {0, 0, (float)src.width, (float)src.height};
        ray::Rectangle dr = {(float)x0, (float)y0, (float)src.width, (float)src.height};
        ray::ImageDrawImagePro(dst, src, sr, dr, {0, 0}, 0.0f, ray::WHITE);
        return;
    }
    unsigned char* d = (unsigned char*)dst->data;
    const unsigned char* sp = (const unsigned char*)src.data;
    for (int y = 0; y < src.height; y++) {
        int dy = y0 + y; if (dy < 0 || dy >= dst->height) continue;
        for (int x = 0; x < src.width; x++) {
            int dx = x0 + x; if (dx < 0 || dx >= dst->width) continue;
            const unsigned char* s = sp + (y * src.width + x) * 4;
            unsigned char* o = d + (dy * dst->width + dx) * 4;
            const int sa = s[3];
            if (sa == 0) continue;
            if (sa == 255) { o[0] = s[0]; o[1] = s[1]; o[2] = s[2]; o[3] = 255; continue; }
            const int da = o[3];
            const int oa = sa * 255 + da * (255 - sa);           // out alpha * 255
            for (int c = 0; c < 3; c++) {
                int v = (s[c] * sa * 255 + o[c] * da * (255 - sa) + oa / 2) / (oa ? oa : 1);
                o[c] = (unsigned char)(v > 255 ? 255 : v);
            }
            o[3] = (unsigned char)((oa + 127) / 255);
        }
    }
}

static void draw_text_clean(ray::Image* dst, const ray::Font& font, const char* text, ray::Vector2 pos,
                            float font_size, float spacing, ray::Color tint) {
    ray::Image t = ray::ImageTextEx(font, text, font_size, spacing, ray::WHITE);
    if (t.data && t.width > 0 && t.height > 0) {
        if (t.format != ray::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) ray::ImageFormat(&t, ray::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        unsigned char* px = (unsigned char*)t.data;
        const int n = t.width * t.height;
        for (int i = 0; i < n; i++) {
            px[i * 4 + 0] = tint.r; px[i * 4 + 1] = tint.g; px[i * 4 + 2] = tint.b;
            px[i * 4 + 3] = (unsigned char)((px[i * 4 + 3] * (int)tint.a) / 255);
        }
        blit_over(dst, t, (int)pos.x, (int)pos.y);
    }
    ray::UnloadImage(t);
}

static void stamp_outline(ray::Image* dst, const ray::Font& font, const char* text, ray::Vector2 pos,
                          float font_size, float spacing, ray::Color outline_color, float radius) {
    ray::Image cov = ray::ImageTextEx(font, text, font_size, spacing, ray::WHITE);
    if (!cov.data || cov.width <= 0 || cov.height <= 0) { ray::UnloadImage(cov); return; }
    if (cov.format != ray::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) ray::ImageFormat(&cov, ray::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    const int r  = (int)std::ceil(radius + 0.5f);
    const int W  = cov.width, H = cov.height;
    const int OW = W + 2 * r, OH = H + 2 * r;
    std::vector<unsigned char> a(W * H), out(OW * OH, 0);
    const unsigned char* cp = (const unsigned char*)cov.data;
    for (int i = 0; i < W * H; i++) a[i] = cp[i * 4 + 3];
    // disk weights, 0..255, with the soft 1 px rim; rows carry their non-zero span
    const int D = 2 * r + 1;
    std::vector<unsigned char> wt(D * D, 0);
    std::vector<int> lo(D, D), hi(D, -1);
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            float d = std::sqrt((float)dx * dx + (float)dy * dy);
            float w = radius + 0.5f - d;
            if (w <= 0.0f) continue;
            if (w > 1.0f) w = 1.0f;
            wt[(dy + r) * D + (dx + r)] = (unsigned char)(w * 255.0f + 0.5f);
            if (dx + r < lo[dy + r]) lo[dy + r] = dx + r;
            if (dx + r > hi[dy + r]) hi[dy + r] = dx + r;
        }
    }
    // H*W*D*D byte ops -- fine for HUD text
    for (int y = 0; y < H; y++) {
        const unsigned char* src = &a[y * W];
        for (int x = 0; x < W; x++) {
            const unsigned v = src[x];
            if (!v) continue;
            for (int dy = -r; dy <= r; dy++) {
                const int row = dy + r;
                if (hi[row] < 0) continue;
                unsigned char* orow = &out[(y + dy + r) * OW + x];
                const unsigned char* wrow = &wt[row * D];
                for (int k = lo[row]; k <= hi[row]; k++) {
                    const unsigned char c = (unsigned char)((v * wrow[k] + 127) / 255);
                    unsigned char& o = orow[k];        // ox = x + (k - r) + r = x + k
                    if (o < c) o = c;
                }
            }
        }
    }
    ray::Image o = ray::GenImageColor(OW, OH, ray::Color{outline_color.r, outline_color.g, outline_color.b, 0});
    unsigned char* op = (unsigned char*)o.data;
    for (int i = 0; i < OW * OH; i++) op[i * 4 + 3] = (unsigned char)((out[i] * (int)outline_color.a) / 255);
    blit_over(dst, o, (int)pos.x - r, (int)pos.y - r);
    ray::UnloadImage(o);
    ray::UnloadImage(cov);
}

static ray::Font deep_copy_font(const ray::Font& src) {
    ray::Font dst = src;
    dst.glyphs = nullptr;
    dst.recs   = nullptr;
    dst.glyphCount = 0;
    if (src.glyphCount > 0 && src.glyphs && src.recs) {
        dst.glyphCount = src.glyphCount;
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

// NOTE: the returned Font aliases manager-owned glyphs/recs/texture; it is only valid
// until the next acquire()/register_text() call for any size. Use copy_font() to keep it.
ray::Font FontManager::get_font(const std::string& text, int font_size) {
    std::lock_guard<std::mutex> lock(font_mutex);
    return acquire(text, font_size).font;
}

ray::Font FontManager::copy_font(const std::string& text, int font_size) {
    std::lock_guard<std::mutex> lock(font_mutex);
    ray::Font copy = deep_copy_font(acquire(text, font_size).font);
    copy.texture = sentinel_texture;
    return copy;
}

// Characters placed to the right of the preceding glyph at the same y,
// rather than stacked below it (terminal punctuation, etc.).
static bool is_beside_prev_char(const std::string& s) {
    static const std::unordered_set<std::string> beside_set = {
        ".", ",", "'", "\"",
        "\xe3\x80\x82",  // 。
        "\xe3\x80\x81",  // 、
    };
    return beside_set.count(s) > 0;
}

// Extra downward draw offset for beside chars whose glyphs sit near the top
// of their cell (e.g. apostrophes), so they appear vertically centred.
static float beside_y_offset(const std::string& s, float char_height) {
    if (s == "'" || s == "\"") return char_height * 0.7f;
    return 0.0f;
}

static bool is_small_kana(const std::string& s) {
    static const std::unordered_set<std::string> sutegana = {
        // Small hiragana
        "ぁ", "ぃ", "ぅ", "ぇ", "ぉ", "っ", "ゃ", "ゅ", "ょ", "ゎ", "ゕ", "ゖ",
        // Small katakana
        "ァ", "ィ", "ゥ", "ェ", "ォ", "ッ", "ャ", "ュ", "ョ", "ヮ", "ヵ", "ヶ",
    };
    return sutegana.count(s) > 0;
}

static bool needs_less_spacing_above(const std::string& s) {
    static const std::unordered_set<std::string> alphabet = {
        " ", "a", "c", "e", "g", "m", "n", "o", "p", "q", "r", "s", "u", "v", "w", "x", "y", "z",
    };
    return is_small_kana(s) || alphabet.count(s) > 0;
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
                           float outline_thickness,
                           float spacing,
                           float v_advance)
    : text(std::move(text)),
      font_size(static_cast<float>(font_size)),
      outline_thickness(static_cast<float>(outline_thickness * global_tex.screen_scale)),
      v_advance(v_advance)
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
            if (cp_size <= 0) break;
            std::string s(ptr, cp_size);
            float w = ray::MeasureTextEx(worker_font, s.c_str(), font_size, spacing).x;
            if (w > max_char_width) max_char_width = w;
            ++char_count;
            ptr += cp_size;
        }
        int pad = (int)this->outline_thickness + 2;
        width  = max_char_width + pad * 2;
        height = char_height * (1.0f + (char_count - 1) * v_advance) + pad * 2;
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

#ifdef __EMSCRIPTEN__
    {
        auto data = is_vertical ? build_vertical_text(color, outline_color, spacing)
                                : build_horizontal_text(color, outline_color, spacing);
        pending_image = std::move(data.img);
    }
#else
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
#endif
}

OutlinedText::~OutlinedText() {
    if (build_future.valid())
        build_future.wait();
    {
        std::lock_guard<std::mutex> lock(pending_mutex);
        if (pending_image.has_value()) {
            ray::UnloadImage(pending_image.value());
            pending_image.reset();
        }
    }
    if (worker_font.glyphCount > 0) {
        worker_font.texture = {};
        ray::UnloadFont(worker_font);
    }
    if (texture.has_value()) {
        ray::UnloadTexture(texture.value());
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
        clear_of(outline_thickness > 0 ? outline_color : color));

    for (int li = 0; li < (int)lines.size(); li++) {
        float y = pad + li * line_advance;
        if (outline_thickness > 0)
            stamp_outline(&img, worker_font, lines[li].c_str(), {(float)pad, y}, font_size, sp, outline_color, outline_thickness);
        draw_text_clean(&img, worker_font, lines[li].c_str(),
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
            if (cp_size <= 0) break;
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
        current_y     += advance * v_advance;
        y_positions[i] = current_y;
    }
    int img_h = items.empty()
                ? pad * 2
                : (int)(y_positions.back() + char_height + pad);

    ray::Image img         = ray::GenImageColor(img_w, img_h, clear_of(color));
    ray::Image outline_img = ray::GenImageColor(img_w, img_h, clear_of(outline_color));

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
                        if (outline_thickness > 0)
                            stamp_outline(target, worker_font, s.c_str(), {cx, y},
                                          font_size, spacing, outline_color, outline_thickness);
                    } else {
                        draw_text_clean(target, worker_font, s.c_str(),
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
                    ray::Image tmp = ray::GenImageColor(tmp_w, tmp_h, clear_of(outline_thickness > 0 ? outline_color : color));

                    if (pass == 0) {
                        if (outline_thickness > 0)
                            stamp_outline(&tmp, worker_font, s.c_str(), {(float)pad, (float)pad},
                                          font_size, spacing, outline_color, outline_thickness);
                    } else {
                        draw_text_clean(&tmp, worker_font, s.c_str(),
                                             {(float)pad, (float)pad}, font_size, spacing, color);
                    }
                    ray::ImageRotateCW(&tmp);

                    float dx = x + (char_width  - (float)tmp.width)  / 2.0f;
                    float dy = draw_y + (char_height - (float)tmp.height) / 2.0f;
                    ray::Rectangle dst = {dx, dy, (float)tmp.width, (float)tmp.height};
                    blit_over(target, tmp, (int)dst.x, (int)dst.y);
                    ray::UnloadImage(tmp);
                } else {
                    if (pass == 0) {
                        if (outline_thickness > 0)
                            stamp_outline(target, worker_font, s.c_str(), {x, draw_y},
                                          font_size, spacing, outline_color, outline_thickness);
                    } else {
                        draw_text_clean(target, worker_font, s.c_str(),
                                             {x, draw_y}, font_size, spacing, color);
                    }
                }
            }
        }

        if (pass == 0) {
            blit_over(&img, outline_img, 0, 0);
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

    // the build task still reads worker_font; only release it once the task has finished
    if (build_future.valid()) build_future.wait();
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

    ray::Rectangle src = params.src.value_or(ray::Rectangle{0, 0, (float)texture->width, (float)texture->height});
    float dx = params.x + x_offset;
    float dy = params.y + y_offset;

    if (params.x2 == 0.0f && params.y2 == 0.0f) {
        dx = roundf(dx);
        dy = roundf(dy);
    }

    // dst is sized off src (not the full texture) so a caller can pass a
    // shorter/narrower src to hard-crop the text instead of stretching it.
    float dst_w = src.width  * params.scale + params.x2;
    float dst_h = src.height * params.scale + params.y2;
    ray::Rectangle dst;
    if (params.center) {
        dst = {
            dx + (src.width  - src.width  * params.scale) * 0.5f,
            dy + (src.height - src.height * params.scale) * 0.5f,
            dst_w, dst_h
        };
    } else {
        dst = { dx, dy, dst_w, dst_h };
    }
    if (debug_log_draws) {
        std::string label = text.size() > 40 ? text.substr(0, 40) + "..." : text;
        debug_draw_log.push_back({"\"" + label + "\"", dst});
        log_lua_site(debug_draw_log.back(), script_lua_state());
    }

    ray::DrawTexturePro(*texture, src, dst, {0, 0}, 0.0f,
                        ray::Fade(ray::WHITE, params.fade));
}

FontManager font_manager;
