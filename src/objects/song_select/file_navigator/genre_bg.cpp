#include "genre_bg.h"

GenreBG::GenreBG(std::string& text_name, std::optional<ray::Color> color, TextureIndex texture_index, float distance)
: texture_index(texture_index) {
    float font_size = tex.skin_config["song_box_name"].font_size;
    if (utf8_char_count(text_name) >= 30)
        font_size -= (int)(10 * tex.screen_scale);
    name = make_unique<OutlinedText>(text_name, font_size, ray::WHITE, ray::BLACK, false);

    if (color.has_value()) {
        shader = ray::LoadShader("shader/dummy.vs", "shader/colortransform.fs");
        float src[3] = { 142 / 255.0f, 212 / 255.0f, 30 / 255.0f };
        float tgt[3] = { color.value().r / 255.0f, color.value().g / 255.0f, color.value().b / 255.0f };
        int source_loc = ray::GetShaderLocation(shader, "sourceColor");
        int target_loc = ray::GetShaderLocation(shader, "targetColor");
        ray::SetShaderValue(shader, source_loc, src, ray::SHADER_UNIFORM_VEC3);
        ray::SetShaderValue(shader, target_loc, tgt, ray::SHADER_UNIFORM_VEC3);
        shader_loaded = true;
    }

    stretch = new MoveAnimation(333, 20 * tex.screen_scale, false, false, 0, 0, 0, std::nullopt, "cubic");
    scale = new TextureResizeAnimation(100, 0.9f, false, false, 1.0);
    move = new MoveAnimation(600, std::min((float)tex.screen_width, distance) * tex.screen_scale, false, false, 0, stretch->duration*1.5);
    fade = new FadeAnimation(100, 0.0, false, false, 1.0);
    stretch->start();
    scale->start();
    move->start();
    fade->start();
    move_left  = nullptr;
    move_right = nullptr;
}

void GenreBG::exit(float left_position, float right_position, FolderBox* center_box) {
    float left_start  = (left_position  < 0.f || left_position  > tex.screen_width) ? 0.f              : left_position;
    float right_start = (right_position < 0.f || right_position > tex.screen_width) ? tex.screen_width : right_position;

    int left_distance  = (int)(442.f - left_start) * tex.screen_scale;
    int right_distance = (int)(835.f - right_start) * tex.screen_scale;

    move_left  = new MoveAnimation(200, left_distance,  false, false, (int)left_start,  166);
    move_right = new MoveAnimation(200, right_distance, false, false, (int)right_start, 166);
    float delay = std::max(move_left->duration, move_right->duration);
    fade = new FadeAnimation(100, 1.0, false, false, 0.0, delay);
    move_left->start();
    move_right->start();
    fade->start();
    center_box->exit_box();
    center_box->fade_in(delay / 2);
}

void GenreBG::fade_out() {
    fade = new FadeAnimation(300);
    fade->start();
}
void GenreBG::fade_in() {
    fade = new FadeAnimation(300, 0.0, false, false, 1.0);
    fade->start();
}

bool GenreBG::is_finished() {
    return move->is_finished;
}

bool GenreBG::is_complete() {
    if (move_left != nullptr && move_right != nullptr) {
        return move_left->is_finished && move_right->is_finished;
    }
    return true;
}

void GenreBG::update(double current_ms, FolderBox* box) {
    stretch->update(current_ms);
    scale->update(current_ms);
    move->update(current_ms);
    fade->update(current_ms);
    if (move_left != nullptr) {
        move_left->update(current_ms);
    }
    if (move_right != nullptr) {
        move_right->update(current_ms);
    }
    if (box != nullptr) {
        box->update(current_ms);
    }
}

void GenreBG::draw_anim(FolderBox* box) {
    if (box == nullptr) return;
    float s = (float)scale->attribute;
    float offset = 5 * s;
    float edge_width = tex.textures["box"]["folder_background_edge"]->width * s;

    float center = (box->left_bound + box->right_bound) / 2.0f;
    float half_width = (box->right_bound - box->left_bound) / 2.0f;
    float base_start = center - half_width * s;
    float base_end   = center + half_width * s;

    bool exiting = (move_left != nullptr && move_right != nullptr);
    float start_position = exiting ? base_start + (float)move_left->attribute : base_start;
    float end_position   = exiting ? base_end   + (float)move_right->attribute : base_end + (float)move->attribute;

    if (shader_loaded) ray::BeginShaderMode(shader);
    tex.draw_texture("box", "folder_background_edge", {
        .frame=(int)texture_index, .scale=s, .center=true, .mirror="horizontal",
        .x=start_position - offset,
        .y=-(float)stretch->attribute * s,
        .y2=(float)stretch->attribute * s,
        .fade=fade->attribute
    });
    tex.draw_texture("box", "folder_background", {
        .frame=(int)texture_index, .scale=s, .center=true,
        .x=start_position + edge_width - offset,
        .y=-(float)stretch->attribute * s,
        .x2=end_position - start_position - edge_width - offset,
        .y2=(float)stretch->attribute * s,
        .fade=fade->attribute
    });
    tex.draw_texture("box", "folder_background_edge", {
        .frame=(int)texture_index, .scale=s, .center=true,
        .x=end_position - edge_width + offset,
        .y=-(float)stretch->attribute * s,
        .y2=(float)stretch->attribute * s,
        .fade=fade->attribute
    });

    float edge_width_top = tex.textures["box"]["folder_background_folder_edge"]->width * s;
    float dest_width = std::min(tex.skin_config["genre_bg_title"].width, name->width) * s;

    tex.draw_texture("box", "folder_background_folder_edge", {
        .frame=(int)texture_index, .scale=s, .center=true,
        .mirror="horizontal",
        .x=center - dest_width / 2 - edge_width_top,
        .y=-(float)stretch->attribute * s,
        .y2=(float)stretch->attribute * s,
        .fade=fade->attribute
    });
    tex.draw_texture("box", "folder_background_folder", {
        .frame=(int)texture_index, .scale=s, .center=true,
        .x=center - dest_width / 2,
        .y=-(float)stretch->attribute * s,
        .x2=dest_width,
        .y2=(float)stretch->attribute * s,
        .fade=fade->attribute
    });
    tex.draw_texture("box", "folder_background_folder_edge", {
        .frame=(int)texture_index, .scale=s, .center=true,
        .x=center + dest_width / 2,
        .y=-(float)stretch->attribute * s,
        .y2=(float)stretch->attribute * s,
        .fade=fade->attribute
    });

    if (shader_loaded) ray::EndShaderMode();

    name->draw({.center=true,
                .x=center - dest_width / 2,
                .y=tex.skin_config["genre_bg_title"].y - (float)stretch->attribute * s,
                .x2 = dest_width - name->width,
                .y2=(float)stretch->attribute * s,
                .fade=fade->attribute
    });
}

void GenreBG::draw_exit_anim(float start_position, float end_position, FolderBox* box) {
    float s = (float)scale->attribute;
    float offset = 5 * s;
    float edge_width = tex.textures["box"]["folder_background_edge"]->width * s;

    float anim_start = (float)move_left->attribute;
    float anim_end   = (float)move_right->attribute;

    if (shader_loaded) ray::BeginShaderMode(shader);

    tex.draw_texture("box", "folder_background_edge", {
        .frame=(int)texture_index, .scale=s, .mirror="horizontal",
        .x=anim_start - offset,
        .fade=fade->attribute
    });
    tex.draw_texture("box", "folder_background", {
        .frame=(int)texture_index, .scale=s,
        .x=anim_start + edge_width - offset,
        .x2=anim_end - anim_start - edge_width - offset,
        .fade=fade->attribute
    });
    tex.draw_texture("box", "folder_background_edge", {
        .frame=(int)texture_index, .scale=s,
        .x=anim_end - edge_width + offset,
        .fade=fade->attribute
    });

    float edge_width_top = tex.textures["box"]["folder_background_folder_edge"]->width;
    float dest_width = std::min(tex.skin_config["genre_bg_title"].width, name->width);
    float center = 638.5f * tex.screen_scale;

    tex.draw_texture("box", "folder_background_folder_edge", {
        .frame=(int)texture_index, .mirror="horizontal",
        .x=center - dest_width / 2 - edge_width_top,
        .fade=fade->attribute
    });
    tex.draw_texture("box", "folder_background_folder", {
        .frame=(int)texture_index,
        .x=center - dest_width / 2,
        .x2=dest_width,
        .fade=fade->attribute
    });
    tex.draw_texture("box", "folder_background_folder_edge", {
        .frame=(int)texture_index,
        .x=center + dest_width / 2,
        .fade=fade->attribute
    });

    if (shader_loaded) ray::EndShaderMode();

    name->draw({.x=center - dest_width / 2,
                .y=tex.skin_config["genre_bg_title"].y,
                .x2 = dest_width - name->width, .fade=fade->attribute});

    box->draw(false);
}

void GenreBG::draw(float start_position, float end_position, FolderBox* folder) {
    float screen_width = tex.screen_width;

    if (!is_finished()) {
        draw_anim(folder);
        return;
    } else if (!is_complete()) {
        draw_exit_anim(start_position, end_position, folder);
        return;
    }

    float edge_width_top = tex.textures["box"]["folder_background_folder_edge"]->width;
    float dest_width = std::min(tex.skin_config["genre_bg_title"].width, name->width);
    float center = 638.5f * tex.screen_scale;

    float bg_start_pos = start_position;
    float bg_end_pos = end_position;
    if ((442 * tex.screen_scale) < start_position && start_position < center) {
        bg_start_pos = 442 * tex.screen_scale;
    }
    if (center < end_position && end_position < (835 * tex.screen_scale)) {
        bg_end_pos = (835 * tex.screen_scale);
    }

    if (shader_loaded) ray::BeginShaderMode(shader);

    if (bg_start_pos > bg_end_pos) {
        // Wraps around — two segments
        float draw_start_l = 0.f;
        float offset = 5;
        float draw_end_l   = std::min(bg_end_pos, screen_width);
        if (draw_start_l < draw_end_l) {
            float edge_width = tex.textures["box"]["folder_background_edge"]->width;
            if (bg_end_pos < screen_width) {
                tex.draw_texture("box", "folder_background", {.frame=(int)texture_index, .x=draw_start_l, .x2=draw_end_l - edge_width + offset, .fade=fade->attribute});
                tex.draw_texture("box", "folder_background_edge", {.frame=(int)texture_index, .x=draw_end_l - edge_width + offset, .fade=fade->attribute});
            } else {
                tex.draw_texture("box", "folder_background", {.frame=(int)texture_index, .x=draw_start_l, .x2=draw_end_l, .fade=fade->attribute});
            }
        }

        float draw_start_r = std::max(bg_start_pos, 0.f);
        float draw_end_r   = screen_width;
        if (draw_start_r < draw_end_r) {
            float edge_width = tex.textures["box"]["folder_background_edge"]->width;
            if (bg_start_pos > 0) {
                tex.draw_texture("box", "folder_background", {.frame=(int)texture_index, .x=draw_start_r + edge_width - offset, .x2=draw_end_r, .fade=fade->attribute});
                tex.draw_texture("box", "folder_background_edge", {.frame=(int)texture_index, .mirror="horizontal", .x=draw_start_r - offset, .fade=fade->attribute});
            } else {
                tex.draw_texture("box", "folder_background", {.frame=(int)texture_index, .x=draw_start_r, .x2=draw_end_r, .fade=fade->attribute});
            }
        }
    } else {
        // Normal case — clamp to screen
        float draw_start = std::max(start_position, 0.f);
        float draw_end   = std::min(end_position,   screen_width);
        if (draw_start < draw_end) {
            tex.draw_texture("box", "folder_background", {
                .frame=(int)texture_index, .x=draw_start, .x2=draw_end, .fade=fade->attribute
            });
        }
    }

    if (!(start_position < center || center < end_position)) {
        if (shader_loaded) ray::EndShaderMode();
        return;
    }

    tex.draw_texture("box", "folder_background_folder_edge", {
        .frame=(int)texture_index, .mirror="horizontal",
        .x=center - dest_width / 2 - edge_width_top,
        .fade=fade->attribute
    });
    tex.draw_texture("box", "folder_background_folder", {
        .frame=(int)texture_index,
        .x=center - dest_width / 2,
        .x2=dest_width,
        .fade=fade->attribute
    });
    tex.draw_texture("box", "folder_background_folder_edge", {
        .frame=(int)texture_index,
        .x=center + dest_width / 2,
        .fade=fade->attribute
    });

    if (shader_loaded) ray::EndShaderMode();

    name->draw({.x=center - dest_width / 2,
                .y=tex.skin_config["genre_bg_title"].y,
                .x2 = dest_width - name->width, .fade=fade->attribute});
}
