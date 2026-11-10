#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../gfx.h" // IWYU pragma: keep
#include "../math/animation.h"
#include "../skin_config.h"
#include "color_utils.h"

namespace fs = std::filesystem;

enum class Mirror : uint8_t {
    NONE,
    HORIZONTAL,
    VERTICAL
};

inline Mirror mirror_from_string(const std::string& s) {
    if (s == "horizontal") {
        return Mirror::HORIZONTAL;
    }
    if (s == "vertical") {
        return Mirror::VERTICAL;
    }
    return Mirror::NONE;
}

struct DrawTextureParams {
    Color color = Color::WHITE;
    int frame = 0;
    float scale = 1.0f;
    bool center = false;
    Mirror mirror = Mirror::NONE;
    float x = 0, y = 0, x2 = 0, y2 = 0;
    Vec2 origin = {0, 0};
    float rotation = 0;
    double fade = 1.1f;
    int index = 0;
    std::optional<Rect> src = std::nullopt;
    std::optional<int> blend = std::nullopt;
};

struct SkinInfo {
    float x;
    float y;
    int font_size;
    float width;
    float height;
    std::map<std::string, std::string> text;
    float outline = -1.0f;

    // Where this entry came from, so the debug menu can write edits back.
    std::filesystem::path json_path;
    float json_scale = 1.0f;

    SkinInfo(float x = 0, float y = 0, int font_size = 0, float width = 0, float height = 0,
             const std::map<std::string, std::string>& text = {}, float outline = -1.0f)
        : x(x), y(y), font_size(font_size), width(width), height(height), text(text), outline(outline) {}
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
    std::optional<std::vector<Rect>> crop_data;

    // Where x/y/x2/y2 came from, so the debug menu can write edits back.
    std::filesystem::path json_path;
    std::string json_key;
    float json_scale = 1.0f;

    float opaque_right = 0.0f;
    TextureObject(std::string name, int width, int height)
        : name(std::move(name)), width(width), height(height), x{0}, y{0}, x2{width}, y2{height} {}

    virtual ~TextureObject() = default;

    float scissor_right() const {
        if (opaque_right > 0.0f) {
            return opaque_right;
        }
        return x2.empty() ? 0.0f : static_cast<float>(x2[0]);
    }

    virtual const Texture* frame_texture(int frame) const { return nullptr; }

    virtual int frame_count() const { return 1; }
};

struct SingleTexture : public TextureObject {
    std::shared_ptr<Texture> texture;

    SingleTexture(const std::string& name, std::shared_ptr<Texture> tex)
        : TextureObject(name, tex->width, tex->height), texture(std::move(tex)) {
        set_texture_wrap(*texture, texture_wrap::CLAMP);
    }

    const Texture* frame_texture(int) const override { return texture.get(); }
};

struct FramedTexture : public TextureObject {
    std::vector<std::shared_ptr<Texture>> textures;

    FramedTexture(const std::string& name, std::vector<std::shared_ptr<Texture>> texs)
        : TextureObject(name, texs.empty() ? 0 : texs[0]->width, texs.empty() ? 0 : texs[0]->height),
          textures(std::move(texs)) {
        for (auto& tex : textures) {
            set_texture_wrap(*tex, texture_wrap::CLAMP);
        }
    }

    int frame_count() const override { return static_cast<int>(textures.size()); }

    const Texture* frame_texture(int frame) const override {
        if (frame < 0 || frame >= static_cast<int>(textures.size())) {
            throw std::runtime_error("Frame " + std::to_string(frame) + " not available in framed texture " + name);
        }
        return textures[frame].get();
    }
};

// Writes obj's x/y/x2/y2 at index back into the texture.json it was loaded from.
bool save_texture_position(const TextureObject& obj, int index);

class OutlinedText;

struct DrawLogEntry {
    std::string name;
    Rect rect;
    TextureObject* tex_obj = nullptr;
    OutlinedText* text_obj = nullptr;
    int index = 0;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scale = 1.0f;
    bool center = false;
    Vec2 origin = {0, 0};
    float rotation = 0.0f;
    bool from_lua = false;
    std::string lua_source;
    int lua_line = 0;
    std::string lua_function;
    int lua_defined_line = 0;
};

inline bool debug_log_draws = false;
inline std::vector<DrawLogEntry> debug_draw_log;
inline std::vector<DrawLogEntry> debug_draw_log_prev;

class TextureWrapper {
private:
    std::unordered_map<int, std::unique_ptr<BaseAnimation>> animations;
    std::vector<std::unique_ptr<BaseAnimation>> copied_animations;
    std::unordered_map<std::string, std::unordered_map<int, std::unique_ptr<BaseAnimation>>> screen_animations;
    fs::path graphics_path;
    fs::path parent_graphics_path;
    std::vector<fs::path> ancestor_graphics_paths;
    std::vector<float> ancestor_scales;
    std::unordered_set<std::string> loaded_subsets;
    std::unordered_map<std::string, int> tex_id_refcount;
    std::unordered_map<std::string, std::unordered_set<std::string>> subset_loaded_ids;
    // set while load_screen_textures runs: the screen's PNGs, decoded ahead of load_folder
    class ImagePrefetch* image_prefetch = nullptr;
    // set while load_screen_textures runs: the skin folders' listings, read once for the screen
    class DirIndex* dir_index = nullptr;

    bool read_screen_animations(const std::string& screen_name, rapidjson::Document& out);

public:
    std::unordered_map<std::string, std::shared_ptr<TextureObject>> textures;
    // Lua tex.get_texture(path) results by the exact path string (nullptr = not found): skins call
    // it every frame. Emptied whenever `textures` changes, and when the language changes.
    std::unordered_map<std::string, std::shared_ptr<TextureObject>> lua_texture_lookup;
    std::string lua_texture_lookup_lang;
    std::unordered_map<SC, SkinInfo> skin_info; // read through get_skin_config
    std::unordered_map<std::string, SkinInfo> skin_config_by_name;
    std::unordered_map<SCO, bool> options;
    Chara3DConfig chara_3d_config;
    std::map<std::string, std::pair<float, float>> chara_positions;
    std::map<std::string, std::pair<float, float>> nameplate_positions;

    std::pair<float, float> chara_xy(const std::string& name, const std::string& fallback = "") const {
        return lookup_xy(chara_positions, name, fallback);
    }

    std::pair<float, float> nameplate_xy(const std::string& name, const std::string& fallback = "") const {
        return lookup_xy(nameplate_positions, name, fallback);
    }
    static std::pair<float, float> lookup_xy(const std::map<std::string, std::pair<float, float>>& m,
                                             const std::string& name, const std::string& fallback) {
        auto it = m.find(name);
        if (it == m.end()) {
            it = m.find(fallback);
        }
        return it == m.end() ? std::pair<float, float>{0.0f, 0.0f} : it->second;
    }
    fs::path font_path;
    int screen_width{1280};
    int screen_height{720};
    float screen_scale{1.0};
    float draw_offset_x = 0.0f;
    float draw_offset_y = 0.0f;

    TextureWrapper() = default;

    void init(const fs::path& skin_path);

    const SkinInfo* get_skin_config(SC key) const {
        auto it = skin_info.find(key);
        return it == skin_info.end() ? nullptr : &it->second;
    }

    // text_config label for `key` in exactly `language` (see font_manager.get_text_label)
    std::string skin_text(const std::string& key, const std::string& language, const std::string& fallback = "") const;

    fs::path skin_root() const { return graphics_path.parent_path(); }
    fs::path parent_root() const { return parent_graphics_path.parent_path(); }
    bool has_parent_skin() const { return parent_graphics_path != graphics_path; }
    // Graphics dirs of the skin and all its ancestors, nearest first
    std::vector<fs::path> all_graphics_roots() const {
        std::vector<fs::path> r{graphics_path};
        r.insert(r.end(), ancestor_graphics_paths.begin(), ancestor_graphics_paths.end());
        return r;
    }

    fs::path resolve_skin_path(const fs::path& relative_path) const {
        if (relative_path.is_absolute()) {
            return {};
        }
        for (const auto& part : relative_path) {
            if (part == "..") {
                return {};
            }
        }
        fs::path child = skin_root() / relative_path;
        if (fs::exists(child)) {
            return child;
        }
        for (const fs::path& ancestor : ancestor_graphics_paths) {
            fs::path p = ancestor.parent_path() / relative_path;
            if (fs::exists(p)) {
                return p;
            }
        }
        return child;
    }

    void unload_textures();

    bool has_animation(const int id) const { return animations.contains(id); }
    BaseAnimation* get_animation(int id, bool is_copy = false);
    BaseAnimation* get_animation(int id, const std::string& screen_name);

    void read_tex_obj_data(const Value& tex_mapping, TextureObject* tex_obj, float scale = 1.0f);

    void load_animations(const std::string& screen_name);

    void load_folder(const std::string& screen_name, const std::string& subset);

    void unload_folder(const std::string& screen_name, const std::string& subset);

    void load_screen_textures(const std::string& screen_name);

    void control(TextureObject* tex_obj, int index = 0);

    void clear_screen(const Color& color);

    TextureObject* get_texture(const std::string& name);

    std::shared_ptr<TextureObject> get_texture_shared(const std::string& name);
    std::vector<std::string> language_variants(const std::string& name) const;

    bool has_texture(const std::string& name);

    void draw_texture(TextureObject* tex_obj, const DrawTextureParams& = {});
};

extern TextureWrapper tex;

extern TextureWrapper global_tex;
