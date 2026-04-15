#include "texture.h"

void TextureWrapper::init(const fs::path& skin_path) {
    graphics_path = skin_path;

    if (!fs::exists(graphics_path)) {
        spdlog::error("No skin has been configured");
        return;
    }

    parent_graphics_path = graphics_path;
    fs::path config_file = graphics_path / "skin_config.json";

    if (!fs::exists(config_file)) {
        throw std::runtime_error("Skin is missing a skin_config.json");
    }

    std::ifstream ifs(config_file);
    IStreamWrapper isw(ifs);
    Document doc;
    doc.ParseStream(isw);

    if (doc.HasParseError()) {
        throw std::runtime_error("Failed to parse skin_config.json");
    }

    for (auto& m : doc.GetObject()) {
        SC key = skin_config_map.at(m.name.GetString());
        const Value& v = m.value;

        float x = v.HasMember("x") ? v["x"].GetFloat() : 0;
        float y = v.HasMember("y") ? v["y"].GetFloat() : 0;
        int font_size = v.HasMember("font_size") ? v["font_size"].GetInt() : 0;
        float width = v.HasMember("width") ? v["width"].GetFloat() : 0;
        float height = v.HasMember("height") ? v["height"].GetFloat() : 0;

        std::map<std::string, std::string> text_map;
        if (v.HasMember("text") && v["text"].IsObject()) {
            for (auto& t : v["text"].GetObject()) {
                text_map[t.name.GetString()] = t.value.GetString();
            }
        }

        skin_config[key] = SkinInfo(x, y, font_size, width, height, text_map);
    }

    screen_width = static_cast<int>(skin_config[SC::SCREEN].width);
    screen_height = static_cast<int>(skin_config[SC::SCREEN].height);
    screen_scale = screen_width / 1280.0f;

    if (doc.HasMember("screen") && doc["screen"].HasMember("parent")) {
        std::string parent = doc["screen"]["parent"].GetString();
        parent_graphics_path = fs::path("Skins") / parent / "Graphics";

        fs::path parent_config = parent_graphics_path / "skin_config.json";
        std::ifstream parent_ifs(parent_config);
        IStreamWrapper parent_isw(parent_ifs);
        Document parent_doc;
        parent_doc.ParseStream(parent_isw);

        for (auto& m : parent_doc.GetObject()) {
            SC key = skin_config_map.at(m.name.GetString());
            const Value& v = m.value;

            float x = (v.HasMember("x") ? v["x"].GetFloat() : 0) * screen_scale;
            float y = (v.HasMember("y") ? v["y"].GetFloat() : 0) * screen_scale;
            int font_size = static_cast<int>((v.HasMember("font_size") ? v["font_size"].GetInt() : 0) * screen_scale);
            float width = (v.HasMember("width") ? v["width"].GetFloat() : 0) * screen_scale;
            float height = (v.HasMember("height") ? v["height"].GetFloat() : 0) * screen_scale;

            std::map<std::string, std::string> text_map;
            if (v.HasMember("text") && v["text"].IsObject()) {
                for (auto& t : v["text"].GetObject()) {
                    text_map[t.name.GetString()] = t.value.GetString();
                }
            }

            skin_config[key] = SkinInfo(x, y, font_size, width, height, text_map);
        }
    }
}

void TextureWrapper::unload_textures() {
    textures.clear();
    loaded_subsets.clear();
    animations.clear();
    copied_animations.clear();
    spdlog::info("All textures unloaded");
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

    if (animations[id]->isStarted()) {
        animations[id]->start();
    }
    return animations[id].get();
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

            int x = static_cast<int>((mapping.HasMember("x") ? mapping["x"].GetInt() : 0) * scale);
            int y = static_cast<int>((mapping.HasMember("y") ? mapping["y"].GetInt() : 0) * scale);
            int x2 = static_cast<int>((mapping.HasMember("x2") ? mapping["x2"].GetInt() : tex_obj->width) * scale);
            int y2 = static_cast<int>((mapping.HasMember("y2") ? mapping["y2"].GetInt() : tex_obj->height) * scale);

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
                        int idx = mapping["frame_order"][j].GetInt();
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

        tex_obj->x = {static_cast<int>((tex_mapping.HasMember("x") ? tex_mapping["x"].GetInt() : 0) * scale)};
        tex_obj->y = {static_cast<int>((tex_mapping.HasMember("y") ? tex_mapping["y"].GetInt() : 0) * scale)};
        tex_obj->x2 = {static_cast<int>((tex_mapping.HasMember("x2") ? tex_mapping["x2"].GetInt() : tex_obj->width) * scale)};
        tex_obj->y2 = {static_cast<int>((tex_mapping.HasMember("y2") ? tex_mapping["y2"].GetInt() : tex_obj->height) * scale)};

        // Handle frame_order
        if (tex_mapping.HasMember("frame_order") && tex_mapping["frame_order"].IsArray()) {
            auto* framed = dynamic_cast<FramedTexture*>(tex_obj);
            if (framed) {
                std::vector<ray::Texture2D> reordered;
                for (SizeType j = 0; j < tex_mapping["frame_order"].Size(); j++) {
                    int idx = tex_mapping["frame_order"][j].GetInt();
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
        std::ifstream ifs(anim_file);
        IStreamWrapper isw(ifs);
        Document doc;
        doc.ParseStream(isw);

        if (doc.HasParseError()) {
            throw std::runtime_error(&"Failed to parse animation.json: " [ doc.GetParseError()]);
        }

        AnimationParser parser;
        animations = parser.parse_animations(doc);
        spdlog::info("Animations loaded for screen: {}", screen_name);
    } else if (parent_graphics_path != graphics_path && fs::exists(parent_anim_file)) {
        std::ifstream ifs(parent_anim_file);
        IStreamWrapper isw(ifs);
        Document doc;
        doc.ParseStream(isw);

        // Scale total_distance values
        if (doc.IsArray()) {
            for (SizeType i = 0; i < doc.Size(); i++) {
                Value& anim = doc[i];
                if (anim.HasMember("total_distance") && !anim["total_distance"].IsObject()) {
                    if (anim["total_distance"].IsInt()) {
                        int val = anim["total_distance"].GetInt();
                        anim["total_distance"].SetInt(static_cast<int>(val * screen_scale));
                    } else if (anim["total_distance"].IsDouble()) {
                        double val = anim["total_distance"].GetDouble();
                        anim["total_distance"].SetDouble(val * screen_scale);
                    }
                }
                if (anim.HasMember("start_position") && !anim["start_position"].IsObject()) {
                    if (anim["start_position"].IsInt()) {
                        int val = anim["start_position"].GetInt();
                        anim["start_position"].SetInt(static_cast<int>(val * screen_scale));
                    } else if (anim["start_position"].IsDouble()) {
                        double val = anim["start_position"].GetDouble();
                        anim["start_position"].SetDouble(val * screen_scale);
                    }
                }
            }
        }

        AnimationParser parser;
        animations = parser.parse_animations(doc);
        spdlog::info("Animations loaded for screen: {}", screen_name);
    }
}

void TextureWrapper::load_folder(const std::string& screen_name, const std::string& subset) {
    // Subset leaf name is the key used in tex_id_map (e.g. "notes_nijiiro" from "game/notes_nijiiro")
    const std::string subset_key = fs::path(subset).filename().string();

    if (loaded_subsets.count(subset_key)) return;

    int loaded_count = 0;

    auto load_from_path = [&](const fs::path& folder, float tex_scale) {
        fs::path tex_json = folder / "texture.json";
        if (!fs::exists(tex_json)) return;

        try {
            std::ifstream ifs(tex_json);
            IStreamWrapper isw(ifs);
            Document doc;
            doc.ParseStream(isw);

            if (doc.HasParseError()) {
                throw std::runtime_error("Failed to parse texture.json in " + folder.string());
            }

            for (auto& m : doc.GetObject()) {
                std::string tex_name = m.name.GetString();
                const Value& tex_mapping = m.value;

                std::string map_key = subset_key + "/" + tex_name;
                auto id_it = tex_id_map.find(map_key);
                if (id_it == tex_id_map.end()) {
                    spdlog::warn("Texture %s has no generated TexID — skipping",
                                  map_key.c_str());
                    continue;
                }
                uint32_t tex_id = static_cast<uint32_t>(id_it->second);

                fs::path tex_dir = folder / tex_name;
                fs::path tex_file = folder / (tex_name + ".png");

                if (fs::is_directory(tex_dir)) {
                    std::vector<fs::path> frames;
                    for (const auto& entry : fs::directory_iterator(tex_dir)) {
                        if (entry.is_regular_file()) {
                            frames.push_back(entry.path());
                        }
                    }
                    std::sort(frames.begin(), frames.end(), [](const fs::path& a, const fs::path& b) {
                        return std::stoi(a.stem().string()) < std::stoi(b.stem().string());
                    });

                    std::vector<ray::Texture2D> loaded_frames;
                    for (const auto& frame : frames) {
                        loaded_frames.push_back(ray::LoadTexture(frame.string().c_str()));
                    }

                    auto framed = std::make_shared<FramedTexture>(tex_name, loaded_frames);
                    read_tex_obj_data(tex_mapping, framed.get(), tex_scale);
                    textures[tex_id] = framed;
                    ++loaded_count;

                } else if (fs::exists(tex_file)) {
                    ray::Texture2D tex = ray::LoadTexture(tex_file.string().c_str());
                    auto single = std::make_shared<SingleTexture>(tex_name, tex);
                    read_tex_obj_data(tex_mapping, single.get(), tex_scale);
                    textures[tex_id] = single;
                    ++loaded_count;

                } else {
                    spdlog::error("Texture %s was not found in %s",
                           tex_name.c_str(), folder.string().c_str());
                }
            }

            spdlog::info("Textures loaded from folder: {}", folder.string());

        } catch (const std::exception& e) {
            spdlog::error("Failed to load textures from folder {}: {}",
                   folder.string(), e.what());
        }
    };

    // Load parent textures first, then child on top — child entries override parent,
    // but parent-only textures are preserved.
    if (parent_graphics_path != graphics_path) {
        load_from_path(parent_graphics_path / screen_name / subset, screen_scale);
    }
    load_from_path(graphics_path / screen_name / subset, 1.0f);

    if (loaded_count == 0) {
        spdlog::error("No textures loaded for {}/{}", screen_name, subset);
    } else {
        loaded_subsets.insert(subset_key);
    }
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

void TextureWrapper::draw_texture(uint32_t id, const DrawTextureParams& params) {
    auto it = textures.find(id);
    if (it == textures.end()) return;

    TextureObject* tex_obj = it->second.get();

    const float mirror_x = (params.mirror == "horizontal") ? -1.0f : 1.0f;
    const float mirror_y = (params.mirror == "vertical") ? -1.0f : 1.0f;

    const ray::Color final_color = (params.fade != 1.1f) ? Fade(params.color, params.fade) : params.color;

    ray::Rectangle source_rect;
    if (params.src.has_value()) {
        source_rect = params.src.value();
    } else if (tex_obj->crop_data.has_value()) {
        try {
            source_rect = tex_obj->crop_data->at(params.frame);
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
            base_x + half_width - scaled_half_width + params.x,
            base_y + half_height - scaled_half_height + params.y,
            tex_obj->x2[params.index] * params.scale + params.x2,
            tex_obj->y2[params.index] * params.scale + params.y2
        };
    } else {
        dest_rect = ray::Rectangle{
            base_x + params.x,
            base_y + params.y,
            tex_obj->x2[params.index] * params.scale + params.x2,
            tex_obj->y2[params.index] * params.scale + params.y2
        };
    }

    auto* framed = dynamic_cast<FramedTexture*>(tex_obj);
    if (framed) {
        if (params.frame >= static_cast<int>(framed->textures.size())) {
            throw std::runtime_error("Frame " + std::to_string(params.frame) +
                " not available in framed texture " + tex_obj->name);
        }
        DrawTexturePro(framed->textures[params.frame], source_rect, dest_rect,
                      params.origin, params.rotation, final_color);
    } else {
        auto* single = dynamic_cast<SingleTexture*>(tex_obj);
        if (single) {
            DrawTexturePro(single->texture, source_rect, dest_rect,
                         params.origin, params.rotation, final_color);
        }
    }
}

TextureWrapper tex;

TextureWrapper global_tex;
