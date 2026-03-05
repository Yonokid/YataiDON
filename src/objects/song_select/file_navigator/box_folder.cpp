#include "box_folder.h"
#include <memory>

FolderBox::FolderBox(const fs::path& path,
                     const std::optional<ray::Color>& back_color,
                     const std::optional<ray::Color>& fore_color,
                     TextureIndex texture_index,
                     GenreIndex genre_index,
                     const std::string& text_name,
                     int tja_count)
    : BaseBox(path, back_color, fore_color, texture_index),
      genre_index(genre_index), tja_count(tja_count)
{
    this->text_name = text_name;
}

void FolderBox::load_text() {
    BaseBox::load_text();
    hori_name = std::make_unique<OutlinedText>(text_name, tex.skin_config["song_hori_name"].font_size, ray::WHITE, ray::BLACK, false);
    tja_count_text = std::make_unique<OutlinedText>(std::to_string(tja_count), tex.skin_config["song_tja_count"].font_size, ray::WHITE, ray::BLACK, false);
    text_loaded = true;
}

void FolderBox::update(double current_time) {
    bool is_open_prev = yellow_box_opened;
    BaseBox::update(current_time);

    if (!is_open_prev && yellow_box_opened) {
        open_anim->start();
        open_fade->start();
        if (!audio->is_sound_playing("voice_enter")) {
            audio->play_sound("genre_voice_" + std::to_string((int)genre_index), "voice");
        }
    } else if (!yellow_box_opened && audio->is_sound_playing("genre_voice_" + std::to_string((int)genre_index))) {
        audio->stop_sound("genre_voice_" + std::to_string((int)genre_index));
    }
}

void FolderBox::draw_closed(float outer_fade_override) {
    BaseBox::draw_closed(outer_fade_override);

    if (shader_loaded && texture_index == TextureIndex::NONE)
        ray::BeginShaderMode(shader);
    tex.draw_texture("box", "folder_clip", {
        .frame=(int)texture_index,
        .x=position-(1.0f * tex.screen_scale),
        .fade=outer_fade_override
    });
    if (shader_loaded && texture_index == TextureIndex::NONE)
        ray::EndShaderMode();

    if (!text_loaded) return;
    float name_h = std::min((float)this->name->height, tex.skin_config["song_box_name"].height);
    this->name->draw({
        .x    = position + tex.skin_config["song_box_name"].x - (int)(this->name->width / 2.0f),
        .y    = tex.skin_config["song_box_name"].y,
        .y2   = name_h - this->name->height,
        .fade = outer_fade_override
    });

    if (!crown.empty()) {
        int highest_crown = std::max_element(crown.begin(), crown.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; })->first;
        int frame = std::min((int)Difficulty::URA, highest_crown);
        Crown c = crown.at(highest_crown);
        if      (c == Crown::DFC)   tex.draw_texture("yellow_box", "crown_dfc",   {.frame=frame, .fade=outer_fade_override});
        else if (c == Crown::FC)    tex.draw_texture("yellow_box", "crown_fc",    {.frame=frame, .fade=outer_fade_override});
        else                         tex.draw_texture("yellow_box", "crown_clear", {.frame=frame, .fade=outer_fade_override});
    }
}

void FolderBox::draw_open(std::optional<float> fade_override) {
    ray::Color color = fade_override.has_value()
        ? ray::Fade(ray::WHITE, *fade_override) : ray::WHITE;

    int frame = (int)texture_index;
    bool use_shader = shader_loaded && texture_index == TextureIndex::NONE;

    // Top flap (only once animation has progressed enough)
    if (open_anim->attribute >= (100.0f * tex.screen_scale)) {
        if (use_shader) ray::BeginShaderMode(shader);
        tex.draw_texture("box", "folder_top_edge", {.color=color, .frame=frame, .mirror="horizontal", .y=-(float)open_anim->attribute});
        tex.draw_texture("box", "folder_top",      {.color=color, .frame=frame, .y=-(float)open_anim->attribute});
        tex.draw_texture("box", "folder_top_edge", {.color=color, .frame=frame, .x=tex.skin_config["song_folder_top"].x, .y=-(float)open_anim->attribute});

        float dest_width = std::min(tex.skin_config["song_hori_name"].width,
                                    (float)hori_name->width);
        hori_name->draw({
            .color = color,
            .x  = (tex.skin_config["song_hori_name"].x) - (dest_width / 2.0f),
            .y  = tex.skin_config["song_hori_name"].y - (float)open_anim->attribute,
            .x2 = dest_width - hori_name->width,
        });
        if (use_shader) ray::EndShaderMode();
    }

    // Expanding box sides
    if (use_shader) ray::BeginShaderMode(shader);
    tex.draw_texture("box", "folder_texture_left",  {.frame=frame, .x=position-(float)open_anim->attribute});
    tex.draw_texture("box", "folder_texture", {
        .frame=frame,
        .x=position-(float)open_anim->attribute,
        .x2=((float)open_anim->attribute * 2.0f) + tex.skin_config["song_box_bg"].width
    });
    tex.draw_texture("box", "folder_texture_right", {.frame=frame, .x=position + (float)open_anim->attribute});
    if (use_shader) ray::EndShaderMode();

    if (texture_index == TextureIndex::DEFAULT)
        tex.draw_texture("box", "genre_overlay_large", {.color=color});
    if (genre_index == GenreIndex::DIFFICULTY)
        tex.draw_texture("box", "diff_overlay_large",  {.color=color});

    // Song count
    color = fade_override.has_value() ? ray::Fade(ray::WHITE, *fade_override) : ray::WHITE;
    if (genre_index != GenreIndex::DIFFICULTY) {
        tex.draw_texture("yellow_box", "song_count_back",  {.color=color, .fade=0.5f});
        tex.draw_texture("yellow_box", "song_count_num",   {.color=color});
        tex.draw_texture("yellow_box", "song_count_songs", {.color=color});

        float dest_width = std::min(tex.skin_config["song_tja_count"].width,
                                    (float)tja_count_text->width);
        tja_count_text->draw({
            .color = color,
            .x  = tex.skin_config["song_tja_count"].x - (dest_width / 2.0f),
            .y  = tex.skin_config["song_tja_count"].y,
            .x2 = dest_width - tja_count_text->width,
        });
    }

    if (texture_index != TextureIndex::DEFAULT) {
        tex.draw_texture("box", "folder_graphic", {.color=color, .frame=(int)genre_index});
        tex.draw_texture("box", "folder_text",    {.color=color, .frame=(int)genre_index});
    }
}
