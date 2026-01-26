#include "texture.h"

void TextureWrapper::init(const std::string& skin_path) {
    graphics_path = fs::path("Skins") / skin_path / "Graphics";

    if (!fs::exists(graphics_path)) {
        TraceLog(LOG_ERROR, "No skin has been configured");
        return;
    }

    parent_graphics_path = graphics_path;
    fs::path config_file = graphics_path / "skin_config.json";

    if (!fs::exists(config_file)) {
        throw std::runtime_error("Skin is missing a skin_config.json");
    }

    // Load skin config
    std::ifstream ifs(config_file);
    IStreamWrapper isw(ifs);
    Document doc;
    doc.ParseStream(isw);

    if (doc.HasParseError()) {
        throw std::runtime_error("Failed to parse skin_config.json");
    }

    // Parse skin config
    for (auto& m : doc.GetObject()) {
        std::string key = m.name.GetString();
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

    screen_width = static_cast<int>(skin_config["screen"].width);
    screen_height = static_cast<int>(skin_config["screen"].height);
    screen_scale = screen_width / 1280.0f;

    // Handle parent skin
    if (doc.HasMember("screen") && doc["screen"].HasMember("parent")) {
        std::string parent = doc["screen"]["parent"].GetString();
        parent_graphics_path = fs::path("Skins") / parent;

        fs::path parent_config = parent_graphics_path / "skin_config.json";
        std::ifstream parent_ifs(parent_config);
        IStreamWrapper parent_isw(parent_ifs);
        Document parent_doc;
        parent_doc.ParseStream(parent_isw);

        for (auto& m : parent_doc.GetObject()) {
            std::string key = m.name.GetString();
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
    std::map<unsigned int, std::string> ids;

    for (auto& [subset, tex_map] : textures) {
        for (auto& [name, tex_obj] : tex_map) {
            auto* framed = dynamic_cast<FramedTexture*>(tex_obj.get());
            if (framed) {
                for (size_t i = 0; i < framed->textures.size(); i++) {
                    unsigned int id = framed->textures[i].id;
                    std::string tex_name = subset + "/" + name + "[" + std::to_string(i) + "]";

                    if (ids.find(id) != ids.end()) {
                        TraceLog(LOG_WARNING, "Duplicate texture ID %u: %s and %s",
                               id, ids[id].c_str(), tex_name.c_str());
                    } else {
                        ids[id] = tex_name;
                    }
                }
            } else {
                auto* single = dynamic_cast<SingleTexture*>(tex_obj.get());
                if (single) {
                    unsigned int id = single->texture.id;
                    std::string tex_name = subset + "/" + name;

                    if (ids.find(id) != ids.end()) {
                        TraceLog(LOG_WARNING, "Duplicate texture ID %u: %s and %s",
                               id, ids[id].c_str(), tex_name.c_str());
                    } else {
                        ids[id] = tex_name;
                    }
                }
            }
        }
    }

    textures.clear();
    animations.clear();
    TraceLog(LOG_INFO, "All textures unloaded");
}

BaseAnimation* TextureWrapper::get_animation(const int id, bool is_copy) {
    if (animations.find(id) == animations.end()) {
        throw std::runtime_error(&"Unable to find animation: " [ id]);
    }

    if (is_copy) {
        auto new_anim = animations[id]->copy();
        if (animations[id]->isStarted()) {
            new_anim->start();
        }
        // Note: Returning raw pointer from unique_ptr requires careful management
        return new_anim.release();
    }

    if (animations[id]->isStarted()) {
        animations[id]->start();
    }
    return animations[id].get();
}

void TextureWrapper::read_tex_obj_data(const Value& tex_mapping, TextureObject* tex_obj) {
    if (tex_mapping.IsArray()) {
        for (SizeType i = 0; i < tex_mapping.Size(); i++) {
            const Value& mapping = tex_mapping[i];

            int x = mapping.HasMember("x") ? mapping["x"].GetInt() : 0;
            int y = mapping.HasMember("y") ? mapping["y"].GetInt() : 0;
            int x2 = mapping.HasMember("x2") ? mapping["x2"].GetInt() : tex_obj->width;
            int y2 = mapping.HasMember("y2") ? mapping["y2"].GetInt() : tex_obj->height;
            bool ctrl = mapping.HasMember("controllable") && mapping["controllable"].GetBool();

            if (i == 0) {
                tex_obj->x[0] = x;
                tex_obj->y[0] = y;
                tex_obj->x2[0] = x2;
                tex_obj->y2[0] = y2;
                tex_obj->controllable[0] = ctrl;
            } else {
                tex_obj->x.push_back(x);
                tex_obj->y.push_back(y);
                tex_obj->x2.push_back(x2);
                tex_obj->y2.push_back(y2);
                tex_obj->controllable.push_back(ctrl);
            }

            // Handle frame_order
            if (mapping.HasMember("frame_order") && mapping["frame_order"].IsArray()) {
                auto* framed = dynamic_cast<FramedTexture*>(tex_obj);
                if (framed) {
                    std::vector<Texture2D> reordered;
                    for (SizeType j = 0; j < mapping["frame_order"].Size(); j++) {
                        int idx = mapping["frame_order"][j].GetInt();
                        reordered.push_back(framed->textures[idx]);
                    }
                    framed->textures = reordered;
                }
            }

            // Handle crop
            if (mapping.HasMember("crop") && mapping["crop"].IsArray()) {
                std::vector<Rectangle> crops;
                for (SizeType j = 0; j < mapping["crop"].Size(); j++) {
                    const Value& crop = mapping["crop"][j];
                    crops.push_back(Rectangle{
                        crop[0].GetFloat(), crop[1].GetFloat(),
                        crop[2].GetFloat(), crop[3].GetFloat()
                    });
                }
                tex_obj->crop_data = crops;
                tex_obj->x2[i] = crops[0].width;
                tex_obj->y2[i] = crops[0].height;
            }
        }
    } else if (tex_mapping.IsObject()) {
        tex_obj->x = {tex_mapping.HasMember("x") ? tex_mapping["x"].GetInt() : 0};
        tex_obj->y = {tex_mapping.HasMember("y") ? tex_mapping["y"].GetInt() : 0};
        tex_obj->x2 = {tex_mapping.HasMember("x2") ? tex_mapping["x2"].GetInt() : tex_obj->width};
        tex_obj->y2 = {tex_mapping.HasMember("y2") ? tex_mapping["y2"].GetInt() : tex_obj->height};
        tex_obj->controllable = {tex_mapping.HasMember("controllable") && tex_mapping["controllable"].GetBool()};

        // Handle frame_order
        if (tex_mapping.HasMember("frame_order") && tex_mapping["frame_order"].IsArray()) {
            auto* framed = dynamic_cast<FramedTexture*>(tex_obj);
            if (framed) {
                std::vector<Texture2D> reordered;
                for (SizeType j = 0; j < tex_mapping["frame_order"].Size(); j++) {
                    int idx = tex_mapping["frame_order"][j].GetInt();
                    reordered.push_back(framed->textures[idx]);
                }
                framed->textures = reordered;
            }
        }

        // Handle crop
        if (tex_mapping.HasMember("crop") && tex_mapping["crop"].IsArray()) {
            std::vector<Rectangle> crops;
            for (SizeType j = 0; j < tex_mapping["crop"].Size(); j++) {
                const Value& crop = tex_mapping["crop"][j];
                crops.push_back(Rectangle{
                    crop[0].GetFloat(), crop[1].GetFloat(),
                    crop[2].GetFloat(), crop[3].GetFloat()
                });
            }
            tex_obj->crop_data = crops;
            tex_obj->x2 = {static_cast<int>(crops[0].width)};
            tex_obj->y2 = {static_cast<int>(crops[0].height)};
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

        AnimationParser parser;
        animations = parser.parse_animations(doc);
        TraceLog(LOG_INFO, "Animations loaded for screen: %s", screen_name.c_str());
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
            }
        }

        AnimationParser parser;
        animations = parser.parse_animations(doc);
        TraceLog(LOG_INFO, "Animations loaded for screen: %s (from parent)", screen_name.c_str());
    }
}

void TextureWrapper::load_folder(const std::string& screen_name, const std::string& subset) {
    fs::path folder = graphics_path / screen_name / subset;

    if (textures.find(screen_name) != textures.end() &&
        textures[screen_name].find(subset) != textures[screen_name].end()) {
        return;
    }

    try {
        fs::path tex_json = folder / "texture.json";
        if (!fs::exists(tex_json)) {
            throw std::runtime_error("texture.json file missing from " + folder.string());
        }

        std::ifstream ifs(tex_json);
        IStreamWrapper isw(ifs);
        Document doc;
        doc.ParseStream(isw);

        textures[folder.stem().string()] = {};

        for (auto& m : doc.GetObject()) {
            std::string tex_name = m.name.GetString();
            const Value& tex_mapping = m.value;

            fs::path tex_dir = folder / tex_name;
            fs::path tex_file = folder / (tex_name + ".png");

            if (fs::is_directory(tex_dir)) {
                // Load framed texture
                std::vector<fs::path> frames;
                for (const auto& entry : fs::directory_iterator(tex_dir)) {
                    if (entry.is_regular_file()) {
                        frames.push_back(entry.path());
                    }
                }

                // Sort frames by numeric stem
                std::sort(frames.begin(), frames.end(), [](const fs::path& a, const fs::path& b) {
                    return std::stoi(a.stem().string()) < std::stoi(b.stem().string());
                });

                std::vector<Texture2D> loaded_frames;
                for (const auto& frame : frames) {
                    loaded_frames.push_back(LoadTexture(frame.string().c_str()));
                }

                auto framed = std::make_shared<FramedTexture>(tex_name, loaded_frames);
                read_tex_obj_data(tex_mapping, framed.get());
                textures[folder.stem().string()][tex_name] = framed;

            } else if (fs::exists(tex_file)) {
                // Load single texture
                Texture2D tex = LoadTexture(tex_file.string().c_str());
                auto single = std::make_shared<SingleTexture>(tex_name, tex);
                read_tex_obj_data(tex_mapping, single.get());
                textures[folder.stem().string()][tex_name] = single;

            } else {
                TraceLog(LOG_ERROR, "Texture %s was not found in %s",
                       tex_name.c_str(), folder.string().c_str());
            }
        }

        TraceLog(LOG_INFO, "Textures loaded from folder: %s", folder.string().c_str());

    } catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "Failed to load textures from folder %s: %s",
               folder.string().c_str(), e.what());
    }
}

void TextureWrapper::load_screen_textures(const std::string& screen_name) {
    fs::path screen_path = graphics_path / screen_name;

    if (!fs::exists(screen_path)) {
        TraceLog(LOG_WARNING, "Textures for Screen %s do not exist", screen_name.c_str());
        return;
    }

    load_animations(screen_name);

    for (const auto& entry : fs::directory_iterator(screen_path)) {
        if (entry.is_directory()) {
            load_folder(screen_name, entry.path().stem().string());
        }
    }

    TraceLog(LOG_INFO, "Screen textures loaded for: %s", screen_name.c_str());
}

void TextureWrapper::control(TextureObject* tex_obj, int index) {
    int distance = IsKeyDown(KEY_LEFT_SHIFT) ? 10 : 1;

    if (IsKeyPressed(KEY_LEFT)) {
        tex_obj->x[index] -= distance;
        //TraceLog(LOG_INFO, "%s: %d, %d", tex_obj->name.c_str(),
               //tex_obj->x[index], tex_obj->y[index]);
    }
    if (IsKeyPressed(KEY_RIGHT)) {
        tex_obj->x[index] += distance;
        //TraceLog(LOG_INFO, "%s: %d, %d", tex_obj->name.c_str(),
               //tex_obj->x[index], tex_obj->y[index]);
    }
    if (IsKeyPressed(KEY_UP)) {
        tex_obj->y[index] -= distance;
        //TraceLog(LOG_INFO, "%s: %d, %d", tex_obj->name.c_str(),
               //tex_obj->x[index], tex_obj->y[index]);
    }
    if (IsKeyPressed(KEY_DOWN)) {
        tex_obj->y[index] += distance;
        //TraceLog(LOG_INFO, "%s: %d, %d", tex_obj->name.c_str(),
               //tex_obj->x[index], tex_obj->y[index]);
    }
}

void TextureWrapper::clear_screen(const Color& color) {
    ClearBackground(color);
}

void TextureWrapper::draw_texture(const std::string& subset, const std::string& texture_name,
    const DrawTextureParams& params) {

    if (textures.find(subset) == textures.end()) return;
    if (textures[subset].find(texture_name) == textures[subset].end()) return;

    float mirror_x = (params.mirror == "horizontal") ? -1.0f : 1.0f;
    float mirror_y = (params.mirror == "vertical") ? -1.0f : 1.0f;

    Color final_color = (params.fade != 1.1f) ? Fade(params.color, params.fade) : params.color;

    TextureObject* tex_obj = textures[subset][texture_name].get();

    Rectangle source_rect;
    if (params.src.has_value()) {
        source_rect = params.src.value();
    } else if (tex_obj->crop_data.has_value()) {
        source_rect = (*tex_obj->crop_data)[params.frame];
    } else {
        source_rect = Rectangle{0, 0,
            static_cast<float>(tex_obj->width) * mirror_x,
            static_cast<float>(tex_obj->height) * mirror_y};
    }

    Rectangle dest_rect;
    if (params.center) {
        dest_rect = Rectangle{
            tex_obj->x[params.index] + (tex_obj->width / 2.0f) - ((tex_obj->width * params.scale) / 2.0f) + params.x,
            tex_obj->y[params.index] + (tex_obj->height / 2.0f) - ((tex_obj->height * params.scale) / 2.0f) + params.y,
            tex_obj->x2[params.index] * params.scale + params.x2,
            tex_obj->y2[params.index] * params.scale + params.y2
        };
    } else {
        dest_rect = Rectangle{
            static_cast<float>(tex_obj->x[params.index]) + params.x,
            static_cast<float>(tex_obj->y[params.index]) + params.y,
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

    if (tex_obj->controllable[params.index] || params.controllable) {
        control(tex_obj, params.index);
    }
}

TextureWrapper tex;
