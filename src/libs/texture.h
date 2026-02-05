#pragma once

#include <rapidjson/istreamwrapper.h>

#include "animation.h"
#include "config.h"

namespace fs = std::filesystem;
using namespace rapidjson;

struct DrawTextureParams {
    ray::Color color = ray::WHITE;
    int frame = 0;
    float scale = 1.0f;
    bool center = false;
    std::string mirror = "";
    float x = 0, y = 0, x2 = 0, y2 = 0;
    ray::Vector2 origin = {0, 0};
    float rotation = 0;
    double fade = 1.1f;
    int index = 0;
    std::optional<ray::Rectangle> src = std::nullopt;
    bool controllable = false;
};

struct SkinInfo {
    float x;
    float y;
    int font_size;
    float width;
    float height;
    std::map<std::string, std::string> text;

    SkinInfo(float x = 0, float y = 0, int font_size = 0,
             float width = 0, float height = 0,
             const std::map<std::string, std::string>& text = {})
        : x(x), y(y), font_size(font_size), width(width), height(height), text(text) {}
};

struct TextureObject {
    std::string name;
    int width;
    int height;
    std::vector<int> x;
    std::vector<int> y;
    std::vector<int> x2;
    std::vector<int> y2;
    std::vector<bool> controllable;
    std::optional<std::vector<ray::Rectangle>> crop_data;

    TextureObject(const std::string& name, int width, int height)
        : name(name), width(width), height(height),
          x{0}, y{0}, x2{width}, y2{height}, controllable{false} {}

    virtual ~TextureObject() = default;
};

struct SingleTexture : public TextureObject {
    ray::Texture2D texture;

    SingleTexture(const std::string& name, const ray::Texture2D& tex)
        : TextureObject(name, tex.width, tex.height), texture(tex) {
        GenTextureMipmaps(&texture);
        SetTextureFilter(texture, ray::TEXTURE_FILTER_TRILINEAR);
        SetTextureWrap(texture, ray::TEXTURE_WRAP_CLAMP);
    }

    ~SingleTexture() override {
        UnloadTexture(texture);
    }
};

struct FramedTexture : public TextureObject {
    std::vector<ray::Texture2D> textures;

    FramedTexture(const std::string& name, const std::vector<ray::Texture2D>& texs)
        : TextureObject(name, texs.empty() ? 0 : texs[0].width,
                       texs.empty() ? 0 : texs[0].height), textures(texs) {
        for (auto& tex : textures) {
            GenTextureMipmaps(&const_cast<ray::Texture2D&>(tex));
            SetTextureFilter(tex, ray::TEXTURE_FILTER_TRILINEAR);
            SetTextureWrap(tex, ray::TEXTURE_WRAP_CLAMP);
        }
    }

    ~FramedTexture() override {
        for (auto& tex : textures) {
            UnloadTexture(tex);
        }
    }
};

class TextureWrapper {
private:
    std::map<int, std::unique_ptr<BaseAnimation>> animations;
    std::vector<std::unique_ptr<BaseAnimation>> copied_animations;
    fs::path graphics_path;
    fs::path parent_graphics_path;

public:
    std::map<std::string, std::map<std::string, std::shared_ptr<TextureObject>>> textures;
    std::map<std::string, SkinInfo> skin_config;
    int screen_width;
    int screen_height;
    float screen_scale;

    TextureWrapper() : screen_width(1280), screen_height(720), screen_scale(1.0) {
    }

    void init(const std::string& skin_path);

    ~TextureWrapper() {
        unload_textures();
    }

    void unload_textures();

    BaseAnimation* get_animation(const int id, bool is_copy = false);

    void read_tex_obj_data(const Value& tex_mapping, TextureObject* tex_obj);

    void load_animations(const std::string& screen_name);

    void load_folder(const std::string& screen_name, const std::string& subset);

    void load_screen_textures(const std::string& screen_name);

    void control(TextureObject* tex_obj, int index = 0);

    void clear_screen(const ray::Color& color);

    void draw_texture(const std::string& subset, const std::string& texture_name, const DrawTextureParams& = {});
};

extern TextureWrapper tex;
