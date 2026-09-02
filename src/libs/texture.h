#pragma once

#include "animation.h"
#include "ray.h"
#include "skin_config_generated.h"
#include "texture_ids_generated.h" // IWYU pragma: keep
#include <filesystem>
#include <stdexcept>
#include <unordered_set>

namespace fs = std::filesystem;

enum class Mirror : uint8_t { NONE, HORIZONTAL, VERTICAL };

inline Mirror mirror_from_string(const std::string& s) {
    if (s == "horizontal") return Mirror::HORIZONTAL;
    if (s == "vertical") return Mirror::VERTICAL;
    return Mirror::NONE;
}

struct DrawTextureParams {
    ray::Color color = ray::WHITE;
    int frame = 0;
    float scale = 1.0f;
    bool center = false;
    Mirror mirror = Mirror::NONE;
    float x = 0, y = 0, x2 = 0, y2 = 0;
    ray::Vector2 origin = {0, 0};
    float rotation = 0;
    double fade = 1.1f;
    int index = 0;
    std::optional<ray::Rectangle> src = std::nullopt;
    std::optional<int> blend = std::nullopt;
};

inline int blend_from_string(const std::string& s) {
    if (s == "additive")   return ray::BLEND_ADDITIVE;
    if (s == "multiplied") return ray::BLEND_MULTIPLIED;
    if (s == "add_colors") return ray::BLEND_ADD_COLORS;
    if (s == "subtract_colors") return ray::BLEND_SUBTRACT_COLORS;
    if (s == "alpha_premultiply") return ray::BLEND_ALPHA_PREMULTIPLY;
    return ray::BLEND_ALPHA;
}

struct SkinInfo {
    float x;
    float y;
    int font_size;
    float width;
    float height;
    std::map<std::string, std::string> text;
    float outline = -1.0f;

    SkinInfo(float x = 0, float y = 0, int font_size = 0,
             float width = 0, float height = 0,
             const std::map<std::string, std::string>& text = {},
             float outline = -1.0f)
        : x(x), y(y), font_size(font_size), width(width), height(height), text(text),
          outline(outline) {}
};

struct Chara3DConfig {
    float scale = 650.0f;
    float rot_x = 181.25f;
    float rot_y = 27.5f;
    float rot_z = 0.0f;
};

struct TextureObject {
    std::string name;
    int width;
    int height;
    std::vector<int> x;
    std::vector<int> y;
    std::vector<int> x2;
    std::vector<int> y2;
    std::optional<std::vector<ray::Rectangle>> crop_data;
    TextureObject(const std::string& name, int width, int height)
        : name(name), width(width), height(height), x{0}, y{0}, x2{width}, y2{height} {}
    virtual ~TextureObject() = default;

    virtual const ray::Texture2D* frame_texture(int frame) const { return nullptr; }

    virtual int frame_count() const { return 1; }
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

    const ray::Texture2D* frame_texture(int) const override { return &texture; }
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

    int frame_count() const override { return static_cast<int>(textures.size()); }

    const ray::Texture2D* frame_texture(int frame) const override {
        if (frame >= static_cast<int>(textures.size())) {
            throw std::runtime_error("Frame " + std::to_string(frame) +
                " not available in framed texture " + name);
        }
        return &textures[frame];
    }
};

class TextureWrapper {
private:
    std::unordered_map<int, std::unique_ptr<BaseAnimation>> animations;
    std::vector<std::unique_ptr<BaseAnimation>> copied_animations;
    std::unordered_map<std::string, std::unordered_map<int, std::unique_ptr<BaseAnimation>>> screen_animations;
    fs::path graphics_path;
    fs::path parent_graphics_path;
    std::unordered_set<std::string> loaded_subsets;

public:
    std::unordered_map<uint32_t, std::shared_ptr<TextureObject>> textures;
    std::unordered_map<SC, SkinInfo> skin_config;
    std::unordered_map<std::string, SkinInfo> skin_config_by_name;
    std::unordered_map<SCO, bool> options;
    Chara3DConfig chara_3d_config;
    fs::path font_path;
    int screen_width;
    int screen_height;
    float screen_scale;
    float draw_offset_x = 0.0f;
    float draw_offset_y = 0.0f;

    TextureWrapper() : screen_width(1280), screen_height(720), screen_scale(1.0) {
    }

    void init(const fs::path& skin_path);

    const SkinInfo* skin_entry(const std::string& key) const {
        auto it = skin_config_by_name.find(key);
        return it == skin_config_by_name.end() ? nullptr : &it->second;
    }

    std::string skin_text(const std::string& key, const std::string& language,
                          const std::string& fallback = "") const {
        auto it = skin_config_by_name.find(key);
        if (it == skin_config_by_name.end()) return fallback;
        auto t = it->second.text.find(language);
        return t == it->second.text.end() ? fallback : t->second;
    }

    ~TextureWrapper() {
        unload_textures();
    }

    // Non-Graphics per-skin asset roots (Sounds/Videos/Models) have no inheritance
    // mechanism of their own ??these let those subsystems reuse the same parent-skin
    // knowledge init() already parsed from skin_config.json's screen.parent, instead
    // of every skin needing its own physical copy of everything.
    fs::path skin_root()   const { return graphics_path.parent_path(); }
    fs::path parent_root() const { return parent_graphics_path.parent_path(); }
    bool has_parent_skin() const { return parent_graphics_path != graphics_path; }

    // relative_path is skin-root-relative, e.g. "Sounds/don.wav", "Videos/op_videos".
    // Prefers the child skin's own copy; falls back to the parent's if the child
    // doesn't have it. Returns the child path unchanged if neither exists (same
    // "let the caller's own missing-file handling deal with it" behavior as before
    // this existed).
    fs::path resolve_skin_path(const fs::path& relative_path) const {
        fs::path child = skin_root() / relative_path;
        if (fs::exists(child)) return child;
        if (has_parent_skin()) {
            fs::path parent = parent_root() / relative_path;
            if (fs::exists(parent)) return parent;
        }
        return child;
    }

    void unload_textures();

    bool has_animation(const int id) const { return animations.find(id) != animations.end(); }
    BaseAnimation* get_animation(const int id, bool is_copy = false);
    BaseAnimation* get_animation(const int id, const std::string& screen_name);

    void read_tex_obj_data(const Value& tex_mapping, TextureObject* tex_obj, float scale = 1.0f);

    void load_animations(const std::string& screen_name);

    void load_folder(const std::string& screen_name, const std::string& subset);

    void unload_folder(const std::string& screen_name, const std::string& subset);

    void load_screen_textures(const std::string& screen_name);

    void control(TextureObject* tex_obj, int index = 0);

    void clear_screen(const ray::Color& color);

    TexID get_enum(const std::string& name);

    bool has_texture(const std::string& name);

    void draw_texture(uint32_t id, const DrawTextureParams& = {});
};

extern TextureWrapper tex;

extern TextureWrapper global_tex;

// TexID enum, per-subset namespaces, and tex_id_map ??auto-generated from skin texture.json files
// Usage: tex.draw_texture(YELLOW_BOX::CROWN_FC, {...})
//        tex.textures[YELLOW_BOX::CROWN_FC]->width
