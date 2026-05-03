#pragma once

#include "texture.h"
#include <future>

class FontManager {
private:
    fs::path font_path;
    ray::Font font;
    int max_font_size;
    std::unordered_set<int> codepoint_cache;
    mutable std::mutex font_mutex;
public:
    FontManager();
    void init(const fs::path& font_path);
    ray::Font get_font(const std::string& text, int font_size);
    // Returns a deep copy of the current font safe to use on another thread.
    // Caller must UnloadFont() the returned font when done.
    ray::Font copy_font(const std::string& text, int font_size);
};

class OutlinedText {
private:
    std::string text;
    float font_size;
    float outline_thickness;

    ray::Font worker_font;

    std::optional<ray::Image> pending_image;
    mutable std::mutex pending_mutex;

    std::optional<ray::Texture> texture;

    std::future<void> build_future;

    struct BuildData { ray::Image img; };

    BuildData build_horizontal_text(ray::Color color, ray::Color outline_color, float spacing);
    BuildData build_vertical_text  (ray::Color color, ray::Color outline_color, float spacing);

public:
    float width  = 0.0f;
    float height = 0.0f;

    OutlinedText(std::string text, int font_size,
                 ray::Color color, ray::Color outline_color,
                 bool is_vertical,
                 int outline_thickness = 5,
                 float spacing = 2.0f);

    ~OutlinedText();

    bool upload_pending();

    bool is_ready() const { return texture.has_value(); }

    void finish();

    void draw(const DrawTextureParams& = {});
};

extern FontManager font_manager;
