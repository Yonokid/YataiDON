#include "box_base.h"

BaseBox::BaseBox(const fs::path& path, const BoxDef& box_def)
    : path(path), texture_index(box_def.texture_index),
      back_color(box_def.back_color), genre_index(box_def.genre_index)
{
    if (box_def.fore_color.has_value()) {
        this->fore_color = box_def.fore_color.value();
    } else if (back_color.has_value()) {
        this->fore_color = darken_color(box_def.back_color.value());
    } else {
        this->fore_color = ray::Color(101, 0, 82, 255);
    }

    position = std::numeric_limits<float>::infinity();
    target_position = std::numeric_limits<float>::infinity();

    open_anim = new MoveAnimation(233, 150.0f * tex.screen_scale, false, false, 0, 133);
    open_fade = new FadeAnimation(200, 0.0f, false, false, 1.0f, 133);
    move = std::make_unique<MoveAnimation>(133, 0, false, false, 0, 0.0, std::nullopt, std::nullopt, "cubic");
    move->start();

    fade_in(100);
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

void BaseBox::reset() {
    yellow_box.reset();
    yellow_box_opened = false;
    open_anim->reset();
    open_fade->reset();
}

void BaseBox::expand_box() {
    yellow_box.emplace();
    yellow_box_opened = false;
    open_anim->start();
}

void BaseBox::close_box() {
    yellow_box.reset();
    yellow_box_opened = false;
}

void BaseBox::enter_box() {
    yellow_box->create_anim_2();
}

void BaseBox::exit_box() {
    yellow_box.reset();
    yellow_box.emplace();
    yellow_box->create_anim();
    open_fade->start();
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

void BaseBox::fade_in(float delay) {
    fade = new FadeAnimation(266, 0.0f, false, false, 1.0f, delay);
    fade->start();
}

void BaseBox::fade_out() {
    fade = new FadeAnimation(166);
    fade->start();
}

void BaseBox::update(double current_time) {
    open_anim->update(current_time);
    open_fade->update(current_time);
    fade->update(current_time);
    float prev_position = move->attribute;
    move->update(current_time);
    if (!move->is_finished) {
        position += move->attribute - prev_position;
    } else {
        position = target_position;
        if (yellow_box.has_value() && !yellow_box_opened) {
            yellow_box->create_anim();
            yellow_box_opened = true;
            open_fade->start();
        }
    }
    if (yellow_box.has_value()) {
        yellow_box->update(current_time);
        left_bound = position + yellow_box->left_distance;
        right_bound = yellow_box->right_distance;
    } else {
        left_bound = position;
        right_bound = position + (float)(tex.textures["box"]["folder_texture_left"]->width) + (float)(tex.textures["box"]["folder_texture_right"]->width) + (tex.skin_config["song_box_bg"].width);
    }
}

void BaseBox::draw_closed() {
    tex.draw_texture("yellow_box", "shadow_bottom_left", {.x=position, .fade=fade->attribute, .index=0});
    tex.draw_texture("yellow_box", "shadow_bottom", {.x=position, .fade=fade->attribute, .index=0});
    tex.draw_texture("yellow_box", "shadow_bottom_right", {.x=position, .fade=fade->attribute, .index=0});
    tex.draw_texture("yellow_box", "shadow_right", {.x=position, .fade=fade->attribute, .index=0});
    tex.draw_texture("yellow_box", "shadow_top_right", {.x=position, .fade=fade->attribute, .index=0});

    if (shader_loaded && texture_index == TextureIndex::NONE)
        ray::BeginShaderMode(shader);

    tex.draw_texture("box", "folder_texture_left",  {.frame=(int)texture_index, .x=position, .fade=fade->attribute});
    tex.draw_texture("box", "folder_texture",       {.frame=(int)texture_index, .x=position, .x2=tex.skin_config["song_box_bg"].width, .fade=fade->attribute});
    tex.draw_texture("box", "folder_texture_right", {.frame=(int)texture_index, .x=position, .fade=fade->attribute});

    if (shader_loaded && texture_index == TextureIndex::NONE)
        ray::EndShaderMode();

    if (texture_index == TextureIndex::DEFAULT)
        tex.draw_texture("box", "genre_overlay", {.x=position, .fade=fade->attribute});
    if (genre_index == GenreIndex::DIFFICULTY)
        tex.draw_texture("box", "diff_overlay",  {.x=position, .fade=fade->attribute});
}

void BaseBox::draw_open() {
    if (yellow_box.has_value()) {
        yellow_box->draw();
    }
}

void BaseBox::draw_diff_select(bool is_ura) {
    if (yellow_box.has_value())
        yellow_box->draw();
}

void BaseBox::draw(bool is_ura)
{
    if (yellow_box.has_value() && yellow_box->is_diff_select) {
        draw_diff_select(is_ura);
    } else if (yellow_box.has_value() && yellow_box_opened) {
        draw_open();
    } else {
        draw_closed();
    }
}
