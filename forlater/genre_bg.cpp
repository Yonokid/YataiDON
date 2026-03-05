#include "genre_bg.h"

GenreBG::GenreBG(BaseBox* start_box, BaseBox* end_box, OutlinedText* title, std::optional<int> diff_sort)
    : title(title), start_box(start_box), end_box(end_box), diff_num(diff_sort)
{
    start_position      = start_box->position;
    end_position_final  = end_box->position;
    color               = end_box->back_color;
    shader_loaded       = false;

    float fade_dur = 133;

    fade_in    = new FadeAnimation(fade_dur, 0.0f, false, 1.0f, false, 0.0, std::nullopt, "quadratic", 50);
    move       = new MoveAnimation(316, std::abs(end_position_final - start_position), false, false, 0, fade_dur / 2.0f, std::nullopt, "quadratic");
    box_fade_in = new FadeAnimation(fade_dur, 0.0f, false, 1.0f, false, 0.0, std::nullopt, std::nullopt, move->duration);

    fade_in->start();
    move->start();
    box_fade_in->start();

    end_position = start_position + move->attribute;
}

GenreBG::~GenreBG() {
    if (shader_loaded)
        ray::UnloadShader(shader);
}

void GenreBG::load_shader() {
    if (color.has_value()) {
        shader = ray::LoadShader("shader/dummy.vs", "shader/colortransform.fs");
        constexpr ray::Color source_rgb = ray::Color(142, 212, 30);
        const auto& target_rgb = *color;
        float src[3] = { source_rgb.r / 255.0f, source_rgb.g / 255.0f, source_rgb.b / 255.0f };
        float tgt[3] = { target_rgb.r / 255.0f, target_rgb.g / 255.0f, target_rgb.b / 255.0f };
        int source_loc = ray::GetShaderLocation(shader, "sourceColor");
        int target_loc = ray::GetShaderLocation(shader, "targetColor");
        ray::SetShaderValue(shader, source_loc, src, ray::SHADER_UNIFORM_VEC3);
        ray::SetShaderValue(shader, target_loc, tgt, ray::SHADER_UNIFORM_VEC3);
    }
    shader_loaded = true;
}

void GenreBG::update(double current_ms) {
    start_position = start_box->position;
    end_position   = start_position + move->attribute;
    if (move->is_finished)
        end_position = end_box->position;
    box_fade_in->update(current_ms);
    fade_in->update(current_ms);
    move->update(current_ms);
}

void GenreBG::draw(float y) {
    bool use_shader = shader_loaded && color.has_value() && end_box->texture_index == TextureIndex::NONE;
    int frame = (int)end_box->texture_index;

    if (use_shader) ray::BeginShaderMode(shader);

    // Left edge
    float offset = start_box->is_open ? (tex.skin_config["genre_bg_offset"].x * -1.0f) : 0.0f;
    if (344.0f * tex.screen_scale < start_box->position && start_box->position < 594.0f * tex.screen_scale)
        offset = -start_position + 444.0f * tex.screen_scale;
    tex.draw_texture("box", "folder_background_edge", {
        .frame=frame, .mirror="horizontal", .x=start_position+offset, .y=y,
        .fade=fade_in->attribute
    });

    // Center background
    float extra_distance = 0.0f;
    if (end_box->is_open || (start_box->is_open &&
        (844.0f * tex.screen_scale) <= end_position &&
        end_position <= (1144.0f * tex.screen_scale)))
    {
        extra_distance = tex.skin_config["genre_bg_extra_distance"].x;
    }

    float x, x2;
    float left_max = tex.skin_config["genre_bg_left_max"].x;
    if (start_position >= left_max && end_position < start_position) {
        x  = start_position + offset;
        x2 = start_position + tex.skin_config["genre_bg_offset_2"].x;
    } else if (start_position <= left_max && end_position < start_position) {
        x  = 0.0f;
        x2 = (float)tex.screen_width;
    } else {
        x  = start_position + offset;
        x2 = std::abs(end_position) - start_position + extra_distance
             + (-1.0f * left_max + (1.0f * tex.screen_scale));
    }
    tex.draw_texture("box", "folder_background", {.frame=frame, .x=x, .y=y, .x2=x2});

    // Wrapped left segment
    if (end_position < start_position && end_position >= left_max) {
        float folder_bg_width = tex.skin_config["genre_bg_folder_background"].width;
        float folder_bg_x     = tex.skin_config["genre_bg_folder_background"].x;
        x2 = std::min(end_position + folder_bg_width, (float)tex.screen_width) + extra_distance;
        tex.draw_texture("box", "folder_background", {.frame=frame, .x=folder_bg_x, .y=y, .x2=x2});
    }

    // Right edge
    if (594.0f * tex.screen_scale < end_box->position && end_box->position < 844.0f * tex.screen_scale)
        offset = -end_position + 674.0f * tex.screen_scale;
    offset = end_box->is_open ? tex.skin_config["genre_bg_offset"].x : 0.0f;
    tex.draw_texture("box", "folder_background_edge", {
        .frame=frame,
        .x=end_position + tex.skin_config["genre_bg_folder_edge"].x + offset,
        .y=y,
        .fade=fade_in->attribute
    });

    if (use_shader) ray::EndShaderMode();

    // Title overlay (only when spanning BOX_CENTER)
    bool spans_center = false;
    /*    (start_position <= BOX_CENTER && end_position >= BOX_CENTER) ||
        ((start_position <= BOX_CENTER || end_position >= BOX_CENTER) &&
        start_position > end_position);*/

    if (spans_center) {
        float title_offset = diff_num.has_value() ? tex.skin_config["genre_bg_offset_3"].x : 0.0f;
        float dest_width = std::min(tex.skin_config["genre_bg_title"].width, (float)title->width);
        float folder_y   = y + tex.skin_config["genre_bg_folder_background_folder"].y;
        float folder_w   = tex.skin_config["genre_bg_folder_background_folder"].width;
        float folder_x   = tex.skin_config["genre_bg_folder_background_folder"].x;

        if (use_shader) ray::BeginShaderMode(shader);
        tex.draw_texture("box", "folder_background_folder", {
            .frame=frame,
            .x=-((title_offset + dest_width) / 2.0f),
            .y=folder_y,
            .x2=dest_width + title_offset + folder_w,
            .fade=fade_in->attribute
        });
        tex.draw_texture("box", "folder_background_folder_edge", {
            .frame=frame,
            .mirror="horizontal",
            .x=-((title_offset + dest_width) / 2.0f),
            .y=folder_y,
            .fade=fade_in->attribute
        });
        tex.draw_texture("box", "folder_background_folder_edge", {
            .frame=frame,
            .x=((title_offset + dest_width) / 2.0f) + folder_x,
            .y=folder_y,
            .fade=fade_in->attribute
        });
        if (use_shader) ray::EndShaderMode();

        if (diff_num.has_value()) {
            tex.draw_texture("diff_sort", "star_num", {
                .frame=*diff_num,
                .x=(tex.skin_config["genre_bg_offset"].x * -1.0f) + (dest_width / 2.0f),
                .y=tex.skin_config["diff_sort_star_num"].y
            });
        }

        title->draw({
            .x   = (tex.screen_width / 2.0f) - (dest_width / 2.0f) - (title_offset / 2.0f),
            .y   = y + tex.skin_config["genre_bg_title"].y,
            .x2  = dest_width - title->width,
            .fade = fade_in->attribute});
    }
}
