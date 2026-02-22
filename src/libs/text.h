#pragma once

#include "texture.h"
#include <unordered_map>


class FontManager {
private:
    fs::path font_path;
    ray::Font font;
    int max_font_size;
    std::unordered_set<int> codepoint_cache;

public:
    FontManager();

    void init(const std::string& font_path);

    ray::Font get_font(const std::string& text, int font_size);
};

class OutlinedText {
private:
    ray::Font sdf_font;
    std::string text;
    float font_size;
    float outline_thickness;
    ray::Texture texture;

public:
    float width;
    float height;
    OutlinedText(std::string text, int font_size, ray::Color color, ray::Color outline_color, int outline_thickness);

    void draw(float x, float y, float fade);
};

extern FontManager font_manager;
