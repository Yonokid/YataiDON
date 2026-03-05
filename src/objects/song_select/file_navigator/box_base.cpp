#include "box_base.h"

BaseBox::BaseBox(const fs::path& path,
                 const std::optional<ray::Color> back_color,
                 const std::optional<ray::Color> fore_color,
                 TextureIndex texture_index)
    : path(path), texture_index(texture_index),
      back_color(back_color)
{
    genre_index = GenreIndex::DEFAULT;

    if (fore_color.has_value()) {
        this->fore_color = fore_color.value();
    } else if (back_color.has_value()) {
        this->fore_color = darken_color(back_color.value());
    } else {
        this->fore_color = ray::Color(101, 0, 82, 255);
    }

    position = std::numeric_limits<float>::infinity();
    target_position = std::numeric_limits<float>::infinity();

    open_anim = new MoveAnimation(233, 150.0f * tex.screen_scale, false, false, 0, 133);
    open_fade = new FadeAnimation(200, 0.0f, 1.0f);
    move = std::make_unique<MoveAnimation>(133, 0, false, false, 0, 0.0, std::nullopt, std::nullopt, "cubic");
    move->start();
}

BaseBox::~BaseBox() {
    if (shader_loaded)
        ray::UnloadShader(shader);
}

void BaseBox::load_text() {
    float font_size = tex.skin_config["song_box_name"].font_size;
    if (utf8_char_count(text_name) >= 30)
        font_size -= (int)(10 * tex.screen_scale);
    name = make_unique<OutlinedText>(text_name, font_size, ray::WHITE, fore_color.value(), true);

    if (back_color.has_value()) {
        shader = ray::LoadShader("shader/dummy.vs", "shader/colortransform.fs");
        shader_loaded = true;
        const auto& target_rgb = back_color.value();
        float src[3] = { 142 / 255.0f, 212 / 255.0f, 30 / 255.0f };
        float tgt[3] = { target_rgb.r / 255.0f, target_rgb.g / 255.0f, target_rgb.b / 255.0f };
        int source_loc = ray::GetShaderLocation(shader, "sourceColor");
        int target_loc = ray::GetShaderLocation(shader, "targetColor");
        ray::SetShaderValue(shader, source_loc, src, ray::SHADER_UNIFORM_VEC3);
        ray::SetShaderValue(shader, target_loc, tgt, ray::SHADER_UNIFORM_VEC3);
    }

    text_loaded = true;
}

void BaseBox::reset_yellow_box() {
    if (yellow_box.has_value()) {
        yellow_box->reset();
        yellow_box_opened = false;
    }
}

void BaseBox::expand_box() {
    yellow_box.emplace();
    yellow_box_opened = false;
}

void BaseBox::close_box() {
    yellow_box.reset();
    yellow_box_opened = false;
}

void BaseBox::enter_diff_select() {
    yellow_box->create_anim_2();
}

void BaseBox::exit_diff_select() {
    yellow_box.reset();
    yellow_box.emplace();
    yellow_box->create_anim();
}

void BaseBox::set_position(float target_position) {
    position = target_position;
    this->target_position = position;
}

void BaseBox::move_box(float target_position, float duration) {
    this->target_position = target_position;
    float delta = target_position - position;
    move = std::make_unique<MoveAnimation>(duration, delta / tex.screen_scale, false, false, 0, 0.0, std::nullopt, std::nullopt, "cubic");
    move->start();
}

void BaseBox::update(double current_time) {
    open_anim->update(current_time);
    open_fade->update(current_time);
    float prev_position = move->attribute;
    move->update(current_time);
    if (!move->is_finished) {
        position += move->attribute - prev_position;
    } else {
        position = target_position;
        if (yellow_box.has_value() && !yellow_box_opened) {
            yellow_box->create_anim();
            yellow_box_opened = true;
        }
    }
    if (yellow_box.has_value()) yellow_box->update(current_time);
}

void BaseBox::draw_closed(float outer_fade_override) {
    if (shader_loaded && texture_index == TextureIndex::NONE)
        ray::BeginShaderMode(shader);

    tex.draw_texture("box", "folder_texture_left",  {.frame=(int)texture_index, .x=position, .fade=outer_fade_override});
    tex.draw_texture("box", "folder_texture",       {.frame=(int)texture_index, .x=position, .x2=tex.skin_config["song_box_bg"].width, .fade=outer_fade_override});
    tex.draw_texture("box", "folder_texture_right", {.frame=(int)texture_index, .x=position, .fade=outer_fade_override});

    if (shader_loaded && texture_index == TextureIndex::NONE)
        ray::EndShaderMode();

    if (texture_index == TextureIndex::DEFAULT)
        tex.draw_texture("box", "genre_overlay", {.x=position, .fade=outer_fade_override});
    if (genre_index == GenreIndex::DIFFICULTY)
        tex.draw_texture("box", "diff_overlay",  {.x=position, .fade=outer_fade_override});
}

void BaseBox::draw_open(std::optional<float> inner_fade_override) {
    if (yellow_box.has_value())
        yellow_box->draw();
    tex.draw_texture("box", "back_graphic", {.fade=inner_fade_override.value()});
}

void BaseBox::draw_diff_select(std::optional<float> fade_override) {
    if (yellow_box.has_value())
        yellow_box->draw();
}

void BaseBox::draw(std::optional<float> inner_fade_override, float outer_fade_override)
{
    draw_closed(outer_fade_override);
    if (yellow_box.has_value() && yellow_box->is_diff_select) {
        draw_diff_select(inner_fade_override);
    } else if (yellow_box.has_value() && yellow_box_opened)
        draw_open(inner_fade_override);
}
