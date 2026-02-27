#pragma once

#include "texture.h"
#include <unordered_set>

class FontManager {
private:
    fs::path font_path;
    ray::Font font;
    int max_font_size;
    std::unordered_set<int> codepoint_cache;

public:
    FontManager();

    void init(const fs::path& font_path);

    ray::Font get_font(const std::string& text, int font_size);
};

class OutlinedText {
private:
    ray::Font sdf_font;
    std::string text;
    float font_size;
    float outline_thickness;
    ray::Texture texture;

    void create_horizontal_text(ray::Color color, ray::Color outline_color, float spacing);

    void create_vertical_text(ray::Color color, ray::Color outline_color, float spacing);

public:
    float width;
    float height;
    OutlinedText(std::string text, int font_size, ray::Color color, ray::Color outline_color, bool is_vertical, int outline_thickness = 5, float spacing = 2.0f);

    void draw(const DrawTextureParams& = {});
};

extern FontManager font_manager;
