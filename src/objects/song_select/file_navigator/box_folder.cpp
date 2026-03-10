#include "box_folder.h"
#include <memory>

FolderBox::FolderBox(const fs::path& path, const BoxDef& box_def, int tja_count)
    : BaseBox(path, box_def), tja_count(tja_count)
{
    this->text_name = box_def.name;
    enter_fade = new FadeAnimation(166);
}

void FolderBox::load_text() {
    BaseBox::load_text();
    hori_name = std::make_unique<OutlinedText>(text_name, tex.skin_config["song_hori_name"].font_size, ray::WHITE, ray::BLACK, false);
    tja_count_text = std::make_unique<OutlinedText>(std::to_string(tja_count), tex.skin_config["song_tja_count"].font_size, ray::WHITE, ray::BLACK, false);
    text_loaded = true;
}

void FolderBox::update(double current_time) {
    bool is_open_prev = yellow_box_opened;
    enter_fade->update(current_time);
    BaseBox::update(current_time);

    if (!is_open_prev && yellow_box_opened) {
        if (!audio->is_sound_playing("voice_enter")) {
            audio->play_sound("genre_voice_" + std::to_string((int)genre_index), "voice");
        }
    } else if (!yellow_box_opened && audio->is_sound_playing("genre_voice_" + std::to_string((int)genre_index))) {
        audio->stop_sound("genre_voice_" + std::to_string((int)genre_index));
    }
}

void FolderBox::enter_box() {
    entered = true;
    enter_fade->start();
}

void FolderBox::exit_box() {
    entered = false;
    enter_fade->reset();
}

void FolderBox::draw_closed() {
    BaseBox::draw_closed();

    if (shader_loaded && texture_index == TextureIndex::NONE)
        ray::BeginShaderMode(shader);
    tex.draw_texture("box", "folder_clip", {.frame=(int)texture_index, .x=position-(1.0f * tex.screen_scale), .fade=fade->attribute});
    if (shader_loaded && texture_index == TextureIndex::NONE)
        ray::EndShaderMode();

    if (!text_loaded) return;
    float name_h = std::min((float)this->name->height, tex.skin_config["song_box_name"].height);
    this->name->draw({
        .x    = position + tex.skin_config["song_box_name"].x - (int)(this->name->width / 2.0f),
        .y    = tex.skin_config["song_box_name"].y,
        .y2   = name_h - this->name->height,
        .fade = fade->attribute
    });

    if (!crown.empty()) {
        int highest_crown = std::max_element(crown.begin(), crown.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; })->first;
        int frame = std::min((int)Difficulty::URA, highest_crown);
        Crown c = crown.at(highest_crown);
        if      (c == Crown::DFC)   tex.draw_texture("yellow_box", "crown_dfc",   {.frame=frame});
        else if (c == Crown::FC)    tex.draw_texture("yellow_box", "crown_fc",    {.frame=frame});
        else                         tex.draw_texture("yellow_box", "crown_clear", {.frame=frame});
    }
}

void FolderBox::draw_open_bg(float fade) {
    int frame = (int)texture_index;
    bool use_shader = shader_loaded && texture_index == TextureIndex::NONE;

    if (open_anim->attribute >= (100.0f * tex.screen_scale)) {
        if (use_shader) ray::BeginShaderMode(shader);
        tex.draw_texture("box", "folder_top_edge", {.frame=frame, .mirror="horizontal", .y=-(float)open_anim->attribute, .fade=fade});
        tex.draw_texture("box", "folder_top",      {.frame=frame, .y=-(float)open_anim->attribute, .fade=fade});
        tex.draw_texture("box", "folder_top_edge", {.frame=frame, .x=tex.skin_config["song_folder_top"].x, .y=-(float)open_anim->attribute, .fade=fade});
        if (use_shader) ray::EndShaderMode();
    }

    if (use_shader) ray::BeginShaderMode(shader);
    tex.draw_texture("box", "folder_texture_left",  {.frame=frame, .x=position-(float)open_anim->attribute, .fade=fade});
    tex.draw_texture("box", "folder_texture", {
        .frame=frame,
        .x=position-(float)open_anim->attribute,
        .x2=((float)open_anim->attribute * 2.0f) + tex.skin_config["song_box_bg"].width,
        .fade=fade
    });
    tex.draw_texture("box", "folder_texture_right", {.frame=frame, .x=position + (float)open_anim->attribute, .fade=fade});
    if (use_shader) ray::EndShaderMode();
}

void FolderBox::draw_open_fg(float fade) {
    if (open_anim->attribute >= (100.0f * tex.screen_scale)) {
        float dest_width = std::min(tex.skin_config["song_hori_name"].width,
                                    (float)hori_name->width);
        hori_name->draw({
            .x  = (tex.skin_config["song_hori_name"].x) - (dest_width / 2.0f),
            .y  = tex.skin_config["song_hori_name"].y - (float)open_anim->attribute,
            .x2 = dest_width - hori_name->width, .fade=fade
        });
    }

    if (texture_index == TextureIndex::DEFAULT)
        tex.draw_texture("box", "genre_overlay_large", {.fade=fade});
    if (genre_index == GenreIndex::DIFFICULTY)
        tex.draw_texture("box", "diff_overlay_large",  {.fade=fade});

    // Song count
    if (genre_index != GenreIndex::DIFFICULTY) {
        tex.draw_texture("yellow_box", "song_count_back",  {.fade=std::min(fade, 0.5f)});
        tex.draw_texture("yellow_box", "song_count_num",   {.fade=fade});
        tex.draw_texture("yellow_box", "song_count_songs", {.fade=fade});

        float dest_width = std::min(tex.skin_config["song_tja_count"].width,
                                    (float)tja_count_text->width);
        tja_count_text->draw({
            .x  = tex.skin_config["song_tja_count"].x - (dest_width / 2.0f),
            .y  = tex.skin_config["song_tja_count"].y,
            .x2 = dest_width - tja_count_text->width,
            .fade=fade
        });
    }

    if (texture_index != TextureIndex::DEFAULT) {
        tex.draw_texture("box", "folder_graphic", {.frame=(int)genre_index, .fade=fade});
        tex.draw_texture("box", "folder_text",    {.frame=(int)genre_index, .fade=fade});
    }
}

void FolderBox::draw_open() {
    if (entered) {
        draw_open_bg(0.0);
        draw_open_fg(enter_fade->attribute);
    } else {
        draw_open_bg(1.0);
        draw_open_fg(open_fade->attribute);
    }
}
