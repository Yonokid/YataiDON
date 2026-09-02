#include "texture.h"
#include "filesystem.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>
#include <atomic>

namespace {

inline double json_number(const Value& v, double fallback = 0.0) {
    if (v.IsInt())    return static_cast<double>(v.GetInt());
    if (v.IsInt64())  return static_cast<double>(v.GetInt64());
    if (v.IsUint())   return static_cast<double>(v.GetUint());
    if (v.IsUint64()) return static_cast<double>(v.GetUint64());
    if (v.IsNumber()) return v.GetDouble();
    return fallback;
}

inline double json_member(const Value& o, const char* key, double fallback) {
    return o.HasMember(key) ? json_number(o[key], fallback) : fallback;
}

}  // namespace

void TextureWrapper::init(const fs::path& skin_path) {
    graphics_path = skin_path;

    if (!fs::exists(graphics_path)) {
        throw std::runtime_error("The skin path provided is not a valid path");
    }

    auto skin_config_file = read_json_file(graphics_path / "skin_config.json");

    // Derive screen dimensions from child config first so screen_scale is known.
    {
        float w = skin_config_file.HasMember("screen") && skin_config_file["screen"].HasMember("width")
                      ? skin_config_file["screen"]["width"].GetFloat() : 1280.0f;
        float h = skin_config_file.HasMember("screen") && skin_config_file["screen"].HasMember("height")
                      ? skin_config_file["screen"]["height"].GetFloat() : 720.0f;
        screen_width  = static_cast<int>(w);
        screen_height = static_cast<int>(h);
        screen_scale  = w / 1280.0f;
    }

    auto load_entry = [this](const std::string& name, const Value& v, float scale) {
        float x = (v.HasMember("x") ? v["x"].GetFloat() : 0) * scale;
        float y = (v.HasMember("y") ? v["y"].GetFloat() : 0) * scale;
        int font_size = static_cast<int>(json_member(v, "font_size", 0) * scale);
        float width = (v.HasMember("width") ? v["width"].GetFloat() : 0) * scale;
        float height = (v.HasMember("height") ? v["height"].GetFloat() : 0) * scale;

        std::map<std::string, std::string> text_map;
        if (v.HasMember("text") && v["text"].IsObject()) {
            for (auto& t : v["text"].GetObject()) {
                text_map[t.name.GetString()] = t.value.GetString();
            }
        }

        float outline = v.HasMember("outline") ? v["outline"].GetFloat() : -1.0f;

        SkinInfo info(x, y, font_size, width, height, text_map, outline);

        skin_config_by_name[name] = info;

        auto sc_it = skin_config_map.find(name);
        if (sc_it != skin_config_map.end()) {
            skin_config[sc_it->second] = std::move(info);
        }
    };

    // Load parent skin_config first so child values override.
    parent_graphics_path = resolve_parent_graphics_path(graphics_path);
    if (parent_graphics_path != graphics_path) {
        auto parent_config = read_json_file(parent_graphics_path / "skin_config.json");

        for (auto& m : parent_config.GetObject()) {
            load_entry(m.name.GetString(), m.value, screen_scale);
        }
    }

    // Load child skin_config ??overrides parent defaults.
    for (auto& m : skin_config_file.GetObject()) {
        load_entry(m.name.GetString(), m.value, 1.0f);
    }

    if (skin_config_file.HasMember("screen") && skin_config_file["screen"].HasMember("chara_3d")) {
        const Value& c3d = skin_config_file["screen"]["chara_3d"];
        if (c3d.IsObject()) {
            if (c3d.HasMember("scale"))  chara_3d_config.scale  = c3d["scale"].GetFloat();
            if (c3d.HasMember("rot_x"))  chara_3d_config.rot_x  = c3d["rot_x"].GetFloat();
            if (c3d.HasMember("rot_y"))  chara_3d_config.rot_y  = c3d["rot_y"].GetFloat();
            if (c3d.HasMember("rot_z"))  chara_3d_config.rot_z  = c3d["rot_z"].GetFloat();
        }
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
    textures.clear();
    loaded_subsets.clear();
    animations.clear();
    copied_animations.clear();
    screen_animations.clear();
}

BaseAnimation* TextureWrapper::get_animation(const int id, bool is_copy) {
    if (animations.find(id) == animations.end()) {
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
    if (screen_animations.find(screen_name) == screen_animations.end()) {
        fs::path screen_path        = graphics_path / screen_name;
        fs::path parent_screen_path = parent_graphics_path / screen_name;
        fs::path anim_file          = screen_path / "animation.json";
        fs::path parent_anim_file   = parent_screen_path / "animation.json";

        if (fs::exists(anim_file)) {
            AnimationParser parser;
            screen_animations[screen_name] = parser.parse_animations(read_json_file(anim_file));
        } else if (parent_graphics_path != graphics_path && fs::exists(parent_anim_file)) {
            auto anim_config = read_json_file(parent_anim_file);
            if (anim_config.IsArray()) {
                for (SizeType i = 0; i < anim_config.Size(); i++) {
                    auto& anim = anim_config[i];
                    if (anim.HasMember("total_distance") && !anim["total_distance"].IsObject()) {
                        if (anim["total_distance"].IsInt())
                            anim["total_distance"].SetInt(static_cast<int>(json_number(anim["total_distance"]) * screen_scale));
                        else if (anim["total_distance"].IsDouble())
                            anim["total_distance"].SetDouble(anim["total_distance"].GetDouble() * screen_scale);
                    }
                    if (anim.HasMember("waypoint") && !anim["waypoint"].IsObject()) {
                        if (anim["waypoint"].IsInt())
                            anim["waypoint"].SetInt(static_cast<int>(json_number(anim["waypoint"]) * screen_scale));
                        else if (anim["waypoint"].IsDouble())
                            anim["waypoint"].SetDouble(anim["waypoint"].GetDouble() * screen_scale);
                    }
                    if (anim.HasMember("start_position") && !anim["start_position"].IsObject()) {
                        if (anim["start_position"].IsInt())
                            anim["start_position"].SetInt(static_cast<int>(json_number(anim["start_position"]) * screen_scale));
                        else if (anim["start_position"].IsDouble())
                            anim["start_position"].SetDouble(anim["start_position"].GetDouble() * screen_scale);
                    }
                }
            }
            AnimationParser parser;
            screen_animations[screen_name] = parser.parse_animations(anim_config);
        } else {
            spdlog::warn("No animation.json found for screen: {}", screen_name);
        }
    }

    auto& anim_map = screen_animations.at(screen_name);
    auto it = anim_map.find(id);
    if (it == anim_map.end()) {
        throw std::runtime_error("Unable to find animation " + std::to_string(id) + " in screen: " + screen_name);
    }

    auto new_anim = it->second->copy();
    if (it->second->isStarted()) new_anim->start();
    copied_animations.push_back(std::move(new_anim));
    return copied_animations.back().get();
}

void TextureWrapper::read_tex_obj_data(const Value& tex_mapping, TextureObject* tex_obj, float scale) {
    if (tex_mapping.IsArray()) {
        // Check if crop data exists in the first mapping (index 0)
        bool has_crop_in_first = tex_mapping.Size() > 0 &&
                                 tex_mapping[0].IsObject() &&
                                 tex_mapping[0].HasMember("crop") &&
                                 tex_mapping[0]["crop"].IsArray();

        std::vector<ray::Rectangle> crops;
        if (has_crop_in_first) {
            const Value& first_mapping = tex_mapping[0];
            for (SizeType j = 0; j < first_mapping["crop"].Size(); j++) {
                const Value& crop = first_mapping["crop"][j];
                crops.push_back(ray::Rectangle{
                    crop[0].GetFloat(), crop[1].GetFloat(),
                    crop[2].GetFloat(), crop[3].GetFloat()
                });
            }
            tex_obj->crop_data = crops;
            tex_obj->width = static_cast<int>(crops[0].width);
            tex_obj->height = static_cast<int>(crops[0].height);
        }

        for (SizeType i = 0; i < tex_mapping.Size(); i++) {
            const Value& mapping = tex_mapping[i];

            int x = static_cast<int>(json_member(mapping, "x", 0) * scale);
            int y = static_cast<int>(json_member(mapping, "y", 0) * scale);
            int x2 = static_cast<int>(json_member(mapping, "x2", tex_obj->width) * scale);
            int y2 = static_cast<int>(json_member(mapping, "y2", tex_obj->height) * scale);

            if (i == 0) {
                tex_obj->x[0] = x;
                tex_obj->y[0] = y;
                tex_obj->x2[0] = x2;
                tex_obj->y2[0] = y2;
            } else {
                tex_obj->x.push_back(x);
                tex_obj->y.push_back(y);
                tex_obj->x2.push_back(x2);
                tex_obj->y2.push_back(y2);
            }

            // Handle frame_order
            if (mapping.HasMember("frame_order") && mapping["frame_order"].IsArray()) {
                auto* framed = dynamic_cast<FramedTexture*>(tex_obj);
                if (framed) {
                    std::vector<ray::Texture2D> reordered;
                    for (SizeType j = 0; j < mapping["frame_order"].Size(); j++) {
                        int idx = static_cast<int>(json_number(mapping["frame_order"][j]));
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
        if (tex_mapping.HasMember("crop") && tex_mapping["crop"].IsArray()) {
            std::vector<ray::Rectangle> crops;
            for (SizeType j = 0; j < tex_mapping["crop"].Size(); j++) {
                const Value& crop = tex_mapping["crop"][j];
                crops.push_back(ray::Rectangle{
                    crop[0].GetFloat(), crop[1].GetFloat(),
                    crop[2].GetFloat(), crop[3].GetFloat()
                });
            }
            tex_obj->crop_data = crops;
            tex_obj->width = static_cast<int>(crops[0].width);
            tex_obj->height = static_cast<int>(crops[0].height);
        }

        tex_obj->x = {static_cast<int>(json_member(tex_mapping, "x", 0) * scale)};
        tex_obj->y = {static_cast<int>(json_member(tex_mapping, "y", 0) * scale)};
        tex_obj->x2 = {static_cast<int>(json_member(tex_mapping, "x2", tex_obj->width) * scale)};
        tex_obj->y2 = {static_cast<int>(json_member(tex_mapping, "y2", tex_obj->height) * scale)};

        // Handle frame_order
        if (tex_mapping.HasMember("frame_order") && tex_mapping["frame_order"].IsArray()) {
            auto* framed = dynamic_cast<FramedTexture*>(tex_obj);
            if (framed) {
                std::vector<ray::Texture2D> reordered;
                for (SizeType j = 0; j < tex_mapping["frame_order"].Size(); j++) {
                    int idx = static_cast<int>(json_number(tex_mapping["frame_order"][j]));
                    reordered.push_back(framed->textures[idx]);
                }
                framed->textures = reordered;
            }
        }
    }
}

void TextureWrapper::load_animations(const std::string& screen_name) {
    fs::path screen_path = graphics_path / screen_name;
    fs::path parent_screen_path = parent_graphics_path / screen_name;
    fs::path anim_file = screen_path / "animation.json";
    fs::path parent_anim_file = parent_screen_path / "animation.json";

    if (fs::exists(anim_file)) {
        auto anim_config = read_json_file(anim_file);

        AnimationParser parser;
        animations = parser.parse_animations(anim_config);
        spdlog::info("Animations loaded for screen: {}", screen_name);
    } else if (parent_graphics_path != graphics_path && fs::exists(parent_anim_file)) {
        auto anim_config = read_json_file(parent_anim_file);

        // Scale total_distance values
        if (anim_config.IsArray()) {
            for (SizeType i = 0; i < anim_config.Size(); i++) {
                Value& anim = anim_config[i];
                if (anim.HasMember("total_distance") && !anim["total_distance"].IsObject()) {
                    if (anim["total_distance"].IsInt()) {
                        int val = static_cast<int>(json_number(anim["total_distance"]));
                        anim["total_distance"].SetInt(static_cast<int>(val * screen_scale));
                    } else if (anim["total_distance"].IsDouble()) {
                        double val = anim["total_distance"].GetDouble();
                        anim["total_distance"].SetDouble(val * screen_scale);
                    }
                }
                if (anim.HasMember("start_position") && !anim["start_position"].IsObject()) {
                    if (anim["start_position"].IsInt()) {
                        int val = static_cast<int>(json_number(anim["start_position"]));
                        anim["start_position"].SetInt(static_cast<int>(val * screen_scale));
                    } else if (anim["start_position"].IsDouble()) {
                        double val = anim["start_position"].GetDouble();
                        anim["start_position"].SetDouble(val * screen_scale);
                    }
                }
                if (anim.HasMember("waypoint") && !anim["waypoint"].IsObject()) {
                    if (anim["waypoint"].IsInt()) {
                        int val = static_cast<int>(json_number(anim["waypoint"]));
                        anim["waypoint"].SetInt(static_cast<int>(val * screen_scale));
                    } else if (anim["waypoint"].IsDouble()) {
                        double val = anim["waypoint"].GetDouble();
                        anim["waypoint"].SetDouble(val * screen_scale);
                    }
                }
            }
        }

        AnimationParser parser;
        animations = parser.parse_animations(anim_config);
        spdlog::info("Animations loaded for screen: {}", screen_name);
    }
}

std::unordered_map<std::string, std::weak_ptr<TextureObject>>& tex_object_cache() {
    static std::unordered_map<std::string, std::weak_ptr<TextureObject>> cache;
    return cache;
}

void decode_images_parallel(const std::vector<fs::path>& files, std::vector<ray::Image>& out) {
    out.assign(files.size(), ray::Image{});
    if (files.empty()) return;

    unsigned hw = std::thread::hardware_concurrency();
    size_t workers = std::min<size_t>(files.size(), hw ? hw : 4);
    if (workers <= 1) {
        for (size_t i = 0; i < files.size(); ++i)
            out[i] = ray::LoadImage(files[i].string().c_str());
        return;
    }

    std::atomic<size_t> next{0};
    std::vector<std::thread> pool;
    pool.reserve(workers);
    for (size_t w = 0; w < workers; ++w) {
        pool.emplace_back([&files, &out, &next]() {
            for (size_t i = next.fetch_add(1); i < files.size(); i = next.fetch_add(1))
                out[i] = ray::LoadImage(files[i].string().c_str());
        });
    }
    for (auto& t : pool) t.join();
}

std::vector<fs::path> sorted_frames(const fs::path& dir) {
    std::vector<fs::path> frames;
    for (const auto& entry : fs::directory_iterator(dir))
        if (entry.is_regular_file()) frames.push_back(entry.path());
    std::sort(frames.begin(), frames.end(), [](const fs::path& a, const fs::path& b) {
        return std::stoi(a.stem().string()) < std::stoi(b.stem().string());
    });
    return frames;
}

std::unordered_set<std::string> overridden_names(const fs::path& child_folder) {
    std::unordered_set<std::string> names;
    try {
        auto cfg = read_json_file(child_folder / "texture.json");
        for (auto& m : cfg.GetObject()) {
            std::string n = m.name.GetString();
            if (fs::is_directory(child_folder / n) || fs::exists(child_folder / (n + ".png")))
                names.insert(n);
        }
    } catch (const std::exception&) {
        // Unreadable child config - fall back to loading everything from the parent.
        names.clear();
    }
    return names;
}

void TextureWrapper::load_folder(const std::string& screen_name, const std::string& subset) {
    // Subset leaf name is the key used in tex_id_map (e.g. "notes_nijiiro" from "game/notes_nijiiro")
    const std::string subset_key = fs::path(subset).filename().string();
    const std::string dedup_key = screen_name + "/" + subset_key;

    if (loaded_subsets.count(dedup_key)) return;

    int loaded_count = 0;

    // A texture.json entry whose PNG(s) still have to be read off disk.
    struct PendingTex {
        uint32_t id;
        std::string name;
        const Value* mapping;
        std::string cache_key;
        size_t first_file;
        size_t file_count;
        bool framed;
    };

    auto load_from_path = [&](const fs::path& folder, float tex_scale,
                              const std::unordered_set<std::string>* skip) {
        fs::path tex_json = folder / "texture.json";
        if (!fs::exists(tex_json)) return;

        try {
            auto tex_config = read_json_file(tex_json);
            auto& cache = tex_object_cache();

            std::vector<PendingTex> pending;
            std::vector<fs::path> files;

            for (auto& m : tex_config.GetObject()) {
                std::string tex_name = m.name.GetString();

                if (skip && skip->count(tex_name)) continue;

                std::string map_key = subset_key + "/" + tex_name;
                auto id_it = tex_id_map.find(map_key);
                if (id_it == tex_id_map.end()) {
                    spdlog::warn("Texture %s has no generated TexID ??skipping",
                                  map_key.c_str());
                    continue;
                }
                uint32_t tex_id = static_cast<uint32_t>(id_it->second);

                std::string cache_key = (folder / tex_name).string();
                auto cit = cache.find(cache_key);
                if (cit != cache.end()) {
                    if (auto shared = cit->second.lock()) {
                        textures[tex_id] = shared;
                        ++loaded_count;
                        continue;
                    }
                    cache.erase(cit);
                }

                fs::path tex_dir = folder / tex_name;
                fs::path tex_file = folder / (tex_name + ".png");

                if (fs::is_directory(tex_dir)) {
                    auto frames = sorted_frames(tex_dir);
                    pending.push_back({tex_id, tex_name, &m.value, cache_key,
                                       files.size(), frames.size(), true});
                    files.insert(files.end(), frames.begin(), frames.end());
                } else if (fs::exists(tex_file)) {
                    pending.push_back({tex_id, tex_name, &m.value, cache_key,
                                       files.size(), size_t{1}, false});
                    files.push_back(tex_file);
                } else {
                    auto existing = textures.find(tex_id);
                    if (existing != textures.end()) {
                        read_tex_obj_data(m.value, existing->second.get(), tex_scale);
                    } else {
                        spdlog::error("Texture {} was not found in {}",
                               tex_name, folder.string());
                    }
                }
            }

            std::vector<ray::Image> images;
            decode_images_parallel(files, images);

            for (const auto& p : pending) {
                bool ok = true;
                std::vector<ray::Texture2D> texs;
                texs.reserve(p.file_count);
                for (size_t i = 0; i < p.file_count; ++i) {
                    ray::Texture2D t = ray::LoadTextureFromImage(images[p.first_file + i]);
                    if (!ray::IsTextureValid(t)) {
                        spdlog::error("Failed to load texture {}: Frame {}", p.name, i);
                        ok = false;
                    }
                    texs.push_back(t);
                }
                if (!ok) {
                    for (auto& t : texs) if (ray::IsTextureValid(t)) ray::UnloadTexture(t);
                    continue;
                }

                std::shared_ptr<TextureObject> obj;
                if (p.framed) obj = std::make_shared<FramedTexture>(p.name, texs);
                else          obj = std::make_shared<SingleTexture>(p.name, texs[0]);

                read_tex_obj_data(*p.mapping, obj.get(), tex_scale);
                textures[p.id] = obj;
                cache[p.cache_key] = obj;
                ++loaded_count;
            }

            for (auto& img : images)
                if (img.data) ray::UnloadImage(img);

            spdlog::debug("Textures loaded from folder: {}", folder.string());

        } catch (const std::exception& e) {
            spdlog::error("Failed to load textures from folder {}: {}",
                   folder.string(), e.what());
        }
    };

    const bool child_has_folder =
        parent_graphics_path == graphics_path ||
        fs::exists(graphics_path / screen_name / subset / "texture.json");

    if (parent_graphics_path != graphics_path &&
        fs::exists(parent_graphics_path / screen_name / subset / "texture.json")) {
        std::unordered_set<std::string> overridden;
        if (child_has_folder)
            overridden = overridden_names(graphics_path / screen_name / subset);
        load_from_path(parent_graphics_path / screen_name / subset, screen_scale, &overridden);
    }
    if (child_has_folder) {
        load_from_path(graphics_path / screen_name / subset, 1.0f, nullptr);
    }

    if (loaded_count == 0) {
        spdlog::error("No textures loaded for {}/{}", screen_name, subset);
    } else {
        loaded_subsets.insert(dedup_key);
    }
}

void TextureWrapper::unload_folder(const std::string& screen_name, const std::string& subset) {
    const std::string subset_key = fs::path(subset).filename().string();
    const std::string dedup_key = screen_name + "/" + subset_key;

    if (!loaded_subsets.count(dedup_key)) return;

    const std::string prefix = subset_key + "/";
    for (const auto& [path, id] : tex_id_map) {
        if (path.size() >= prefix.size() && path.substr(0, prefix.size()) == prefix) {
            textures.erase(static_cast<uint32_t>(id));
        }
    }

    loaded_subsets.erase(dedup_key);
    spdlog::info("Textures unloaded for folder: {}/{}", screen_name, subset);
}

void TextureWrapper::load_screen_textures(const std::string& screen_name) {
    fs::path screen_path = graphics_path / screen_name;
    fs::path parent_screen_path = parent_graphics_path / screen_name;

    bool child_exists = fs::exists(screen_path);
    bool parent_exists = parent_graphics_path != graphics_path && fs::exists(parent_screen_path);

    if (!child_exists && !parent_exists) {
        spdlog::warn("Textures for Screen {} do not exist", screen_name);
        return;
    }

    load_animations(screen_name);

    if (child_exists) {
        for (const auto& entry : fs::directory_iterator(screen_path)) {
            if (entry.is_directory()) {
                load_folder(screen_name, entry.path().stem().string());
            }
        }
    }

    // Load subsets from parent that are not present in the child skin
    if (parent_exists) {
        for (const auto& entry : fs::directory_iterator(parent_screen_path)) {
            if (entry.is_directory()) {
                load_folder(screen_name, entry.path().stem().string());
            }
        }
    }

    spdlog::info("Screen textures loaded for: {}", screen_name);
}

void TextureWrapper::clear_screen(const ray::Color& color) {
    ray::ClearBackground(color);
}
TexID TextureWrapper::get_enum(const std::string& name) {
    try {
        return tex_id_map.at(name);
    } catch (const std::out_of_range& e) {
        spdlog::warn("Texture not found: {}", name);
        return TexID::KIDOU__WARNING;
    }
}

bool TextureWrapper::has_texture(const std::string& name) {
    auto it = tex_id_map.find(name);
    return it != tex_id_map.end() &&
           textures.find(static_cast<uint32_t>(it->second)) != textures.end();
}

void TextureWrapper::draw_texture(uint32_t id, const DrawTextureParams& params) {
    auto it = textures.find(id);
    if (it == textures.end()) return;

    TextureObject* tex_obj = it->second.get();

    const float mirror_x = (params.mirror == Mirror::HORIZONTAL) ? -1.0f : 1.0f;
    const float mirror_y = (params.mirror == Mirror::VERTICAL) ? -1.0f : 1.0f;

    const ray::Color final_color = (params.fade != 1.1f) ? Fade(params.color, params.fade) : params.color;

    ray::Rectangle source_rect;
    if (params.src.has_value()) {
        source_rect = params.src.value();
    } else if (tex_obj->crop_data.has_value()) {
        try {
            source_rect = tex_obj->crop_data->at(params.frame);
            source_rect.height = static_cast<float>(tex_obj->height) * mirror_y;
        } catch (const std::out_of_range& e) {
            spdlog::error("Frame index out of range for texture {}", tex_obj->name);
            spdlog::error("Frame index: {}, Number of frames: {}", params.frame, tex_obj->crop_data->size());
            throw;
        }
    } else {
        const float width = static_cast<float>(tex_obj->width);
        const float height = static_cast<float>(tex_obj->height);
        source_rect = ray::Rectangle{0, 0, width * mirror_x, height * mirror_y};
    }

    // Calculate destination rectangle with reduced redundant calculations
    const float base_x = tex_obj->x[params.index];
    const float base_y = tex_obj->y[params.index];
    const float width = static_cast<float>(tex_obj->width);
    const float height = static_cast<float>(tex_obj->height);

    ray::Rectangle dest_rect;
    if (params.center) {
        const float half_width = width * 0.5f;
        const float half_height = height * 0.5f;
        const float scaled_half_width = (width * params.scale) * 0.5f;
        const float scaled_half_height = (height * params.scale) * 0.5f;

        dest_rect = ray::Rectangle{
            base_x + draw_offset_x + half_width - scaled_half_width + params.x,
            base_y + draw_offset_y + half_height - scaled_half_height + params.y,
            tex_obj->x2[params.index] * params.scale + params.x2,
            tex_obj->y2[params.index] * params.scale + params.y2
        };
    } else {
        dest_rect = ray::Rectangle{
            base_x + draw_offset_x + params.x,
            base_y + draw_offset_y + params.y,
            tex_obj->x2[params.index] * params.scale + params.x2,
            tex_obj->y2[params.index] * params.scale + params.y2
        };
    }

    const ray::Texture2D* frame_tex = tex_obj->frame_texture(params.frame);
    if (frame_tex) {
        if (params.blend.has_value()) {
            ray::BeginBlendMode(params.blend.value());
            DrawTexturePro(*frame_tex, source_rect, dest_rect,
                          params.origin, params.rotation, final_color);
            ray::BeginBlendMode(ray::BLEND_CUSTOM_SEPARATE);
            return;
        }
        DrawTexturePro(*frame_tex, source_rect, dest_rect,
                      params.origin, params.rotation, final_color);
    }
}

TextureWrapper tex;

TextureWrapper global_tex;
