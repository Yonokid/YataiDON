#include "text.h"

#include <cmath>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <vector>

#include "../../platform/platform_windows.h"
#include "../file/filesystem.h"
#include "../file/script.h"
#include "../gfx.h"
#include "../global_data.h"
#include "han_fold_table.h"
#if !defined(_WIN32) && defined(YATAIDON_HAVE_ICONV)
#include <iconv.h>
#endif

FontManager::FontManager() = default;

void FontManager::init(const fs::path& font_path) {
    std::scoped_lock lock(font_mutex);
    if (!exists(font_path)) {
        throw std::runtime_error("Failed to load font: " + font_path.string());
    }
    this->font_path = font_path;
    for (auto& [size, entry] : fonts) {
        release_font(entry);
        release_cache(entry);
    }
    fonts.clear();
    font_data = load_file_bytes(font_path.string().c_str());
    if (font_data.empty()) {
        throw std::runtime_error("Failed to read font data: " + font_path.string());
    }
}

static std::unordered_map<std::string, SkinInfo> text_entries;

static double json_member(const rapidjson::Value& o, const char* key, double fallback) {
    return o.HasMember(key) && o[key].IsNumber() ? o[key].GetDouble() : fallback;
}

static void read_text_entries(const fs::path& file, float scale) {
    if (!fs::exists(file)) {
        return;
    }
    try {
        auto doc = read_json_file(file);
        if (!doc.IsObject()) {
            return;
        }
        for (auto& m : doc.GetObject()) {
            const auto& v = m.value;
            if (!v.IsObject() || !v.HasMember("text") || !v["text"].IsObject()) {
                continue;
            }
            std::map<std::string, std::string> labels;
            for (const auto& t : v["text"].GetObject()) {
                if (t.value.IsString()) {
                    labels[t.name.GetString()] = t.value.GetString();
                }
            }
            text_entries[m.name.GetString()] = SkinInfo(static_cast<float>(json_member(v, "x", 0.0)) * scale,
                                                        static_cast<float>(json_member(v, "y", 0.0)) * scale,
                                                        static_cast<int>(json_member(v, "font_size", 0) * scale),
                                                        static_cast<float>(json_member(v, "width", 0.0)) * scale,
                                                        static_cast<float>(json_member(v, "height", 0.0)) * scale,
                                                        labels, json_member(v, "outline", -1.0));
        }
    } catch (const std::exception& e) {
        spdlog::warn("text_config {}: {}", file.string(), e.what());
    }
}

void FontManager::load_text_config(const fs::path& skin_root) {
    text_entries.clear();
    std::vector<fs::path> roots = skin_ancestor_roots(); // nearest first
    roots.insert(roots.begin(), skin_root);
    for (size_t i = roots.size(); i-- > 0;) { // farthest first so nearer skins override
        float scale = 1.0f;
        if (i > 0) {
            float aw = 1280.0f;
            try {
                auto cfg = read_json_file(roots[i] / "Graphics" / "skin_config.json");
                if (cfg.HasMember("screen") && cfg["screen"].HasMember("width") && cfg["screen"]["width"].IsNumber()) {
                    aw = cfg["screen"]["width"].GetFloat();
                }
            } catch (const std::exception&) {
            }
            scale = static_cast<float>(tex.screen_width) / aw;
        }
        read_text_entries(roots[i] / "Graphics" / "skin_config.json", scale);
        read_text_entries(roots[i] / "text_config.json", scale);
    }
}

const SkinInfo* FontManager::get_text_info(const std::string& key) {
    auto it = text_entries.find(key);
    return it == text_entries.end() ? nullptr : &it->second;
}

std::string FontManager::get_text_label(const std::string& key) {
    auto it = text_entries.find(key);
    if (it == text_entries.end()) {
        spdlog::warn("text_config: missing key '{}'", key);
        return "";
    }
    return localized_text(it->second.text, global_data.config->general.language,
                          LocalizedTextFallback::ENGLISH_OR_FIRST);
}

void FontManager::unload() {
    std::scoped_lock lock(font_mutex);
    for (auto& [size, entry] : fonts) {
        release_font(entry);
        release_cache(entry);
    }
    fonts.clear();
}

void FontManager::evict_lru(int keep_size) {
    while (fonts.size() > MAX_SIZED_FONTS) {
        auto victim = fonts.end();
        for (auto it = fonts.begin(); it != fonts.end(); ++it) {
            if (it->first == keep_size) {
                continue;
            }
            if (victim == fonts.end() || it->second.last_used < victim->second.last_used) {
                victim = it;
            }
        }
        if (victim == fonts.end()) {
            break;
        }
        spdlog::debug("font: evicting {}px atlas ({} codepoints), {} resident", victim->first,
                      victim->second.codepoints.size(), fonts.size());
        release_font(victim->second);
        release_cache(victim->second);
        fonts.erase(victim);
    }
}

// Rasterize only the codepoints not yet in the cache (stb_truetype via load_font_data).
bool FontManager::rasterize_new(SizedFont& entry, int font_size, const std::vector<int>& cps) {
    if (cps.empty()) {
        return true;
    }
    std::vector<GlyphInfo> glyphs = load_font_data(font_bytes(), font_size, cps);
    std::unordered_set<int> got;
    for (const GlyphInfo& g : glyphs) {
        entry.cache.push_back(g); // takes ownership of the glyph image
        got.insert(g.value);
    }
    entry.atlas_dirty = true;

    std::vector<int> missing, alts;
    for (int cp : cps) {
        if (got.contains(cp)) {
            continue;
        }
        int alt = static_cast<int>(han_fold_codepoint(static_cast<uint32_t>(cp)));
        if (alt != cp) {
            missing.push_back(cp);
            alts.push_back(alt);
        }
    }
    if (missing.empty()) {
        return true;
    }
    std::vector<GlyphInfo> ag = load_font_data(font_bytes(), font_size, alts);
    std::vector<bool> assigned(alts.size(), false);
    for (auto& i : ag) {
        bool used = false;
        for (size_t k = 0; k < alts.size(); k++) {
            if (assigned[k] || alts[k] != i.value) {
                continue;
            }
            GlyphInfo gi = i;
            gi.value = missing[k];
            if (used) {
                gi.image = image_copy(i.image); // two originals folding to one form
            }
            entry.cache.push_back(gi);
            used = true;
            assigned[k] = true;
        }
        if (!used) {
            unload_image(i.image);
        }
    }
    // Any alt the font lacks never got assigned; retry those too.
    for (size_t k = 0; k < alts.size(); k++) {
        if (!assigned[k]) {
            entry.codepoints.erase(missing[k]);
        }
    }
    return true;
}

// Pack the cached glyphs into a fresh atlas texture. The Font's glyph images alias
// the GRAYSCALE bitmaps owned by `cache` (text_coverage composites from them).
void FontManager::rebuild_atlas(SizedFont& entry, int font_size) {
    release_font(entry);
    const int n = static_cast<int>(entry.cache.size());
    Font f{};
    f.baseSize = font_size;
    f.glyphCount = n;
    f.glyphPadding = 4; // same as load_font
    if (n == 0) {
        entry.atlas_dirty = false;
        return;
    } // nothing to pack; keep loaded == false
    f.glyphs = static_cast<GlyphInfo*>(std::malloc(sizeof(GlyphInfo) * n));
    if (!f.glyphs) {
        return;
    }
    for (int i = 0; i < n; i++) {
        f.glyphs[i] = entry.cache[i];
    }
    Image atlas = gen_image_font_atlas(f.glyphs, &f.recs, n, font_size, f.glyphPadding);
    if (!atlas.data || !f.recs) {
        if (atlas.data) {
            unload_image(atlas);
        }
        if (f.recs) {
            std::free(f.recs);
        }
        std::free(f.glyphs);
        return;
    }
    f.texture = load_texture_from_image(atlas);
    unload_image(atlas);
    set_texture_filter(f.texture, texture_filter::BILINEAR);
    entry.font = f;
    entry.loaded = true;
    entry.atlas_dirty = false;
}

// Free texture/recs/glyph-struct array of a font built by rebuild_atlas. Never
// unload_font(): that would free the glyph images still owned by `cache`.
void FontManager::release_font(SizedFont& entry) {
    if (!entry.loaded) {
        return;
    }
    if (entry.font.texture.id != 0) {
        unload_texture(entry.font.texture);
    }
    if (entry.font.recs) {
        std::free(entry.font.recs);
    }
    if (entry.font.glyphs) {
        std::free(entry.font.glyphs);
    }
    entry.font = {};
    entry.loaded = false;
}

void FontManager::release_cache(SizedFont& entry) {
    for (auto& g : entry.cache) {
        unload_image(g.image);
    }
    entry.cache.clear();
    entry.codepoints.clear();
}

bool FontManager::register_codepoints(SizedFont& entry, int font_size, const std::string& text) {
    std::vector<int> fresh;
    if (entry.codepoints.empty()) {
        // "A" is what OutlinedText measures line height with; keep it (and '?') always present.
        for (int cp : {'A', '?', ' '}) {
            entry.codepoints.insert(cp);
            fresh.push_back(cp);
        }
    }
    const char* ptr = text.c_str();
    while (*ptr) {
        int cp_size = 0;
        int codepoint = get_codepoint_next(ptr, &cp_size);
        if (cp_size <= 0) {
            break;
        }
        if (codepoint > 0 && entry.codepoints.insert(codepoint).second) {
            fresh.push_back(codepoint);
        }
        ptr += cp_size;
    }
    if (fresh.empty()) {
        return false;
    }
    if (!rasterize_new(entry, font_size, fresh)) {
        for (int cp : fresh) {
            entry.codepoints.erase(cp); // allow a later retry
        }
        spdlog::warn("font: failed to rasterize {} codepoints at {}px", fresh.size(), font_size);
        return false;
    }
    return true;
}

void FontManager::register_text(const std::string& text, int font_size) {
    std::scoped_lock lock(font_mutex);
    font_size = std::max(font_size, 1);
    register_codepoints(fonts[font_size], font_size, text);
}

FontManager::SizedFont& FontManager::acquire(const std::string& text, int font_size) {
    font_size = std::max(font_size, 1);

    SizedFont& entry = fonts[font_size];
    register_codepoints(entry, font_size, text);
    if (!entry.loaded || entry.atlas_dirty) {
        rebuild_atlas(entry, font_size);
    }

    entry.last_used = ++use_clock;
    evict_lru(font_size); // never evicts `font_size` itself, so `entry` stays valid
    return entry;
}

static Color clear_of(Color c) {
    return Color{c.r, c.g, c.b, 0};
}

// Both images are RGBA8 (every canvas here comes from gen_image_color).
static void blit_over(Image* dst, const Image& src, int x0, int y0) {
    if (!dst->data || !src.data) {
        return;
    }
    auto* d = static_cast<unsigned char*>(dst->data);
    const auto* sp = static_cast<const unsigned char*>(src.data);
    for (int y = 0; y < src.height; y++) {
        int dy = y0 + y;
        if (dy < 0 || dy >= dst->height) {
            continue;
        }
        for (int x = 0; x < src.width; x++) {
            int dx = x0 + x;
            if (dx < 0 || dx >= dst->width) {
                continue;
            }
            const unsigned char* s = sp + (y * src.width + x) * 4;
            unsigned char* o = d + (dy * dst->width + dx) * 4;
            const int sa = s[3];
            if (sa == 0) {
                continue;
            }
            if (sa == 255) {
                o[0] = s[0];
                o[1] = s[1];
                o[2] = s[2];
                o[3] = 255;
                continue;
            }
            const int da = o[3];
            const int oa = sa * 255 + da * (255 - sa); // out alpha * 255
            for (int c = 0; c < 3; c++) {
                int v = (s[c] * sa * 255 + o[c] * da * (255 - sa) + oa / 2) / (oa ? oa : 1);
                o[c] = static_cast<unsigned char>(v > 255 ? 255 : v);
            }
            o[3] = static_cast<unsigned char>((oa + 127) / 255);
        }
    }
}

static int text_margin(float font_size) {
    return static_cast<int>(std::ceil(font_size * 0.25f));
}

// Alpha-only rendering of a line: every caller recolors it, so only coverage is kept.
struct Coverage {
    std::vector<unsigned char> a;
    int w = 0, h = 0;
};

static Coverage text_coverage(const Font& font, const char* text, float font_size, float spacing, int margin) {
    Vec2 sz = measure_text_ex(font, text, font_size, spacing);
    Coverage c;
    c.w = static_cast<int>(sz.x);
    c.h = static_cast<int>(sz.y) + margin * 2;
    if (c.w <= 0 || c.h <= 0) {
        return {};
    }
    c.a.assign(static_cast<size_t>(c.w) * c.h, 0);
    int x = 0;
    for (const char* p = text; *p;) {
        int n = 0;
        int cp = get_codepoint_next(p, &n);
        if (n <= 0) {
            break;
        }
        p += n;
        int gi = get_glyph_index(font, cp);
        const GlyphInfo& g = font.glyphs[gi];
        const auto* px = static_cast<const unsigned char*>(g.image.data);
        if (px && cp != ' ' && cp != '\t') {
            const int ox = x + g.offsetX, oy = margin + g.offsetY;
            for (int y = 0; y < g.image.height; y++) {
                if (oy + y < 0 || oy + y >= c.h) {
                    continue;
                }
                for (int gx = 0; gx < g.image.width; gx++) {
                    if (ox + gx < 0 || ox + gx >= c.w) {
                        continue;
                    }
                    const unsigned s = px[y * g.image.width + gx];
                    unsigned char& d = c.a[(oy + y) * c.w + ox + gx];
                    if (s) { // alpha "over", same integer rounding as before (raylib's ColorAlphaBlend)
                        d = s == 255 ? 255 : static_cast<unsigned char>(((s + 1) * 256 + d * (255 - s)) >> 8);
                    }
                }
            }
        }
        x += (g.advanceX == 0 ? static_cast<int>(font.recs[gi].width + spacing)
                              : g.advanceX + static_cast<int>(spacing));
    }
    return c;
}

static void draw_text_clean(Image* dst, const Font& font, const char* text, Vec2 pos, float font_size, float spacing,
                            Color tint) {
    const int m = text_margin(font_size);
    Coverage c = text_coverage(font, text, font_size, spacing, m);
    if (c.a.empty()) {
        return;
    }
    Image t = gen_image_color(c.w, c.h, tint);
    auto* px = static_cast<unsigned char*>(t.data);
    for (size_t i = 0; i < c.a.size(); i++) {
        px[i * 4 + 3] = c.a[i] * tint.a / 255;
    }
    blit_over(dst, t, static_cast<int>(pos.x), static_cast<int>(pos.y) - m);
    unload_image(t);
}

static void stamp_outline(Image* dst, const Font& font, const char* text, Vec2 pos, float font_size, float spacing,
                          Color outline_color, float radius) {
    const int m = text_margin(font_size);
    Coverage cov = text_coverage(font, text, font_size, spacing, m);
    if (cov.a.empty()) {
        return;
    }
    const std::vector<unsigned char>& a = cov.a;
    const int r = static_cast<int>(std::ceil(radius + 0.5f));
    const int W = cov.w, H = cov.h;
    const int OW = W + 2 * r, OH = H + 2 * r;
    std::vector<unsigned char> out(OW * OH, 0);
    // disk weights, 0..255, with the soft 1 px rim; rows carry their non-zero span
    const int D = 2 * r + 1;
    std::vector<unsigned char> wt(D * D, 0);
    std::vector<int> lo(D, D), hi(D, -1);
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            float d = std::sqrt(static_cast<float>(dx) * dx + static_cast<float>(dy) * dy);
            float w = radius + 0.5f - d;
            if (w <= 0.0f) {
                continue;
            }
            w = std::min(w, 1.0f);
            wt[(dy + r) * D + (dx + r)] = static_cast<unsigned char>(w * 255.0f + 0.5f);
            lo[dy + r] = std::min(lo[dy + r], dx + r);
            hi[dy + r] = std::max(hi[dy + r], dx + r);
        }
    }
    // H*W*D*D byte ops -- fine for HUD text
    for (int y = 0; y < H; y++) {
        const unsigned char* src = &a[y * W];
        for (int x = 0; x < W; x++) {
            const unsigned v = src[x];
            if (!v) {
                continue;
            }
            for (int dy = -r; dy <= r; dy++) {
                const int row = dy + r;
                if (hi[row] < 0) {
                    continue;
                }
                unsigned char* orow = &out[(y + dy + r) * OW + x];
                const unsigned char* wrow = &wt[row * D];
                for (int k = lo[row]; k <= hi[row]; k++) {
                    const auto c = static_cast<unsigned char>((v * wrow[k] + 127) / 255);
                    unsigned char& o = orow[k]; // ox = x + (k - r) + r = x + k
                    o = std::max(o, c);
                }
            }
        }
    }
    Image o = gen_image_color(OW, OH, Color{outline_color.r, outline_color.g, outline_color.b, 0});
    auto* op = static_cast<unsigned char*>(o.data);
    for (int i = 0; i < OW * OH; i++) {
        op[i * 4 + 3] = out[i] * outline_color.a / 255;
    }
    blit_over(dst, o, static_cast<int>(pos.x) - r, static_cast<int>(pos.y) - m - r);
    unload_image(o);
}

static Font deep_copy_font(const Font& src) {
    Font dst = src;
    dst.glyphs = nullptr;
    dst.recs = nullptr;
    dst.glyphCount = 0;
    if (src.glyphCount > 0 && src.glyphs && src.recs) {
        dst.glyphCount = src.glyphCount;
        dst.glyphs = static_cast<GlyphInfo*>(std::malloc(src.glyphCount * sizeof(GlyphInfo)));
        memcpy(dst.glyphs, src.glyphs, src.glyphCount * sizeof(GlyphInfo));
        for (int i = 0; i < src.glyphCount; i++) {
            const Image& si = src.glyphs[i].image;
            Image& di = dst.glyphs[i].image;
            if (si.data && si.width > 0 && si.height > 0) {
                int dataSize = get_pixel_data_size(si.width, si.height, si.format);
                di.data = std::malloc(dataSize);
                memcpy(di.data, si.data, dataSize);
            } else {
                di.data = nullptr;
            }
        }
        dst.recs = static_cast<Rect*>(std::malloc(src.glyphCount * sizeof(Rect)));
        memcpy(dst.recs, src.recs, src.glyphCount * sizeof(Rect));
    }
    return dst;
}

// NOTE: the returned Font aliases manager-owned glyphs/recs/texture; it is only valid
// until the next acquire()/register_text() call for any size. Use copy_font() to keep it.
Font FontManager::get_font(const std::string& text, int font_size) {
    std::scoped_lock lock(font_mutex);
    return acquire(text, font_size).font;
}

Font FontManager::copy_font(const std::string& text, int font_size) {
    std::scoped_lock lock(font_mutex);
    Font copy = deep_copy_font(acquire(text, font_size).font);
    copy.texture = {}; // CPU-only: measured and composited, never drawn
    return copy;
}

// Characters placed to the right of the preceding glyph at the same y,
// rather than stacked below it (terminal punctuation, etc.).
static bool is_beside_prev_char(const std::string& s) {
    static const std::unordered_set<std::string> beside_set = {
        ".",
        ",",
        "'",
        "\"",
        "\xe3\x80\x82", // 。
        "\xe3\x80\x81", // 、
    };
    return beside_set.contains(s);
}

// Extra downward draw offset for beside chars whose glyphs sit near the top
// of their cell (e.g. apostrophes), so they appear vertically centred.
static float beside_y_offset(const std::string& s, float char_height) {
    if (s == "'" || s == "\"") {
        return char_height * 0.7f;
    }
    return 0.0f;
}

static bool is_small_kana(const std::string& s) {
    static const std::unordered_set<std::string> sutegana = {
        // Small hiragana
        "ぁ",
        "ぃ",
        "ぅ",
        "ぇ",
        "ぉ",
        "っ",
        "ゃ",
        "ゅ",
        "ょ",
        "ゎ",
        "ゕ",
        "ゖ",
        // Small katakana
        "ァ",
        "ィ",
        "ゥ",
        "ェ",
        "ォ",
        "ッ",
        "ャ",
        "ュ",
        "ョ",
        "ヮ",
        "ヵ",
        "ヶ",
    };
    return sutegana.contains(s);
}

static bool needs_less_spacing_above(const std::string& s) {
    static const std::unordered_set<std::string> alphabet = {
        " ", "a", "c", "e", "g", "m", "n", "o", "p", "q", "r", "s", "u", "v", "w", "x", "y", "z",
    };
    return is_small_kana(s) || alphabet.contains(s);
}

// Consecutive runs of 2+ of these are drawn horizontally side-by-side.
static bool is_hgroup_char(const std::string& s) {
    static const std::unordered_set<std::string> hgroup_set = {
        "!",
        "?",
        "\xef\xbc\x81", // ！
        "\xef\xbc\x9f", // ？
        "\xe2\x80\xa0", // chaos time the dark
    };
    return hgroup_set.contains(s);
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
        "-",
        "\xe2\x80\x90",
        "|",
        "/",
        "\\",
        "\xe3\x83\xbc",
        "\xef\xbd\x9e",
        "~",
        "\xef\xbc\x88",
        "\xef\xbc\x89",
        "(",
        ")",
        "\xe3\x80\x8c",
        "\xe3\x80\x8d",
        "[",
        "]",
        "\xef\xbc\xb3",
        "\xef\xbc\xb4",
        "\xe3\x80\x90",
        "\xe3\x80\x91",
        "\xe2\x80\xa6",
        "\xe2\x86\x92",
        ":",
        "\xef\xbc\x9a",
    };
    return rotate_set.contains(s);
}

OutlinedText::OutlinedText(std::string text, int font_size, Color color, Color outline_color, bool is_vertical,
                           float outline_thickness, float spacing, float v_advance, const std::string& font_name)
    : text(std::move(text)), font_size(static_cast<float>(font_size)),
      outline_thickness(outline_thickness * global_tex.screen_scale), v_advance(v_advance) {
    worker_font = named_font_manager(font_name).copy_font(this->text, font_size);

    if (is_vertical) {
        float char_height = measure_text_ex(worker_font, "A", font_size, spacing).y;
        float max_char_width = 0.0f;
        int char_count = 0;
        const char* ptr = this->text.c_str();
        while (*ptr) {
            int cp_size = 0;
            get_codepoint_next(ptr, &cp_size);
            if (cp_size <= 0) {
                break;
            }
            std::string s(ptr, cp_size);
            float w = measure_text_ex(worker_font, s.c_str(), font_size, spacing).x;
            max_char_width = std::max(max_char_width, w);
            ++char_count;
            ptr += cp_size;
        }
        int pad = static_cast<int>(this->outline_thickness) + 2;
        width = max_char_width + pad * 2;
        height = char_height * (1.0f + (char_count - 1) * v_advance) + pad * 2;
    } else {
        float sp = (spacing < 0) ? 0.0f : spacing;
        int pad = static_cast<int>(this->outline_thickness) + 2;
        auto lines = split_lines(this->text);
        float line_h = measure_text_ex(worker_font, "A", font_size, sp).y;
        float line_advance = line_h + this->outline_thickness * 2;
        float max_w = 0.0f;
        for (const auto& line : lines) {
            float lw = measure_text_ex(worker_font, line.c_str(), font_size, sp).x;
            max_w = std::max(max_w, lw);
        }
        width = max_w + pad * 2;
        height = line_advance * (lines.size() - 1) + line_h + pad * 2;
    }

#ifdef __EMSCRIPTEN__
    {
        auto data = is_vertical ? build_vertical_text(color, outline_color, spacing)
                                : build_horizontal_text(color, outline_color, spacing);
        pending_image = std::move(data.img);
    }
#else
    if (is_vertical) {
        build_future = std::async(std::launch::async, [this, color, outline_color, spacing] {
            auto data = build_vertical_text(color, outline_color, spacing);
            std::scoped_lock lock(pending_mutex);
            pending_image = data.img;
        });
    } else {
        build_future = std::async(std::launch::async, [this, color, outline_color, spacing] {
            auto data = build_horizontal_text(color, outline_color, spacing);
            std::scoped_lock lock(pending_mutex);
            pending_image = data.img;
        });
    }
#endif
}

OutlinedText::~OutlinedText() {
    if (build_future.valid()) {
        build_future.wait();
    }
    {
        std::scoped_lock lock(pending_mutex);
        if (pending_image.has_value()) {
            unload_image(pending_image.value());
            pending_image.reset();
        }
    }
    if (worker_font.glyphCount > 0) {
        worker_font.texture = {};
        unload_font(worker_font);
    }
    if (texture.has_value()) {
        unload_texture(texture.value());
    }
}

OutlinedText::BuildData OutlinedText::build_horizontal_text(Color color, Color outline_color, float spacing) {
    float sp = (spacing < 0) ? 0.0f : spacing;
    auto lines = split_lines(text);
    float line_h = measure_text_ex(worker_font, "A", font_size, sp).y;
    float line_advance = line_h + outline_thickness * 2;
    float max_w = 0.0f;
    for (const auto& line : lines) {
        float lw = measure_text_ex(worker_font, line.c_str(), font_size, sp).x;
        max_w = std::max(max_w, lw);
    }

    int pad = static_cast<int>(outline_thickness) + 2;

    top_extra = std::max(0, text_margin(font_size) - pad);
    Image img = gen_image_color(static_cast<int>(max_w) + pad * 2,
                                static_cast<int>(line_advance * (lines.size() - 1) + line_h) + pad * 2 + top_extra,
                                clear_of(outline_thickness > 0 ? outline_color : color));

    for (size_t li = 0; li < lines.size(); li++) {
        float y = pad + top_extra + li * line_advance;
        if (outline_thickness > 0) {
            stamp_outline(&img, worker_font, lines[li].c_str(), {static_cast<float>(pad), y}, font_size, sp,
                          outline_color, outline_thickness);
        }
        draw_text_clean(&img, worker_font, lines[li].c_str(), {static_cast<float>(pad), y}, font_size, sp, color);
    }

    return {img};
}

OutlinedText::BuildData OutlinedText::build_vertical_text(Color color, Color outline_color, float spacing) {
    float char_height = measure_text_ex(worker_font, "A", font_size, spacing).y;

    // Parse the text into items. Consecutive runs of 2+ hgroup chars (!, ?)
    // become a single item drawn horizontally; everything else is one item.
    struct TextItem {
        std::vector<std::string> chars;
        bool is_hgroup;
        float width; // total render width
    };

    std::vector<std::string> raw_chars;
    {
        const char* ptr = text.c_str();
        while (*ptr) {
            int cp_size = 0;
            get_codepoint_next(ptr, &cp_size);
            if (cp_size <= 0) {
                break;
            }
            raw_chars.emplace_back(ptr, cp_size);
            ptr += cp_size;
        }
    }

    std::vector<TextItem> items;
    for (size_t i = 0; i < raw_chars.size();) {
        if (is_hgroup_char(raw_chars[i])) {
            size_t j = i;
            while (j < raw_chars.size() && is_hgroup_char(raw_chars[j])) {
                j++;
            }
            TextItem item;
            item.is_hgroup = (j - i >= 2);
            item.chars.assign(raw_chars.begin() + i, raw_chars.begin() + j);
            item.width = 0.0f;
            for (const auto& s : item.chars) {
                item.width += measure_text_ex(worker_font, s.c_str(), font_size, spacing).x;
            }
            items.push_back(std::move(item));
            i = j;
        } else {
            TextItem item;
            item.is_hgroup = false;
            item.chars = {raw_chars[i]};
            item.width = measure_text_ex(worker_font, raw_chars[i].c_str(), font_size, spacing).x;
            items.push_back(std::move(item));
            ++i;
        }
    }

    float max_item_width = 0.0f;
    for (const auto& item : items) {
        max_item_width = std::max(max_item_width, item.width);
    }

    int pad = static_cast<int>(outline_thickness) + 2;
    int img_w = static_cast<int>(max_item_width + pad * 2);

    // Pre-compute y position for each item.
    static constexpr float small_char_advance_factor = 0.8f;
    static constexpr float beside_advance_factor = 0.3f;
    std::vector<float> y_positions(items.size(), pad);
    float current_y = pad;
    for (size_t i = 1; i < items.size(); i++) {
        float advance;
        const auto& first = items[i].chars[0];
        if (items[i].is_hgroup) {
            advance = char_height;
        } else if (is_beside_prev_char(first)) {
            advance = char_height * beside_advance_factor;
        } else if (needs_less_spacing_above(first)) {
            advance = char_height * small_char_advance_factor;
        } else {
            advance = char_height;
        }
        current_y += advance * v_advance;
        y_positions[i] = current_y;
    }
    int img_h = items.empty() ? pad * 2 : static_cast<int>(y_positions.back() + char_height + pad);

    Image img = gen_image_color(img_w, img_h, clear_of(color));
    Image outline_img = gen_image_color(img_w, img_h, clear_of(outline_color));

    // Two passes: 0 = outlines onto outline_img, 1 = fills onto img.
    for (int pass = 0; pass < 2; pass++) {
        Image* target = (pass == 0) ? &outline_img : &img;

        for (size_t i = 0; i < items.size(); i++) {
            const auto& item = items[i];
            float y = y_positions[i];

            if (item.is_hgroup) {
                // Chars drawn left-to-right, group centred in max_item_width.
                float cx = pad + (max_item_width - item.width) / 2.0f;
                for (const auto& s : item.chars) {
                    float cw = measure_text_ex(worker_font, s.c_str(), font_size, spacing).x;
                    if (pass == 0) {
                        if (outline_thickness > 0) {
                            stamp_outline(target, worker_font, s.c_str(), {cx, y}, font_size, spacing, outline_color,
                                          outline_thickness);
                        }
                    } else {
                        draw_text_clean(target, worker_font, s.c_str(), {cx, y}, font_size, spacing, color);
                    }
                    cx += cw;
                }
            } else {
                const auto& s = item.chars[0];
                float char_width = item.width;
                float x = is_beside_prev_char(s) ? pad + (max_item_width - char_width)
                                                 : pad + (max_item_width - char_width) / 2.0f;
                float draw_y = y + beside_y_offset(s, char_height);

                if (!is_beside_prev_char(s) && in_rotate_set(s)) {
                    int tmp_w = static_cast<int>(char_width) + pad * 2;
                    int tmp_h = static_cast<int>(char_height) + pad * 2;
                    Image tmp = gen_image_color(tmp_w, tmp_h, clear_of(outline_thickness > 0 ? outline_color : color));

                    if (pass == 0) {
                        if (outline_thickness > 0) {
                            stamp_outline(&tmp, worker_font, s.c_str(),
                                          {static_cast<float>(pad), static_cast<float>(pad)}, font_size, spacing,
                                          outline_color, outline_thickness);
                        }
                    } else {
                        draw_text_clean(&tmp, worker_font, s.c_str(),
                                        {static_cast<float>(pad), static_cast<float>(pad)}, font_size, spacing, color);
                    }
                    image_rotate_cw(&tmp);

                    float dx = x + (char_width - tmp.width) / 2.0f;
                    float dy = draw_y + (char_height - tmp.height) / 2.0f;
                    Rect dst = {dx, dy, static_cast<float>(tmp.width), static_cast<float>(tmp.height)};
                    blit_over(target, tmp, static_cast<int>(dst.x), static_cast<int>(dst.y));
                    unload_image(tmp);
                } else {
                    if (pass == 0) {
                        if (outline_thickness > 0) {
                            stamp_outline(target, worker_font, s.c_str(), {x, draw_y}, font_size, spacing,
                                          outline_color, outline_thickness);
                        }
                    } else {
                        draw_text_clean(target, worker_font, s.c_str(), {x, draw_y}, font_size, spacing, color);
                    }
                }
            }
        }

        if (pass == 0) {
            blit_over(&img, outline_img, 0, 0);
            unload_image(outline_img);
        }
    }

    return {img};
}

bool OutlinedText::upload_pending() {
    std::optional<Image> img;
    {
        std::scoped_lock lock(pending_mutex);
        if (!pending_image.has_value()) {
            return false;
        }
        img = pending_image;
        pending_image.reset();
    }

    // the build task still reads worker_font; only release it once the task has finished
    if (build_future.valid()) {
        build_future.wait();
    }
    worker_font.texture = {};
    unload_font(worker_font);
    worker_font = {};

    Texture tex = load_texture_from_image(*img);
    set_texture_filter(tex, texture_filter::BILINEAR);
    unload_image(*img);

    texture = tex;
    width = tex.width;
    height = static_cast<float>(tex.height) - top_extra;
    return true;
}

void OutlinedText::finish() {
    if (build_future.valid()) {
        build_future.get();
    }
    upload_pending();
}

void OutlinedText::draw(const DrawTextureParams& params) {
    upload_pending();

    if (!texture.has_value()) {
        return;
    }

    Rect src = params.src.value_or(Rect{0, 0, static_cast<float>(texture->width), static_cast<float>(texture->height)});
    float dx = params.x + x_offset;
    float dy = params.y + y_offset - top_extra;

    if (params.x2 == 0.0f && params.y2 == 0.0f) {
        dx = roundf(dx);
        dy = roundf(dy);
    }

    // dst is sized off src (not the full texture) so a caller can pass a
    // shorter/narrower src to hard-crop the text instead of stretching it.
    float dst_w = src.width * params.scale + params.x2;
    float dst_h = src.height * params.scale + params.y2;
    Rect dst;
    if (params.center) {
        dst = {dx + (src.width - src.width * params.scale) * 0.5f, dy + (src.height - src.height * params.scale) * 0.5f,
               dst_w, dst_h};
    } else {
        dst = {dx, dy, dst_w, dst_h};
    }
    if (debug_log_draws) {
        std::string label = text.size() > 40 ? text.substr(0, 40) + "..." : text;
        debug_draw_log.push_back({"\"" + label + "\"", dst});
        log_lua_site(debug_draw_log.back(), script_lua_state());
    }

    draw_texture_pro(*texture, src, dst, {0, 0}, 0.0f, fade(Color::WHITE, params.fade));
}

FontManager font_manager;

static std::unordered_map<std::string, std::unique_ptr<FontManager>> named_fonts;

FontManager& named_font_manager(const std::string& name) {
    if (name.empty()) {
        return font_manager;
    }
    auto it = named_fonts.find(name);
    if (it == named_fonts.end()) {
        std::unique_ptr<FontManager> fm;
        if (name.find_first_of("/\\") == std::string::npos) {
            fs::path p = resolve_skin_path("Fonts/" + (fs::path(name).has_extension() ? name : name + ".ttf"));
            try {
                fm = std::make_unique<FontManager>();
                fm->init(p);
                if (fm->get_font("A?", 16).glyphCount <= 0) {
                    throw std::runtime_error("no usable glyphs in " + p.string());
                }
            } catch (const std::exception& e) {
                spdlog::warn("font '{}' failed to load, using default: {}", name, e.what());
                fm.reset();
            }
        } else {
            spdlog::warn("invalid font name '{}', using default", name);
        }
        it = named_fonts.emplace(name, std::move(fm)).first; // null entry caches failure
    }
    return it->second ? *it->second : font_manager;
}

void unload_named_fonts() {
    for (auto& [n, fm] : named_fonts) {
        if (fm) {
            fm->unload();
        }
    }
    named_fonts.clear();
}

bool is_valid_utf8(std::string_view text) {
    size_t i = 0;
    const size_t n = text.size();
    while (i < n) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c < 0x80) {
            i++;
            continue;
        }
        int extra;
        unsigned int cp;
        if ((c & 0xE0) == 0xC0) {
            extra = 1;
            cp = c & 0x1F;
        } else if ((c & 0xF0) == 0xE0) {
            extra = 2;
            cp = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            extra = 3;
            cp = c & 0x07;
        } else {
            return false;
        }
        if (i + extra >= n) {
            return false;
        }
        for (int k = 1; k <= extra; k++) {
            const auto cc = static_cast<unsigned char>(text[i + k]);
            if ((cc & 0xC0) != 0x80) {
                return false;
            }
            cp = (cp << 6) | (cc & 0x3F);
        }
        // overlong forms, surrogates and values past U+10FFFF are not UTF-8
        if ((extra == 1 && cp < 0x80) || (extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000) ||
            (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
            return false;
        }
        i += extra + 1;
    }
    return true;
}

std::string utf8_from_utf8_or_sjis(std::string_view text) {
    if (is_valid_utf8(text)) {
        return std::string(text);
    }
#if defined(_WIN32)
    return win32_utf8_from_sjis(text);
#elif defined(YATAIDON_HAVE_ICONV)
    iconv_t cd = iconv_open("UTF-8", "CP932");
    if (cd == reinterpret_cast<iconv_t>(-1)) {
        return std::string(text);
    }
    std::string out(text.size() * 3 + 4, '\0');
    char* in_ptr = const_cast<char*>(text.data());
    size_t in_left = text.size();
    char* out_ptr = out.data();
    size_t out_left = out.size();
    while (in_left > 0) {
        if (iconv(cd, &in_ptr, &in_left, &out_ptr, &out_left) != static_cast<size_t>(-1)) {
            break;
        }
        // a byte CP932 does not map: keep it as '?' and go on
        if (out_left == 0) {
            break;
        }
        *out_ptr++ = '?';
        out_left--;
        in_ptr++;
        in_left--;
    }
    iconv_close(cd);
    out.resize(out.size() - out_left);
    return out;
#else
    return std::string(text);
#endif
}
