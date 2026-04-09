#include "box_folder.h"
#include "raylib.h"
#include <memory>

FolderBox::FolderBox(const fs::path& path, const BoxDef& box_def, int tja_count, std::map<std::pair<std::string, std::string>, fs::path>& song_files)
    : BaseBox(path, box_def), tja_count(tja_count)
{
    this->text_name = box_def.name;
    enter_fade = new FadeAnimation(166);
    std::set<int> disqualified;

    auto update_crown = [&](const fs::path& file_path) {
        auto& hashes = scores_manager.get_hashes(file_path);
        for (int diff = 0; diff < 5; diff++) {
            if (hashes[diff].empty()) continue;
            auto score = scores_manager.get_score(hashes[diff], diff, 1);

            if (!score || score->crown == Crown::NONE) {
                crown.erase(diff);
                disqualified.insert(diff);
                continue;
            }

            if (disqualified.count(diff)) continue;

            if (crown.find(diff) == crown.end())
                crown[diff] = score->crown;
            else
                crown[diff] = std::min(crown[diff], score->crown);
        }
    };

    for (const auto& entry : fs::recursive_directory_iterator(path)) {
        if (entry.path().filename() == "song_list.txt") {
            std::ifstream file(entry.path());
            std::string line;
            while (std::getline(file, line)) {
                std::vector<std::string> fields;
                std::stringstream ss(line);
                std::string field;
                while (std::getline(ss, field, '|')) fields.push_back(field);
                if (fields.size() < 3) continue;
                for (const auto& [key, value] : song_files)
                    if (key.first == fields[1] && key.second == fields[2])
                        update_crown(value);
            }
        }
        if (entry.path().extension() == ".tja")
            update_crown(entry.path());
    }
}

FolderBox::~FolderBox() {
    delete enter_fade;
}

void FolderBox::load_text() {
    BaseBox::load_text();
    hori_name = std::make_unique<OutlinedText>(text_name, tex.skin_config["song_hori_name"].font_size, ray::WHITE, ray::BLACK, false);
    tja_count_text = std::make_unique<OutlinedText>(std::to_string(tja_count), tex.skin_config["song_tja_count"].font_size, ray::WHITE, ray::BLACK, false);
    if (fs::exists(fs::path(path / "box.png")) && !box_texture.has_value()) {
        box_texture = ray::LoadTexture((path / "box.png").string().c_str());
        ray::GenTextureMipmaps(&box_texture.value());
        ray::SetTextureFilter(box_texture.value(), ray::TEXTURE_FILTER_TRILINEAR);
    }
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
        if      (c == Crown::DFC)   tex.draw_texture("yellow_box", "crown_dfc",   {.frame=frame, .x=position});
        else if (c == Crown::FC)    tex.draw_texture("yellow_box", "crown_fc",    {.frame=frame, .x=position});
        else                         tex.draw_texture("yellow_box", "crown_clear", {.frame=frame, .x=position});
    }
}

void FolderBox::draw_open_bg(float fade) {
    float shadow_fade = std::min(fade, (float)open_fade->attribute);
    tex.draw_texture("yellow_box", "shadow_bottom_left", {.x=position, .fade=shadow_fade, .index=1});
    tex.draw_texture("yellow_box", "shadow_bottom", {.x=position, .fade=shadow_fade, .index=1});
    tex.draw_texture("yellow_box", "shadow_bottom_right", {.x=position, .fade=shadow_fade, .index=1});
    tex.draw_texture("yellow_box", "shadow_right", {.x=position, .fade=shadow_fade, .index=1});
    tex.draw_texture("yellow_box", "shadow_top_right", {.x=position, .fade=shadow_fade, .index=1});
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

    if (box_texture.has_value()) {
        float scaled_width = box_texture->width * tex.screen_scale;
        float scaled_height = box_texture->height * tex.screen_scale;
        float max_width = 344 * tex.screen_scale;
        float max_height = 424 * tex.screen_scale;
        if (scaled_width > max_width || scaled_height > max_height) {
            float width_scale = max_width / scaled_width;
            float height_scale = max_height / scaled_height;
            float scale_factor = std::min(width_scale, height_scale);
            scaled_width *= scale_factor;
            scaled_height *= scale_factor;
        }
        int x = int(position + tex.skin_config["box_texture"].x - (scaled_width / 2));
        int y = int(tex.skin_config["box_texture"].y - (scaled_height / 2));
        ray::Rectangle src(0, 0, box_texture->width, box_texture->height);
        ray::Rectangle dest(x, y, scaled_width, scaled_height);
        ray::DrawTexturePro(box_texture.value(), src, dest, ray::Vector2(0, 0), 0, ray::Fade(ray::WHITE, fade));
    } else if (texture_index != TextureIndex::DEFAULT) {
        tex.draw_texture("box", "folder_graphic", {.frame=(int)genre_index, .fade=fade});
        tex.draw_texture("box", "folder_text",    {.frame=(int)genre_index, .fade=fade});
    }
}

void FolderBox::draw_open() {
    if (entered) {
        draw_open_bg(0.0);
        draw_open_fg(enter_fade->attribute);
    } else {
        draw_open_bg(std::min(1.0, fade->attribute));
        draw_open_fg(std::min(open_fade->attribute, fade->attribute));
    }
}
