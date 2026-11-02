#include "texture.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <condition_variable>
#include <cwctype>
#include <format>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string_view>
#include <thread>

#include "../../platform/platform_windows.h"
#include "../asset_cache.h"
#include "../file/filesystem.h"
#include "../file/script.h"
#include "../gfx.h"
#include "../global_data.h"
#include "../profiler.h"
#include "text.h"

// Uploaded textures by PNG path, kept across screens (with their alpha border).
struct CachedGPUTexture {
    std::shared_ptr<Texture> texture;
    Rect border;
    bool has_border;
};
static AssetCache<CachedGPUTexture>& gpu_asset_cache() {
    static AssetCache<CachedGPUTexture> cache(asset_cache_budget(128u << 20));
    return cache;
}

namespace {

    inline double json_number(const Value& v, double fallback = 0.0) {
        if (v.IsInt()) {
            return static_cast<double>(v.GetInt());
        }
        if (v.IsInt64()) {
            return static_cast<double>(v.GetInt64());
        }
        if (v.IsUint()) {
            return static_cast<double>(v.GetUint());
        }
        if (v.IsUint64()) {
            return static_cast<double>(v.GetUint64());
        }
        if (v.IsNumber()) {
            return v.GetDouble();
        }
        return fallback;
    }

    inline double json_member(const Value& o, const char* key, double fallback) {
        return o.HasMember(key) ? json_number(o[key], fallback) : fallback;
    }

} // namespace

std::string TextureWrapper::skin_text(const std::string& key, const std::string& language,
                                      const std::string& fallback) const {
    const SkinInfo* info = font_manager.get_text_info(key);
    if (!info) {
        return fallback;
    }
    auto t = info->text.find(language);
    return t == info->text.end() ? fallback : t->second;
}

void TextureWrapper::init(const fs::path& skin_path) {
    graphics_path = skin_path;

    if (!fs::exists(graphics_path)) {
        throw std::runtime_error("The skin path provided is not a valid path");
    }

    // Every entry below is only ever inserted/overwritten from what the new
    // skin's (and its parent's) json actually specifies. A key the previous
    // skin set but this one doesn't mention would otherwise survive the
    // switch with the old skin's value.
    skin_info.clear();
    skin_config_by_name.clear();
    options.clear();
    chara_3d_config = Chara3DConfig{};
    chara_positions.clear();
    nameplate_positions.clear();

    auto skin_config_file = read_json_file(graphics_path / "skin_config.json");

    // Derive screen dimensions from child config first so screen_scale is known.
    {
        float w = skin_config_file.HasMember("screen") && skin_config_file["screen"].HasMember("width")
                      ? skin_config_file["screen"]["width"].GetFloat()
                      : 1280.0f;
        float h = skin_config_file.HasMember("screen") && skin_config_file["screen"].HasMember("height")
                      ? skin_config_file["screen"]["height"].GetFloat()
                      : 720.0f;
        screen_width = static_cast<int>(w);
        screen_height = static_cast<int>(h);
        screen_scale = w / 1280.0f;
    }

    auto load_entry = [this](const std::string& name, const Value& v, float scale) {
        if (!v.IsObject()) {
            return;
        }
        float x = static_cast<float>(json_member(v, "x", 0.0)) * scale;
        float y = static_cast<float>(json_member(v, "y", 0.0)) * scale;
        int font_size = static_cast<int>(json_member(v, "font_size", 0) * scale);
        float width = static_cast<float>(json_member(v, "width", 0.0)) * scale;
        float height = static_cast<float>(json_member(v, "height", 0.0)) * scale;

        std::map<std::string, std::string> text_map;
        if (v.HasMember("text") && v["text"].IsObject()) {
            for (auto& t : v["text"].GetObject()) {
                text_map[t.name.GetString()] = t.value.GetString();
            }
        }

        float outline = static_cast<float>(json_member(v, "outline", -1.0));

        SkinInfo info(x, y, font_size, width, height, text_map, outline);

        skin_config_by_name[name] = info;

        auto sc_it = skin_config_map.find(name);
        if (sc_it != skin_config_map.end()) {
            skin_info[sc_it->second] = std::move(info);
        }
    };

    // "nameplate": an [x, y] per place the nameplate is drawn, same scaling.
    // "chara_3d": the model's scale and rotation, plus a [x, y] per place it is drawn ("entry",
    // "song_select_1p", ...). Positions are scaled like the other skin_config values.
    auto load_chara = [this](const Value& cfg, float scale) {
        if (cfg.HasMember("nameplate") && cfg["nameplate"].IsObject()) {
            for (auto& m : cfg["nameplate"].GetObject()) {
                if (m.value.IsArray() && m.value.Size() >= 2 && m.value[0].IsNumber() && m.value[1].IsNumber()) {
                    nameplate_positions[m.name.GetString()] = {m.value[0].GetFloat() * scale,
                                                               m.value[1].GetFloat() * scale};
                }
            }
        }
        if (!cfg.HasMember("chara_3d") || !cfg["chara_3d"].IsObject()) {
            return;
        }
        for (auto& m : cfg["chara_3d"].GetObject()) {
            std::string name = m.name.GetString();
            if (m.value.IsNumber()) {
                struct CharaField {
                    std::string_view name;
                    float Chara3DConfig::* member;
                };

                static constexpr CharaField fields[] = {
                    {"scale", &Chara3DConfig::scale},
                    {"rot_x", &Chara3DConfig::rot_x},
                    {"rot_y", &Chara3DConfig::rot_y},
                    {"rot_z", &Chara3DConfig::rot_z},
                };

                const auto field = std::ranges::find(fields, name, &CharaField::name);
                if (field != std::end(fields)) {
                    chara_3d_config.*(field->member) = m.value.GetFloat();
                }
            } else if (m.value.IsArray() && m.value.Size() >= 2 && m.value[0].IsNumber() && m.value[1].IsNumber()) {
                chara_positions[name] = {m.value[0].GetFloat() * scale, m.value[1].GetFloat() * scale};
            }
        }
    };

    // Ancestors (parent, its parent, ...), each scaled by this skin's width over its own
    // (screen.width, default 1280). Their skin_config loads farthest first, so nearer values
    // override, and the child's below overrides them all.
    ancestor_graphics_paths.clear();
    ancestor_scales.clear();
    {
        std::vector<fs::path> chain = resolve_skin_chain(graphics_path);
        for (size_t i = 1; i < chain.size(); i++) {
            float aw = 1280.0f;
            try {
                auto cfg = read_json_file(chain[i] / "skin_config.json");
                if (cfg.HasMember("screen") && cfg["screen"].HasMember("width") && cfg["screen"]["width"].IsNumber()) {
                    aw = cfg["screen"]["width"].GetFloat();
                }
            } catch (const std::exception& e) {
                spdlog::warn("skin ancestor {}: {}", chain[i].string(), e.what());
            }
            ancestor_graphics_paths.push_back(chain[i]);
            ancestor_scales.push_back(static_cast<float>(screen_width) / aw);
        }
    }
    parent_graphics_path = ancestor_graphics_paths.empty() ? graphics_path : ancestor_graphics_paths.front();
    for (size_t i = ancestor_graphics_paths.size(); i-- > 0;) {
        auto ancestor_config = read_json_file(ancestor_graphics_paths[i] / "skin_config.json");
        for (auto& m : ancestor_config.GetObject()) {
            load_entry(m.name.GetString(), m.value, ancestor_scales[i]);
        }
        load_chara(ancestor_config, ancestor_scales[i]);
    }

    // Load child skin_config ??overrides parent defaults.
    for (auto& m : skin_config_file.GetObject()) {
        load_entry(m.name.GetString(), m.value, 1.0f);
    }
    load_chara(skin_config_file, 1.0f);

    // a skin whose chain defines no value for a required key reads as zeroes
    for (unsigned int k = 0; k < static_cast<unsigned int>(SC::OPTIONAL_KEYS); k++) {
        skin_info.try_emplace(static_cast<SC>(k));
    }

    if (skin_config_file.HasMember("screen") && skin_config_file["screen"].HasMember("options")) {
        const Value& opts = skin_config_file["screen"]["options"];
        if (opts.IsObject()) {
            for (auto& opt : opts.GetObject()) {
                if (opt.value.IsBool()) {
                    auto it = screen_options_map.find(opt.name.GetString());
                    if (it != screen_options_map.end()) {
                        options[it->second] = opt.value.GetBool();
                    }
                }
            }
        }
    }
}

void TextureWrapper::unload_textures() {
    lua_texture_lookup.clear();
    textures.clear();
    loaded_subsets.clear();
    animations.clear();
    copied_animations.clear();
    screen_animations.clear();
    subset_loaded_ids.clear();
    tex_id_refcount.clear();
    debug_draw_log.clear();
    debug_draw_log_prev.clear();
}

BaseAnimation* TextureWrapper::get_animation(const int id, bool is_copy) {
    if (!animations.contains(id)) {
        throw std::runtime_error("Unable to find animation: " + std::to_string(id));
    }

    if (is_copy) {
        auto new_anim = animations[id]->copy();
        if (animations[id]->isStarted()) {
            new_anim->start();
        }
        // Note: Returning raw pointer from unique_ptr requires careful management
        copied_animations.push_back(std::move(new_anim));
        return copied_animations.back().get();
    }

    return animations[id].get();
}

BaseAnimation* TextureWrapper::get_animation(const int id, const std::string& screen_name) {
    if (!screen_animations.contains(screen_name)) {
        Document anim_config;
        if (read_screen_animations(screen_name, anim_config)) {
            AnimationParser parser;
            screen_animations[screen_name] = parser.parse_animations(anim_config);
        } else {
            spdlog::warn("No animation.json found for screen: {}", screen_name);
        }
    }

    auto screen_it = screen_animations.find(screen_name);
    if (screen_it == screen_animations.end()) {
        throw std::runtime_error("No animations available for screen: " + screen_name);
    }
    auto& anim_map = screen_it->second;
    auto it = anim_map.find(id);
    if (it == anim_map.end()) {
        throw std::runtime_error("Unable to find animation " + std::to_string(id) + " in screen: " + screen_name);
    }

    auto new_anim = it->second->copy();
    if (it->second->isStarted()) {
        new_anim->start();
    }
    copied_animations.push_back(std::move(new_anim));
    return copied_animations.back().get();
}

void TextureWrapper::read_tex_obj_data(const Value& tex_mapping, TextureObject* tex_obj, float scale) {
    if (tex_mapping.IsArray()) {
        // Check if crop data exists in the first mapping (index 0)
        bool has_crop_in_first = tex_mapping.Size() > 0 && tex_mapping[0].IsObject() &&
                                 tex_mapping[0].HasMember("crop") && tex_mapping[0]["crop"].IsArray() &&
                                 !tex_mapping[0]["crop"].Empty();

        std::vector<Rect> crops;
        if (has_crop_in_first) {
            const Value& first_mapping = tex_mapping[0];
            for (SizeType j = 0; j < first_mapping["crop"].Size(); j++) {
                const Value& crop = first_mapping["crop"][j];
                if (!crop.IsArray() || crop.Size() < 4) {
                    spdlog::error("Invalid crop entry {} for texture {}", j, tex_obj->name);
                    continue;
                }
                crops.emplace_back(crop[0].GetFloat(), crop[1].GetFloat(), crop[2].GetFloat(), crop[3].GetFloat());
            }
            if (crops.empty()) {
                has_crop_in_first = false;
            } else {
                tex_obj->crop_data = crops;
                tex_obj->width = static_cast<int>(crops[0].width);
                tex_obj->height = static_cast<int>(crops[0].height);
            }
        }

        tex_obj->x.resize(tex_mapping.Size());
        tex_obj->y.resize(tex_mapping.Size());
        tex_obj->x2.resize(tex_mapping.Size());
        tex_obj->y2.resize(tex_mapping.Size());

        for (SizeType i = 0; i < tex_mapping.Size(); i++) {
            const Value& mapping = tex_mapping[i];

            int x = static_cast<int>(json_member(mapping, "x", 0) * scale);
            int y = static_cast<int>(json_member(mapping, "y", 0) * scale);
            int x2 = static_cast<int>(json_member(mapping, "x2", tex_obj->width) * scale);
            int y2 = static_cast<int>(json_member(mapping, "y2", tex_obj->height) * scale);

            tex_obj->x[i] = x;
            tex_obj->y[i] = y;
            tex_obj->x2[i] = x2;
            tex_obj->y2[i] = y2;

            // Handle frame_order
            if (mapping.HasMember("frame_order") && mapping["frame_order"].IsArray()) {
                auto* framed = dynamic_cast<FramedTexture*>(tex_obj);
                if (framed) {
                    std::vector<std::shared_ptr<Texture>> reordered;
                    for (SizeType j = 0; j < mapping["frame_order"].Size(); j++) {
                        int idx = static_cast<int>(json_number(mapping["frame_order"][j]));
                        if (idx < 0 || idx >= static_cast<int>(framed->textures.size())) {
                            spdlog::error("Invalid frame_order index {} for texture {}", idx, tex_obj->name);
                            continue;
                        }
                        reordered.push_back(framed->textures[idx]);
                    }
                    framed->textures = reordered;
                }
            }

            // Apply crop dimensions to all indices if crop exists in first mapping
            if (has_crop_in_first) {
                tex_obj->x2[i] = static_cast<int>(crops[0].width * scale);
                tex_obj->y2[i] = static_cast<int>(crops[0].height * scale);
            }
        }
    } else if (tex_mapping.IsObject()) {
        if (tex_mapping.HasMember("crop") && tex_mapping["crop"].IsArray() && !tex_mapping["crop"].Empty()) {
            std::vector<Rect> crops;
            for (SizeType j = 0; j < tex_mapping["crop"].Size(); j++) {
                const Value& crop = tex_mapping["crop"][j];
                if (!crop.IsArray() || crop.Size() < 4) {
                    spdlog::error("Invalid crop entry {} for texture {}", j, tex_obj->name);
                    continue;
                }
                crops.emplace_back(crop[0].GetFloat(), crop[1].GetFloat(), crop[2].GetFloat(), crop[3].GetFloat());
            }
            if (!crops.empty()) {
                tex_obj->crop_data = crops;
                tex_obj->width = static_cast<int>(crops[0].width);
                tex_obj->height = static_cast<int>(crops[0].height);
            }
        }

        tex_obj->x = {static_cast<int>(json_member(tex_mapping, "x", 0) * scale)};
        tex_obj->y = {static_cast<int>(json_member(tex_mapping, "y", 0) * scale)};
        tex_obj->x2 = {static_cast<int>(json_member(tex_mapping, "x2", tex_obj->width) * scale)};
        tex_obj->y2 = {static_cast<int>(json_member(tex_mapping, "y2", tex_obj->height) * scale)};

        // Handle frame_order
        if (tex_mapping.HasMember("frame_order") && tex_mapping["frame_order"].IsArray()) {
            auto* framed = dynamic_cast<FramedTexture*>(tex_obj);
            if (framed) {
                std::vector<std::shared_ptr<Texture>> reordered;
                for (SizeType j = 0; j < tex_mapping["frame_order"].Size(); j++) {
                    int idx = static_cast<int>(json_number(tex_mapping["frame_order"][j]));
                    if (idx < 0 || idx >= static_cast<int>(framed->textures.size())) {
                        spdlog::error("Invalid frame_order index {} for texture {}", idx, tex_obj->name);
                        continue;
                    }
                    reordered.push_back(framed->textures[idx]);
                }
                framed->textures = reordered;
            }
        }
    }

    int filter = texture_filter::BILINEAR;
    for (int i = 0; i < tex_obj->frame_count(); i++) {
        if (const Texture* t = tex_obj->frame_texture(i)) {
            set_texture_filter(*const_cast<Texture*>(t), filter);
        }
    }
}

bool TextureWrapper::read_screen_animations(const std::string& screen_name, Document& out) {
    fs::path own = graphics_path / screen_name / "animation.json";
    if (fs::exists(own)) {
        out = read_json_file(own);
        return true;
    }
    for (size_t i = 0; i < ancestor_graphics_paths.size(); i++) {
        fs::path file = ancestor_graphics_paths[i] / screen_name / "animation.json";
        if (!fs::exists(file)) {
            continue;
        }
        out = read_json_file(file);
        const float scale = ancestor_scales[i];
        auto scale_number = [scale](Value& v) {
            if (v.IsInt()) {
                v.SetInt(static_cast<int>(json_number(v) * scale));
            } else if (v.IsDouble()) {
                v.SetDouble(v.GetDouble() * scale);
            }
        };
        if (out.IsArray()) {
            for (SizeType k = 0; k < out.Size(); k++) {
                Value& anim = out[k];
                if (anim.HasMember("total_distance") && !anim["total_distance"].IsObject()) {
                    scale_number(anim["total_distance"]);
                }
                if (anim.HasMember("start_position") && !anim["start_position"].IsObject()) {
                    scale_number(anim["start_position"]);
                }
                if (anim.HasMember("waypoints") && anim["waypoints"].IsArray()) {
                    for (auto& wp : anim["waypoints"].GetArray()) {
                        if (wp.IsObject() && wp.HasMember("value")) {
                            scale_number(wp["value"]);
                        }
                    }
                }
            }
        }
        return true;
    }
    return false;
}

void TextureWrapper::load_animations(const std::string& screen_name) {
    PROFILE_SCOPE();
    PROFILE_ZONE_TEXT(screen_name);
    Document anim_config;
    if (read_screen_animations(screen_name, anim_config)) {
        AnimationParser parser;
        animations = parser.parse_animations(anim_config);
        spdlog::info("Animations loaded for screen: {}", screen_name);
    }
}

std::unordered_map<std::string, std::weak_ptr<TextureObject>>& tex_object_cache() {
    static std::unordered_map<std::string, std::weak_ptr<TextureObject>> cache;
    return cache;
}

std::shared_ptr<Texture> shared_texture(Texture tex) {
    return std::shared_ptr<Texture>(new Texture(tex), [](Texture* p) {
        if (p->id != 0) {
            unload_texture(*p);
        }
        delete p;
    });
}

void decode_images_parallel(const std::vector<fs::path>& files, std::vector<Image>& out) {
    out.assign(files.size(), Image{});
    if (files.empty()) {
        return;
    }

    unsigned hw = std::thread::hardware_concurrency();
    size_t workers = std::min<size_t>(files.size(), hw ? hw : 4);
    if (workers <= 1) {
        for (size_t i = 0; i < files.size(); ++i) {
            out[i] = load_image(files[i].string().c_str());
        }
        return;
    }

    std::atomic<size_t> next{0};
    std::vector<std::thread> pool;
    pool.reserve(workers);
    for (size_t w = 0; w < workers; ++w) {
        pool.emplace_back([&files, &out, &next] {
            for (size_t i = next.fetch_add(1); i < files.size(); i = next.fetch_add(1)) {
                try {
                    out[i] = load_image(files[i].string().c_str());
                } catch (const std::exception& e) {
                    spdlog::error("Failed to decode image {}: {}", files[i].string(), e.what());
                    out[i] = Image{};
                }
            }
        });
    }
    for (auto& t : pool) {
        t.join();
    }
}

// A screen's texture folders, each listed once. Loading a screen asked the file system about
// every texture.json entry separately (is it a frame folder, is its PNG there) in the child skin
// and again in each ancestor, a few thousand calls for a 1080p skin -- about a third of the time
// song select took to open. Names match case-insensitively on Windows, as the file system does.
class DirIndex {
public:
    struct Entry {
        fs::path path;
        bool is_dir;
    };

    // the folder's entries, or nullptr when it cannot be listed
    const std::vector<Entry>* list(const fs::path& dir) {
        const Listing& l = get(dir);
        return l.ok ? &l.entries : nullptr;
    }
    bool exists(const fs::path& dir, const fs::path& name) { return find(dir, name) != nullptr; }
    bool is_dir(const fs::path& dir, const fs::path& name) {
        const Entry* e = find(dir, name);
        return e && e->is_dir;
    }
    bool is_file(const fs::path& dir, const fs::path& name) {
        const Entry* e = find(dir, name);
        return e && !e->is_dir;
    }

private:
    struct Listing {
        bool ok = false;
        std::vector<Entry> entries;
        std::unordered_map<fs::path::string_type, size_t> by_name;
    };
    std::unordered_map<fs::path::string_type, Listing> listings;

    static fs::path::string_type key(const fs::path& p) {
        fs::path::string_type k = p.native();
#ifdef _WIN32
        for (auto& c : k) {
            c = static_cast<wchar_t>(towlower(c));
        }
#endif
        return k;
    }

    const Listing& get(const fs::path& dir) {
        auto [it, inserted] = listings.try_emplace(key(dir));
        Listing& l = it->second;
        if (!inserted) {
            return l;
        }
#ifdef _WIN32
        std::vector<Win32DirEntry> raw;
        l.ok = win32_list_dir(dir, raw);
        for (Win32DirEntry& e : raw) {
            l.entries.push_back({dir / e.name, e.is_dir});
        }
#else
        std::error_code ec;
        fs::directory_iterator iter(dir, ec);
        l.ok = !ec;
        for (; !ec && iter != fs::directory_iterator(); iter.increment(ec)) {
            std::error_code type_ec;
            l.entries.push_back({iter->path(), iter->is_directory(type_ec)});
        }
#endif
        for (size_t i = 0; i < l.entries.size(); ++i) {
            l.by_name.emplace(key(l.entries[i].path.filename()), i);
        }
        return l;
    }

    const Entry* find(const fs::path& dir, const fs::path& name) {
        const Listing& l = get(dir);
        auto it = l.by_name.find(key(name));
        return it == l.by_name.end() ? nullptr : &l.entries[it->second];
    }
};

std::vector<fs::path> sorted_frames(DirIndex& dirs, const fs::path& dir) {
    std::vector<fs::path> frames;
    if (const auto* entries = dirs.list(dir)) {
        for (const DirIndex::Entry& entry : *entries) {
            if (!entry.is_dir) {
                frames.push_back(entry.path);
            }
        }
    }
    std::ranges::sort(frames, [](const fs::path& a, const fs::path& b) {
        const std::string as = a.stem().string(), bs = b.stem().string();
        int an = 0, bn = 0;
        auto ar = std::from_chars(as.data(), as.data() + as.size(), an);
        auto br = std::from_chars(bs.data(), bs.data() + bs.size(), bn);
        bool a_ok = ar.ec == std::errc() && ar.ptr == as.data() + as.size();
        bool b_ok = br.ec == std::errc() && br.ptr == bs.data() + bs.size();
        if (a_ok && b_ok) {
            return an < bn;
        }
        if (a_ok != b_ok) {
            return a_ok;
        }
        return as < bs;
    });
    return frames;
}

std::unordered_set<std::string> overridden_names(DirIndex& dirs, const fs::path& child_folder) {
    std::unordered_set<std::string> names;
    try {
        auto cfg = read_json_file(child_folder / "texture.json");
        for (auto& m : cfg.GetObject()) {
            std::string n = m.name.GetString();
            if (dirs.is_dir(child_folder, n) || dirs.exists(child_folder, n + ".png")) {
                names.insert(n);
            }
        }
    } catch (const std::exception&) {
        // Unreadable child config - fall back to loading everything from the parent.
        names.clear();
    }
    return names;
}

class ImagePrefetch {
public:
    ImagePrefetch(const std::vector<std::vector<fs::path>>& groups, const std::unordered_set<std::string>& border_keys,
                  size_t budget_bytes)
        : budget(budget_bytes) {
        for (size_t g = 0; g < groups.size(); ++g) {
            for (const fs::path& p : groups[g]) {
                std::string key = image_key(p);
                if (index.contains(key)) {
                    continue;
                }
                index.emplace(key, items.size());
                items.push_back(Item{p, g});
                items.back().want_border = border_keys.contains(key);
            }
        }
        unsigned hw = std::thread::hardware_concurrency();
        size_t workers = std::min<size_t>(items.size(), std::clamp<unsigned>(hw ? hw - 1 : 3, 1, 16));
        for (size_t w = 0; w < workers; ++w) {
            pool.emplace_back([this] { work(); });
        }
    }

    ~ImagePrefetch() {
        {
            std::scoped_lock lock(mutex);
            stop = true;
        }
        cv.notify_all();
        for (auto& t : pool) {
            t.join();
        }
        for (Item& it : items) {
            if (it.state == State::Done && it.image.data) {
                unload_image(it.image);
            }
        }
    }

    // The decoded image for path when it's part of this prefetch (waiting for a worker that
    // has started it, decoding it here if none has); false when it isn't.
    bool take(const fs::path& path, Image& image, Rect& border, bool& has_border) {
        std::unique_lock<std::mutex> lock(mutex);
        auto found = index.find(image_key(path));
        if (found == index.end()) {
            return false;
        }
        Item& it = items[found->second];
        cv.wait(lock, [&] { return it.state != State::Decoding; });
        if (it.state == State::Pending) {
            it.state = State::Taken;
            lock.unlock();
            has_border = decode(it.path, it.want_border, image, border);
            return true;
        }
        if (it.state != State::Done) {
            return false;
        }
        image = it.image;
        border = it.border;
        has_border = it.has_border;
        it.image = Image{};
        it.state = State::Taken;
        held -= it.bytes;
        cv.notify_all();
        return true;
    }

    // Folder group g has been loaded: whatever it (and earlier groups) left untaken goes.
    void finish_group(size_t g) {
        std::scoped_lock lock(mutex);
        finished_groups = std::max(finished_groups, g + 1);
        for (Item& it : items) {
            if (it.group > g) {
                continue;
            }
            if (it.state == State::Pending) {
                it.state = State::Dropped;
            } else if (it.state == State::Done) {
                if (it.image.data) {
                    unload_image(it.image);
                }
                it.image = Image{};
                held -= it.bytes;
                it.state = State::Dropped;
            }
        }
        cv.notify_all();
    }

    static std::string image_key(const fs::path& p) { return p.lexically_normal().generic_string(); }

private:
    enum class State {
        Pending,
        Decoding,
        Done,
        Taken,
        Dropped
    };
    struct Item {
        fs::path path;
        size_t group;
        State state = State::Pending;
        Image image{};
        Rect border;
        bool want_border = false;
        bool has_border = false;
        size_t bytes = 0;
    };

    // true when border was computed
    static bool decode(const fs::path& path, bool want_border, Image& image, Rect& border) {
        try {
            image = load_image(path.string().c_str());
        } catch (const std::exception& e) {
            spdlog::error("Failed to decode image {}: {}", path.string(), e.what());
            image = Image{};
        }
        border = Rect{};
        if (!want_border || !image.data) {
            return false;
        }
        border = get_image_alpha_border(image, 0.0f);
        return true;
    }

    void work() {
        std::unique_lock<std::mutex> lock(mutex);
        while (true) {
            cv.wait(lock, [&] { return stop || held < budget; });
            while (next < items.size() && items[next].state != State::Pending) {
                ++next;
            }
            if (stop || next >= items.size()) {
                return;
            }
            Item& it = items[next++];
            it.state = State::Decoding;
            lock.unlock();
            Image image;
            Rect border;
            const bool has_border = decode(it.path, it.want_border, image, border);
            lock.lock();
            if (finished_groups > it.group) { // its folder was loaded while this one decoded
                if (image.data) {
                    unload_image(image);
                }
                it.state = State::Dropped;
                continue;
            }
            it.image = image;
            it.border = border;
            it.has_border = has_border;
            it.bytes =
                image.data ? static_cast<size_t>(get_pixel_data_size(image.width, image.height, image.format)) : 0;
            it.state = State::Done;
            held += it.bytes;
            cv.notify_all();
        }
    }

    std::vector<Item> items;
    std::unordered_map<std::string, size_t> index;
    std::vector<std::thread> pool;
    std::mutex mutex;
    std::condition_variable cv;
    size_t next = 0;
    size_t held = 0;
    size_t finished_groups = 0; // groups below this are done with
    const size_t budget;
    bool stop = false;
};

void TextureWrapper::load_folder(const std::string& screen_name, const std::string& subset) {
    lua_texture_lookup.clear();
    // Subset leaf name is the key prefix used in textures[] (e.g. "notes_nijiiro" from "game/notes_nijiiro")
    const std::string subset_key = fs::path(subset).filename().string();
    const std::string dedup_key = screen_name + "/" + subset_key;

    if (loaded_subsets.contains(dedup_key)) {
        return;
    }

    int loaded_count = 0;
    std::unordered_set<std::string> ids_this_call;
    DirIndex local_dirs;
    DirIndex& dirs = dir_index ? *dir_index : local_dirs;

    // A texture.json entry whose PNG(s) still have to be read off disk.
    struct PendingTex {
        std::string id;
        std::string name;
        const Value* mapping;
        std::string cache_key;
        size_t first_file;
        size_t file_count;
        bool framed;
    };

    auto load_from_path = [&](const fs::path& folder, float tex_scale, const std::unordered_set<std::string>* skip) {
        fs::path tex_json = folder / "texture.json";
        if (!dirs.is_file(folder, "texture.json")) {
            return;
        }

        try {
            auto tex_config = read_json_file(tex_json);
            auto& cache = tex_object_cache();

            std::vector<PendingTex> pending;
            std::vector<fs::path> files;
            std::unordered_map<std::string, size_t> file_index;
            auto push_file = [&](const fs::path& path) -> size_t {
                std::string key = path.string();
                auto it = file_index.find(key);
                if (it != file_index.end()) {
                    return it->second;
                }
                size_t idx = files.size();
                files.push_back(path);
                file_index.emplace(std::move(key), idx);
                return idx;
            };

            for (auto& m : tex_config.GetObject()) {
                std::string tex_name = m.name.GetString();

                if (skip && skip->contains(tex_name)) {
                    continue;
                }

                std::string tex_id = std::format("{}/{}", subset_key, tex_name);

                std::string cache_key = (folder / tex_name).string();
                auto cit = cache.find(cache_key);
                if (cit != cache.end()) {
                    if (auto shared = cit->second.lock()) {
                        textures[tex_id] = shared;
                        ids_this_call.insert(tex_id);
                        ++loaded_count;
                        continue;
                    }
                    cache.erase(cit);
                }

                fs::path tex_dir = folder / tex_name;
                fs::path tex_file = folder / (tex_name + ".png");
                // "file" lets several keys share one atlas PNG, distinguished by "crop".
                bool file_override = m.value.IsObject() && m.value.HasMember("file") && m.value["file"].IsString();
                if (file_override) {
                    tex_file = folder / m.value["file"].GetString();
                }

                if (!file_override && dirs.is_dir(folder, tex_name)) {
                    auto frames = sorted_frames(dirs, tex_dir);
                    size_t first = files.size();
                    for (const auto& frame : frames) {
                        push_file(frame);
                    }
                    pending.push_back({tex_id, tex_name, &m.value, cache_key, first, frames.size(), true});
                } else if (dirs.exists(tex_file.parent_path(), tex_file.filename())) {
                    size_t first = push_file(tex_file);
                    pending.push_back({tex_id, tex_name, &m.value, cache_key, first, size_t{1}, false});
                } else {
                    auto existing = textures.find(tex_id);
                    if (existing != textures.end()) {
                        read_tex_obj_data(m.value, existing->second.get(), tex_scale);
                        ids_this_call.insert(tex_id);
                        ++loaded_count;
                    } else {
                        spdlog::error("Texture {} was not found in {}", tex_name, folder.string());
                    }
                }
            }

            // images load_screen_textures already decoded, the rest here
            std::vector<Image> images(files.size());
            std::vector<std::shared_ptr<Texture>> gpu(files.size());
            std::vector<char> needs_border(files.size(), 0);
            for (const auto& p : pending) {
                if (!p.framed) {
                    needs_border[p.first_file] = true;
                }
            }
            std::vector<Rect> borders(files.size());
            std::vector<char> has_border(files.size(), 0);
            std::vector<fs::path> rest;
            std::vector<size_t> rest_index;
            for (size_t i = 0; i < files.size(); ++i) {
                if (auto cached = gpu_asset_cache().get(files[i].string())) {
                    if (!needs_border[i] || cached->has_border) {
                        gpu[i] = cached->texture;
                        borders[i] = cached->border;
                        has_border[i] = cached->has_border;
                        continue;
                    }
                }
                bool got_border = false;
                if (image_prefetch && image_prefetch->take(files[i], images[i], borders[i], got_border)) {
                    has_border[i] = got_border;
                } else {
                    rest.push_back(files[i]);
                    rest_index.push_back(i);
                }
            }
            std::vector<Image> decoded;
            decode_images_parallel(rest, decoded);
            for (size_t k = 0; k < rest.size(); ++k) {
                images[rest_index[k]] = decoded[k];
            }

            try {
                // GPU textures indexed by file, so keys that share one atlas PNG
                // (via "file" + "crop") upload it once and share the texture.
                for (const auto& p : pending) {
                    bool ok = true;
                    std::vector<std::shared_ptr<Texture>> texs;
                    texs.reserve(p.file_count);
                    for (size_t i = 0; i < p.file_count; ++i) {
                        size_t fi = p.first_file + i;
                        std::shared_ptr<Texture> tex = gpu[fi];
                        if (!tex) {
                            const Image& img = images[fi];
                            if (!img.data) {
                                spdlog::error("Failed to decode image for texture {}: Frame {}", p.name, i);
                                ok = false;
                            } else {
                                Texture t = load_texture_from_image(img);
                                if (!is_texture_valid(t)) {
                                    spdlog::error("Failed to load texture {}: Frame {}", p.name, i);
                                    ok = false;
                                } else {
                                    tex = shared_texture(t);
                                    gpu[fi] = tex;
                                    if (gpu_asset_cache().enabled()) {
                                        // the image is freed after upload; keep its border for cache hits
                                        if (!p.framed && !has_border[fi]) {
                                            borders[fi] = get_image_alpha_border(img, 0.0f);
                                            has_border[fi] = true;
                                        }
                                        const size_t bytes =
                                            (static_cast<size_t>(get_pixel_data_size(t.width, t.height, t.format)) +
                                             65535u) &
                                            ~static_cast<size_t>(65535u);

                                        gpu_asset_cache().put(files[fi].string(),
                                                              std::make_shared<CachedGPUTexture>(CachedGPUTexture{
                                                                  tex, borders[fi], static_cast<bool>(has_border[fi])}),
                                                              bytes);
                                    }
                                }
                            }
                        }
                        texs.push_back(std::move(tex));
                    }
                    if (!ok) {
                        continue;
                    }

                    std::shared_ptr<TextureObject> obj;
                    if (p.framed) {
                        obj = std::make_shared<FramedTexture>(p.id, std::move(texs));
                    } else {
                        obj = std::make_shared<SingleTexture>(p.id, texs[0]);
                    }

                    read_tex_obj_data(*p.mapping, obj.get(), tex_scale);

                    if (!p.framed && !obj->crop_data.has_value() && p.mapping->IsObject() &&
                        !p.mapping->HasMember("x2")) {
                        const Image& img = images[p.first_file];
                        if (img.data || has_border[p.first_file]) {
                            const Rect border =
                                has_border[p.first_file] ? borders[p.first_file] : get_image_alpha_border(img, 0.0f);
                            if (border.width > 0.0f) {
                                obj->opaque_right = (border.x + border.width) * tex_scale;
                            }
                        }
                    }

                    textures[p.id] = obj;
                    cache[p.cache_key] = obj;
                    ids_this_call.insert(p.id);
                    ++loaded_count;
                }
            } catch (...) {
                for (auto& img : images) {
                    if (img.data) {
                        unload_image(img);
                    }
                }
                throw;
            }

            for (auto& img : images) {
                if (img.data) {
                    unload_image(img);
                }
            }

            spdlog::debug("Textures loaded from folder: {}", folder.string());

        } catch (const std::exception& e) {
            spdlog::error("Failed to load textures from folder {}: {}", folder.string(), e.what());
        }
    };

    // Farthest ancestor first, child last (later loads overwrite), each ancestor skipping
    // the names already provided by the child or a closer ancestor.
    const fs::path own_dir = graphics_path / screen_name / subset;
    const bool child_has_folder = ancestor_graphics_paths.empty() || dirs.is_file(own_dir, "texture.json");
    std::vector<std::unordered_set<std::string>> skips(ancestor_graphics_paths.size());
    std::unordered_set<std::string> claimed;
    if (child_has_folder && !ancestor_graphics_paths.empty()) {
        claimed = overridden_names(dirs, own_dir);
    }
    for (size_t i = 0; i < ancestor_graphics_paths.size(); i++) {
        const fs::path dir = ancestor_graphics_paths[i] / screen_name / subset;
        skips[i] = claimed;
        if (dirs.is_file(dir, "texture.json")) {
            for (const std::string& n : overridden_names(dirs, dir)) {
                claimed.insert(n);
            }
        }
    }
    for (size_t i = ancestor_graphics_paths.size(); i-- > 0;) {
        const fs::path dir = ancestor_graphics_paths[i] / screen_name / subset;
        if (dirs.is_file(dir, "texture.json")) {
            load_from_path(dir, ancestor_scales[i], &skips[i]);
        }
    }
    if (child_has_folder) {
        load_from_path(own_dir, 1.0f, nullptr);
    }

    if (loaded_count == 0) {
        spdlog::error("No textures loaded for {}/{}", screen_name, subset);
    } else {
        loaded_subsets.insert(dedup_key);
        subset_loaded_ids[dedup_key] = ids_this_call;
        for (const std::string& id : ids_this_call) {
            ++tex_id_refcount[id];
        }
    }
}

void TextureWrapper::unload_folder(const std::string& screen_name, const std::string& subset) {
    lua_texture_lookup.clear();
    const std::string subset_key = fs::path(subset).filename().string();
    const std::string dedup_key = screen_name + "/" + subset_key;

    if (!loaded_subsets.contains(dedup_key)) {
        return;
    }

    // A subset name can be shared by several screens (they resolve to the same
    // "subset/name" keys), so only drop a texture once every
    // screen/subset that loaded it has also unloaded it.
    auto& refcount = tex_id_refcount;
    auto ids_it = subset_loaded_ids.find(dedup_key);
    if (ids_it != subset_loaded_ids.end()) {
        for (const std::string& id : ids_it->second) {
            auto rc_it = refcount.find(id);
            if (rc_it == refcount.end()) {
                continue;
            }
            if (--rc_it->second <= 0) {
                textures.erase(id);
                refcount.erase(rc_it);
            }
        }
        subset_loaded_ids.erase(ids_it);
    }

    loaded_subsets.erase(dedup_key);
    spdlog::info("Textures unloaded for folder: {}/{}", screen_name, subset);
}

// decoded-but-not-yet-uploaded images load_screen_textures may hold ahead of load_folder
constexpr size_t kPrefetchBudgetBytes = size_t{128} << 20;

void TextureWrapper::load_screen_textures(const std::string& screen_name) {
    PROFILE_SCOPE();
    PROFILE_ZONE_TEXT(screen_name);
    std::vector<fs::path> roots{graphics_path};
    roots.insert(roots.end(), ancestor_graphics_paths.begin(), ancestor_graphics_paths.end());
    DirIndex dirs;
    dir_index = &dirs;
    struct ResetDirs {
        DirIndex*& p;
        ~ResetDirs() { p = nullptr; }
    } reset_dirs{dir_index};
    std::vector<fs::path> screen_dirs;
    for (const fs::path& root : roots) {
        if (dirs.is_dir(root, screen_name)) {
            screen_dirs.push_back(root / screen_name);
        }
    }

    if (screen_dirs.empty()) {
        spdlog::warn("Textures for Screen {} do not exist", screen_name);
        return;
    }

    load_animations(screen_name);

    // Subsets of the child and every ancestor, each once
    std::vector<std::string> subsets;
    std::unordered_set<std::string> seen;
    for (const fs::path& dir : screen_dirs) {
        const auto* entries = dirs.list(dir);
        if (!entries) {
            continue;
        }
        for (const DirIndex::Entry& entry : *entries) {
            if (!entry.is_dir) {
                continue;
            }
            std::string name = entry.path.filename().string();
            if (seen.insert(name).second && !loaded_subsets.contains(std::format("{}/{}", screen_name, name))) {
                subsets.push_back(std::move(name));
            }
        }
    }

    // every PNG those folders hold (frame folders included), one group per subset
    std::vector<std::vector<fs::path>> groups(subsets.size());
    std::unordered_set<std::string> border_keys;
    for (size_t g = 0; g < subsets.size(); ++g) {
        for (const fs::path& root : roots) {
            const fs::path dir = root / screen_name / subsets[g];
            if (!dirs.is_file(dir, "texture.json")) {
                continue;
            }

            try {
                auto cfg = read_json_file(dir / "texture.json");
                for (auto& m : cfg.GetObject()) {
                    if (!m.value.IsObject() || m.value.HasMember("x2") || m.value.HasMember("crop")) {
                        continue;
                    }
                    const bool file_override = m.value.HasMember("file") && m.value["file"].IsString();
                    const std::string name = m.name.GetString();
                    if (!file_override && dirs.is_dir(dir, name)) {
                        continue;
                    }
                    border_keys.insert(ImagePrefetch::image_key(
                        dir / (file_override ? std::string(m.value["file"].GetString()) : name + ".png")));
                }
            } catch (const std::exception&) {
            }
            const auto* entries = dirs.list(dir);
            if (!entries) {
                continue;
            }
            for (const DirIndex::Entry& entry : *entries) {
                const fs::path& p = entry.path;
                if (entry.is_dir) {
                    if (const auto* frames = dirs.list(p)) {
                        for (const DirIndex::Entry& frame : *frames) {
                            if (!frame.is_dir && frame.path.extension() == ".png") {
                                if (gpu_asset_cache().get(frame.path.string())) {
                                    continue;
                                }
                                groups[g].push_back(frame.path);
                            }
                        }
                    }
                } else if (p.extension() == ".png") {
                    if (gpu_asset_cache().get(p.string())) {
                        continue;
                    }
                    groups[g].push_back(p);
                }
            }
        }
    }

    {
        ImagePrefetch prefetch(groups, border_keys, kPrefetchBudgetBytes);
        image_prefetch = &prefetch;
        struct Reset {
            ImagePrefetch*& p;
            ~Reset() { p = nullptr; }
        } reset{image_prefetch};
        for (size_t g = 0; g < subsets.size(); ++g) {
            load_folder(screen_name, subsets[g]);
            prefetch.finish_group(g);
        }
    }

    spdlog::info("Screen textures loaded for: {}", screen_name);
}

void TextureWrapper::clear_screen(const Color& color) {
    clear_background(color);
}

std::vector<std::string> TextureWrapper::language_variants(const std::string& name) const {
    std::vector<std::string> out{name};
    if (!global_data.config) {
        return out;
    }
    const std::string& lang = global_data.config->general.language;
    const std::string suffix = "_" + lang;
    if (name.size() <= suffix.size() || !name.ends_with(suffix)) {
        return out;
    }
    const std::string base = name.substr(0, name.size() - suffix.size());
    for (const char* fb : {"ja", "en"}) { // the cabinet's own order: untranslated rows draw from the Japanese set
        if (lang != fb) {
            out.push_back(base + "_" + fb);
        }
    }
    out.push_back(base); // art with no language variants at all
    return out;
}

TextureObject* TextureWrapper::get_texture(const std::string& name) {
    auto it = textures.find(name);
    if (it != textures.end()) {
        return it->second.get();
    }
    return get_texture_shared(name).get();
}

std::shared_ptr<TextureObject> TextureWrapper::get_texture_shared(const std::string& name) {
    if (auto hit = textures.find(name); hit != textures.end()) {
        return hit->second;
    }
    for (const auto& v : language_variants(name)) {
        auto it = textures.find(v);
        if (it != textures.end()) {
            return it->second;
        }
    }
    spdlog::warn("Texture not found: {}", name);
    auto it = textures.find("kidou/warning");
    return it != textures.end() ? it->second : nullptr;
}

bool TextureWrapper::has_texture(const std::string& name) {
    if (textures.contains(name)) {
        return true;
    }
    return std::ranges::any_of(language_variants(name), [this](const auto& v) { return textures.contains(v); });
}

void TextureWrapper::draw_texture(TextureObject* tex_obj, const DrawTextureParams& params) {
    if (!tex_obj) {
        return;
    }

    const float mirror_x = (params.mirror == Mirror::HORIZONTAL) ? -1.0f : 1.0f;
    const float mirror_y = (params.mirror == Mirror::VERTICAL) ? -1.0f : 1.0f;

    const Color final_color = (params.fade != 1.1f) ? fade(params.color, params.fade) : params.color;

    Rect source_rect;
    if (params.src.has_value()) {
        source_rect = params.src.value();
    } else if (tex_obj->crop_data.has_value()) {
        try {
            source_rect = tex_obj->crop_data->at(params.frame);
            source_rect.width -= 1.0f;
            source_rect.height -= 1.0f;
            source_rect.x += mirror_x < 0 ? -0.5f : 0.5f;
            source_rect.y += mirror_y < 0 ? -0.5f : 0.5f;
            source_rect.width *= mirror_x;
            source_rect.height *= mirror_y;
        } catch (const std::out_of_range& e) {
            spdlog::error("Frame index out of range for texture {}", tex_obj->name);
            spdlog::error("Frame index: {}, Number of frames: {}", params.frame, tex_obj->crop_data->size());
            throw;
        }
    } else {
        const float width = static_cast<float>(tex_obj->width);
        const float height = static_cast<float>(tex_obj->height);
        source_rect = Rect{0, 0, width * mirror_x, height * mirror_y};
    }

    if (params.index < 0 || static_cast<size_t>(params.index) >= tex_obj->x.size()) {
        spdlog::error("Draw index {} out of range for texture {}", params.index, tex_obj->name);
        return;
    }

    // Calculate destination rectangle with reduced redundant calculations
    const float base_x = tex_obj->x[params.index];
    const float base_y = tex_obj->y[params.index];

    Rect dest_rect;
    if (params.center) {
        const float x2v = tex_obj->x2[params.index];
        const float y2v = tex_obj->y2[params.index];
        const float half_width = x2v * 0.5f;
        const float half_height = y2v * 0.5f;
        const float scaled_half_width = (x2v * params.scale) * 0.5f;
        const float scaled_half_height = (y2v * params.scale) * 0.5f;

        dest_rect = Rect{base_x + draw_offset_x + half_width - scaled_half_width + params.x,
                         base_y + draw_offset_y + half_height - scaled_half_height + params.y,
                         x2v * params.scale + params.x2, y2v * params.scale + params.y2};
    } else {
        dest_rect = Rect{base_x + draw_offset_x + params.x, base_y + draw_offset_y + params.y,
                         tex_obj->x2[params.index] * params.scale + params.x2,
                         tex_obj->y2[params.index] * params.scale + params.y2};
    }

    if (debug_log_draws) {
        debug_draw_log.push_back({.name = tex_obj->name,
                                  .rect = dest_rect,
                                  .tex_obj = tex_obj,
                                  .index = params.index,
                                  .offset_x = params.x,
                                  .offset_y = params.y,
                                  .scale = params.scale,
                                  .center = params.center,
                                  .origin = params.origin,
                                  .rotation = params.rotation});
        log_lua_site(debug_draw_log.back(), script_lua_state());
    }

    const Texture* frame_tex = tex_obj->frame_texture(params.frame);
    if (frame_tex) {
        if (params.blend.has_value()) {
            begin_blend_mode(params.blend.value());
            draw_texture_pro(*frame_tex, source_rect, dest_rect, params.origin, params.rotation, final_color);
            end_blend_mode();
            return;
        }
        draw_texture_pro(*frame_tex, source_rect, dest_rect, params.origin, params.rotation, final_color);
    }
}

TextureWrapper tex;

TextureWrapper global_tex;
