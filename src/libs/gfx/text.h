#pragma once

#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../gfx.h"
#include "color_utils.h"
#include "texture.h"

class FontManager {
private:
    fs::path font_path;
    uint64_t generation;

    struct SizedFont {
        Font font{}; // texture + recs + glyph structs; glyph images are fresh crops, not aliases of `cache`
        std::unordered_set<int> codepoints; // only what was asked for AT THIS SIZE
        std::vector<GlyphInfo> cache;       // rasterized once per codepoint (images owned here)
        bool atlas_dirty = false;
        uint64_t last_used = 0;
        bool loaded = false;
    };

    std::vector<unsigned char> font_data; // the .ttf bytes, read once

    // font_data, or the built-in default font until init() loads a skin font
    std::span<const unsigned char> font_bytes() const {
        return font_data.empty() ? default_font_ttf() : std::span<const unsigned char>(font_data);
    }

    bool rasterize_new(SizedFont& entry, int font_size, const std::vector<int>& cps);   // font_mutex held
    void rebuild_atlas(SizedFont& entry, int font_size);                                // font_mutex held
    void release_font(SizedFont& entry);                                                // font_mutex held
    void release_cache(SizedFont& entry);                                               // font_mutex held
    bool register_codepoints(SizedFont& entry, int font_size, const std::string& text); // font_mutex held

    std::unordered_map<int, SizedFont> fonts;
    uint64_t use_clock = 0;

    mutable std::mutex font_mutex;

    static constexpr size_t MAX_SIZED_FONTS = 32;

    SizedFont& acquire(const std::string& text, int font_size); // font_mutex held
    void evict_lru(int keep_size);                              // font_mutex held

public:
    FontManager();
    void init(const fs::path& font_path);
    void unload();
    Font get_font(const std::string& text, int font_size);
    Font copy_font(const std::string& text, int font_size);
    void register_text(const std::string& text, int font_size);
    uint64_t cache_generation() const;

    void load_text_config(const fs::path& skin_root);

    std::string get_text_label(const std::string& key);

    const SkinInfo* get_text_info(const std::string& key);
};

enum class LocalizedTextFallback {
    NONE,
    ENGLISH,
    ENGLISH_OR_FIRST,
    REQUIRED_ENGLISH,
};

inline std::string localized_text(const std::map<std::string, std::string>& values, const std::string& language,
                                  LocalizedTextFallback fallback) {
    if (auto it = values.find(language); it != values.end()) {
        return it->second;
    }

    if (fallback == LocalizedTextFallback::NONE) {
        return "";
    }
    if (fallback == LocalizedTextFallback::REQUIRED_ENGLISH) {
        return values.at("en");
    }

    if (auto it = values.find("en"); it != values.end()) {
        return it->second;
    }

    if (fallback == LocalizedTextFallback::ENGLISH_OR_FIRST && !values.empty()) {
        return values.begin()->second;
    }

    return "";
}

struct CachedOutlinedText;

// Fill color for the bytes [begin, end) of a text with its markup removed.
struct TextColorSpan {
    size_t begin;
    size_t end;
    Color color;
};

// OutlinedText reads <c=#RRGGBB>...</c> in its text as a fill color for that part (box.def titles use this).
// `text` without those tags, for code that compares or counts what is shown.
std::string strip_text_markup(const std::string& text);

class OutlinedText {
private:
    std::string text;
    std::vector<TextColorSpan> color_spans;
    float font_size;
    float outline_thickness;
    float v_advance = 1.0f;

    Font worker_font;

    std::optional<Image> pending_image;
    mutable std::mutex pending_mutex;

    std::optional<Texture> texture;
    std::shared_ptr<CachedOutlinedText> cached_texture;
    std::string cache_key;

    std::future<void> build_future;

    struct BuildData {
        Image img;
    };

    Color fill_at(size_t offset, Color base) const;

    BuildData build_horizontal_text(Color color, Color outline_color, float spacing);
    BuildData build_vertical_text(Color color, Color outline_color, float spacing);

public:
    float width = 0.0f;
    float height = 0.0f;
    float x_offset = 0.0f;
    float y_offset = 0.0f;
    int top_extra = 0;

    OutlinedText(std::string text, int font_size, Color color, Color outline_color, bool is_vertical,
                 float outline_thickness = 5.0f, float spacing = 2.0f, float v_advance = 1.0f,
                 const std::string& font_name = "");

    ~OutlinedText();

    bool upload_pending();

    bool is_ready() const { return texture.has_value(); }

    const std::string& get_text() const { return text; }

    void finish();

    void draw(const DrawTextureParams& = {});
};

extern FontManager font_manager;

// Skin font by name ("Fonts/<name>[.ttf]"); empty or unloadable -> the default font_manager.
FontManager& named_font_manager(const std::string& name);
void unload_named_fonts();

// Text read from charts and box.def files is UTF-8 (with or without a BOM) or, in files made for
// older simulators, Shift-JIS without a BOM.

bool is_valid_utf8(std::string_view text);

// `text` as UTF-8: returned as it is when it already is valid UTF-8, otherwise read as Shift-JIS
// (code page 932). Where no converter is available the bytes are returned unchanged.
std::string utf8_from_utf8_or_sjis(std::string_view text);
