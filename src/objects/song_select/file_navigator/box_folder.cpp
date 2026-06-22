#include "box_folder.h"
#include "../../../libs/filesystem.h"
#include "../../../libs/scores.h"
#include "../../../libs/audio.h"

FolderBox::FolderBox(const fs::path& path, const BoxDef& box_def, std::map<std::pair<std::string, std::string>, fs::path>& song_files)
    : BaseBox(path, box_def), tja_count(0)
{
    this->text_name = box_def.name;
    enter_fade = std::make_unique<FadeAnimation>(166);
    refresh_scores(song_files);
}

void FolderBox::refresh_scores(std::map<std::pair<std::string, std::string>, fs::path>& song_files) {
    crown.clear();
    tja_count = 0;
    std::set<int> disqualified;

    auto update_crown = [&](const fs::path& file_path) {
        auto& hashes = scores_manager.get_hashes(file_path);
        for (int diff = 0; diff < 5; diff++) {
            if (hashes[diff].empty()) continue;
            auto score = scores_manager.get_score(hashes[diff], diff, global_data.config->general.player_1_id);

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
            auto entries = read_song_list(entry.path());
            tja_count += (int)entries.size();
            for (const auto& e : entries)
                if (auto found = scores_manager.get_path_by_hash(e.hash))
                    update_crown(*found);
            continue;
        }
        auto ext = entry.path().extension();
        if (ext == ".tja" || ext == ".osu") {
            tja_count++;
            update_crown(entry.path());
        }
    }
}

FolderBox::~FolderBox() = default;

void FolderBox::load_text() {
    BaseBox::load_text();
    hori_name = std::make_unique<OutlinedText>(text_name, tex.skin_config[SC::SONG_HORI_NAME].font_size, ray::WHITE, ray::BLACK, false);
    tja_count_text = std::make_unique<OutlinedText>(std::to_string(tja_count), tex.skin_config[SC::SONG_TJA_COUNT].font_size, ray::WHITE, ray::BLACK, false);
    if (is_osu_folder) {
        auto it = fs::directory_iterator(path);
        while (it->path().extension() != ".jpg" && it->path().extension() != ".png") {
            it++;
        }
        box_texture = ray::LoadTexture((it->path()).string().c_str());
        ray::GenTextureMipmaps(&box_texture.value());
        ray::SetTextureFilter(box_texture.value(), ray::TEXTURE_FILTER_TRILINEAR);
    } else if (fs::exists(fs::path(path / "box.png")) && !box_texture.has_value()) {
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
        if (!audio.is_sound_playing("voice_enter")) {
            audio.play_sound("genre_voice_" + std::to_string((int)genre_index), VolumePreset::VOICE);
        }
    } else if (!yellow_box_opened && audio.is_sound_playing("genre_voice_" + std::to_string((int)genre_index))) {
        audio.stop_sound("genre_voice_" + std::to_string((int)genre_index));
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

    float bx = box_x();
    float by = box_y();

    if (shader_loaded && texture_index == TextureIndex::NONE)
        ray::BeginShaderMode(shader);
    tex.draw_texture(BOX::FOLDER_CLIP, {.frame=(int)texture_index, .x=bx-(1.0f * tex.screen_scale), .y=by, .fade=fade->attribute});
    if (shader_loaded && texture_index == TextureIndex::NONE)
        ray::EndShaderMode();

    if (!text_loaded) return;
    float name_h = std::min((float)this->name->height, tex.skin_config[SC::SONG_BOX_NAME].height);
    this->name->draw({
        .x    = bx + tex.skin_config[SC::SONG_BOX_NAME].x - (int)(this->name->width / 2.0f),
        .y    = tex.skin_config[SC::SONG_BOX_NAME].y + by,
        .y2   = name_h - this->name->height,
        .fade = fade->attribute
    });

    if (!crown.empty()) {
        int highest_crown = std::max_element(crown.begin(), crown.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; })->first;
        int frame = std::min((int)Difficulty::URA, highest_crown);
        Crown c = crown.at(highest_crown);
        if      (c == Crown::DFC)   tex.draw_texture(YELLOW_BOX::CROWN_DFC,   {.frame=frame, .x=bx, .y=by});
        else if (c == Crown::FC)    tex.draw_texture(YELLOW_BOX::CROWN_FC,    {.frame=frame, .x=bx, .y=by});
        else                         tex.draw_texture(YELLOW_BOX::CROWN_CLEAR, {.frame=frame, .x=bx, .y=by});
    }
}

void FolderBox::draw_open_bg(float fade) {
    float bx = box_x();
    float by = box_y();
    float shadow_fade = std::min(fade, (float)open_fade->attribute);
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM_LEFT,  {.x=bx, .y=by, .fade=shadow_fade, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM,       {.x=bx, .y=by, .fade=shadow_fade, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_BOTTOM_RIGHT, {.x=bx, .y=by, .fade=shadow_fade, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_RIGHT,        {.x=bx, .y=by, .fade=shadow_fade, .index=1});
    tex.draw_texture(YELLOW_BOX::SHADOW_TOP_RIGHT,    {.x=bx, .y=by, .fade=shadow_fade, .index=1});
    int frame = (int)texture_index;
    bool use_shader = shader_loaded && texture_index == TextureIndex::NONE;

    if (open_anim->attribute >= (100.0f * tex.screen_scale)) {
        if (use_shader) ray::BeginShaderMode(shader);
        tex.draw_texture(BOX::FOLDER_TOP_EDGE, {.frame=frame, .mirror="horizontal", .y=by-(float)open_anim->attribute, .fade=fade});
        tex.draw_texture(BOX::FOLDER_TOP,      {.frame=frame, .y=by-(float)open_anim->attribute, .fade=fade});
        tex.draw_texture(BOX::FOLDER_TOP_EDGE, {.frame=frame, .x=tex.skin_config[SC::SONG_FOLDER_TOP].x, .y=by-(float)open_anim->attribute, .fade=fade});
        if (use_shader) ray::EndShaderMode();
    }

    if (use_shader) ray::BeginShaderMode(shader);
    tex.draw_texture(BOX::FOLDER_TEXTURE_LEFT,  {.frame=frame, .x=bx-(float)open_anim->attribute, .y=by, .fade=fade});
    tex.draw_texture(BOX::FOLDER_TEXTURE, {
        .frame=frame,
        .x=bx-(float)open_anim->attribute,
        .y=by,
        .x2=((float)open_anim->attribute * 2.0f) + tex.skin_config[SC::SONG_BOX_BG].width,
        .fade=fade
    });
    tex.draw_texture(BOX::FOLDER_TEXTURE_RIGHT, {.frame=frame, .x=bx + (float)open_anim->attribute, .y=by, .fade=fade});
    if (use_shader) ray::EndShaderMode();
}

void FolderBox::draw_open_fg(float fade) {
    if (open_anim->attribute >= (100.0f * tex.screen_scale)) {
        float dest_width = std::min(tex.skin_config[SC::SONG_HORI_NAME].width,
                                    (float)hori_name->width);
        hori_name->draw({
            .x  = (tex.skin_config[SC::SONG_HORI_NAME].x) - (dest_width / 2.0f),
            .y  = tex.skin_config[SC::SONG_HORI_NAME].y - (float)open_anim->attribute,
            .x2 = dest_width - hori_name->width, .fade=fade
        });
    }

    if (texture_index == TextureIndex::DEFAULT)
        tex.draw_texture(BOX::GENRE_OVERLAY_LARGE, {.fade=fade});
    if (genre_index == GenreIndex::DIFFICULTY)
        tex.draw_texture(BOX::DIFF_OVERLAY_LARGE,  {.fade=fade});

    // Song count
    if (genre_index != GenreIndex::DIFFICULTY) {
        tex.draw_texture(YELLOW_BOX::SONG_COUNT_BACK,  {.fade=std::min(fade, 0.5f)});
        tex.draw_texture(YELLOW_BOX::SONG_COUNT_NUM,   {.fade=fade});
        tex.draw_texture(YELLOW_BOX::SONG_COUNT_SONGS, {.fade=fade});

        float dest_width = std::min(tex.skin_config[SC::SONG_TJA_COUNT].width,
                                    (float)tja_count_text->width);
        tja_count_text->draw({
            .x  = tex.skin_config[SC::SONG_TJA_COUNT].x - (dest_width / 2.0f),
            .y  = tex.skin_config[SC::SONG_TJA_COUNT].y,
            .x2 = dest_width - tja_count_text->width,
            .fade=fade
        });
    }

    if (box_texture.has_value()) {
        float scaled_width = box_texture->width * tex.screen_scale;
        float scaled_height = box_texture->height * tex.screen_scale;
        float max_width = tex.skin_config[SC::BOX_FOLDER_MAX_SIZE].width;
        float max_height = tex.skin_config[SC::BOX_FOLDER_MAX_SIZE].height;
        if (scaled_width > max_width || scaled_height > max_height) {
            float width_scale = max_width / scaled_width;
            float height_scale = max_height / scaled_height;
            float scale_factor = std::min(width_scale, height_scale);
            scaled_width *= scale_factor;
            scaled_height *= scale_factor;
        }
        int x = int(box_x() + tex.skin_config[SC::BOX_TEXTURE].x - (scaled_width / 2));
        int y = int(tex.skin_config[SC::BOX_TEXTURE].y + box_y() - (scaled_height / 2));
        ray::Rectangle src(0, 0, box_texture->width, box_texture->height);
        ray::Rectangle dest(x, y, scaled_width, scaled_height);
        ray::DrawTexturePro(box_texture.value(), src, dest, ray::Vector2(0, 0), 0, ray::Fade(ray::WHITE, fade));
    } else if (texture_index != TextureIndex::DEFAULT) {
        tex.draw_texture(BOX::FOLDER_GRAPHIC, {.frame=(int)genre_index, .fade=fade});
        tex.draw_texture(BOX::FOLDER_TEXT,    {.frame=(int)genre_index, .fade=fade});
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
