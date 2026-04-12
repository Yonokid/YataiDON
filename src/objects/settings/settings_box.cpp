#include "settings_box.h"
#include "spdlog/spdlog.h"


BaseOptionBox* SettingsBox::make_option_box(const rapidjson::Value& opt) {
    std::string type        = opt["type"].GetString();
    std::string path        = opt["path"].GetString();
    std::string lang        = global_data.config->general.language;

    auto localise = [&](const rapidjson::Value& obj) -> std::string {
        if (obj.HasMember(lang.c_str())) return obj[lang.c_str()].GetString();
        if (obj.HasMember("en"))         return obj["en"].GetString();
        return "";
    };

    std::string name = localise(opt["name"]);
    std::string desc = localise(opt["description"]);

    std::map<std::string,std::string> values_map;
    if (opt.HasMember("values") && opt["values"].IsObject()) {
        for (auto it = opt["values"].MemberBegin(); it != opt["values"].MemberEnd(); ++it) {
            values_map[it->name.GetString()] = localise(it->value);
        }
    }

    if (type == "bool") {
        std::string true_label  = values_map.count("true")  ? values_map["true"]  : "Enabled";
        std::string false_label = values_map.count("false") ? values_map["false"] : "Disabled";
        return new BoolOptionBox(name, desc, path, true_label, false_label);
    }
    if (type == "int") {
        return new IntOptionBox(name, desc, path, values_map);
    }
    if (type == "string") {
        return new StrOptionBox(name, desc, path, values_map);
    }
    if (type == "keybind") {
        return new KeybindOptionBox(name, desc, path);
    }
    if (type == "keybind_controller") {
        return new KeyBindControllerOptionBox(name, desc, path);
    }
    if (type == "float") {
        return new FloatOptionBox(name, desc, path);
    }
    return new StrOptionBox(name, desc, path, values_map);
}

static constexpr float WRAP_BOTTOM = 650.0f;
static constexpr float WRAP_TOP    = -50.0f;
static constexpr float BOX_STEP    = 100.0f;

SettingsBox::SettingsBox(const std::string& name,
                         const std::string& label_text,
                         const rapidjson::Value& options_json)
    : box_name(name)
    , label(new OutlinedText(label_text, 35, ray::WHITE, ray::Color{109,68,24,255}, false, 5))
    , x(10.0f)
    , y(WRAP_TOP)
    , start_position(WRAP_TOP)
    , target_position(std::numeric_limits<float>::infinity())
    , direction(1)
    , move_anim(nullptr)
    , blue_arrow_fade(nullptr)
    , blue_arrow_move(nullptr)
    , is_selected(false)
    , in_box(false)
    , option_index(0)
{
    move_anim        = tex.get_animation(0, true);
    blue_arrow_fade  = (FadeAnimation*) tex.get_animation(1, true);
    blue_arrow_move  = (MoveAnimation*) tex.get_animation(2, true);

    if (options_json.IsObject()) {
        for (auto it = options_json.MemberBegin(); it != options_json.MemberEnd(); ++it) {
            try {
                options.push_back(make_option_box(it->value));
            } catch (const std::exception& e) {
                spdlog::warn("Skipping option '{}': {}", it->name.GetString(), e.what());
            }
        }
    }
}

SettingsBox::~SettingsBox() {
    delete label;
    for (auto* opt : options) delete opt;
}

void SettingsBox::set_y(float new_y) {
    y              = new_y;
    start_position = new_y;
    target_position = std::numeric_limits<float>::infinity();
}

bool SettingsBox::move_left() {
    if (y != target_position && !std::isinf(target_position)) return false;
    move_anim->start();
    direction      = 1;
    start_position = y;
    target_position = y + BOX_STEP * direction;
    if (target_position >= WRAP_BOTTOM) {
        target_position = WRAP_TOP + (target_position - WRAP_BOTTOM);
    }
    return true;
}

void SettingsBox::move_right() {
    if (y != target_position && !std::isinf(target_position)) return;
    move_anim->start();
    direction      = -1;
    start_position = y;
    target_position = y + BOX_STEP * direction;
    if (target_position < WRAP_TOP) {
        target_position = WRAP_BOTTOM + (target_position - WRAP_TOP);
    }
}

bool SettingsBox::move_option_left() {
    if (!options.empty() && options[option_index]->is_highlighted) {
        options[option_index]->move_left();
        return true;
    }
    if (option_index == 0) {
        in_box = false;
        return false;
    }
    option_index--;
    return true;
}

void SettingsBox::move_option_right() {
    if (!options.empty() && options[option_index]->is_highlighted) {
        options[option_index]->move_right();
    } else {
        option_index = std::min(option_index + 1, (int)options.size() - 1);
    }
}

void SettingsBox::select_option() {
    if (options.empty()) return;
    options[option_index]->is_highlighted = !options[option_index]->is_highlighted;
    options[option_index]->confirm();
}

void SettingsBox::select() {
    in_box = true;
}

void SettingsBox::update(double current_time_ms, bool selected) {
    is_selected = selected;
    blue_arrow_fade->update(current_time_ms);
    blue_arrow_move->update(current_time_ms);
    move_anim->update(current_time_ms);

    if (move_anim->is_finished) {
        y = target_position;
    } else {
        y = start_position + move_anim->attribute * direction;
    }
    for (auto* opt : options) {
        opt->update(current_time_ms);
    }
}

void SettingsBox::draw_text() const {
    auto& box_tex = tex.textures.at("box").at("box");
    float text_x = x + (box_tex->width  / 2.0f) - (label->width  / 2.0f);
    float text_y = y + (box_tex->height / 2.0f) - (label->height / 2.0f);

    if (is_selected) {
        label->draw({.x=text_x, .y=text_y});
    } else if (box_name == "exit") {
        label->draw({.color=ray::RED, .x=text_x, .y=text_y});
    } else {
        label->draw({.x=text_x, .y=text_y});
    }
}

void SettingsBox::draw() {
    tex.draw_texture("box", "box", {.x=x, .y=y});
    if (is_selected) {
        tex.draw_texture("box", "box_highlight", {.x=x, .y=y});
    }
    if (in_box && !options.empty()) {
        options[option_index]->draw();
        if (!options[option_index]->is_highlighted) {
            tex.draw_texture("background", "blue_arrow",
                {.x=-(float)blue_arrow_move->attribute,
                 .fade=blue_arrow_fade->attribute,
                 .index=0});
            if (option_index != (int)options.size() - 1) {
                tex.draw_texture("background", "blue_arrow",
                    {.mirror="horizontal",
                     .x=(float)blue_arrow_move->attribute,
                     .fade=blue_arrow_fade->attribute,
                     .index=1});
            }
        }
    }
    draw_text();
}
