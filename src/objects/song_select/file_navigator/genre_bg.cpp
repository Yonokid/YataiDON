#include "genre_bg.h"

GenreBG::GenreBG(std::string& text_name, ray::Color color, TextureIndex texture_index)
: texture_index(texture_index) {
    float font_size = tex.skin_config["song_box_name"].font_size;
    if (utf8_char_count(text_name) >= 30)
        font_size -= (int)(10 * tex.screen_scale);
    name = make_unique<OutlinedText>(text_name, font_size, ray::WHITE, ray::BLACK, false);

    shader = ray::LoadShader("shader/dummy.vs", "shader/colortransform.fs");
    float src[3] = { 142 / 255.0f, 212 / 255.0f, 30 / 255.0f };
    float tgt[3] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f };
    int source_loc = ray::GetShaderLocation(shader, "sourceColor");
    int target_loc = ray::GetShaderLocation(shader, "targetColor");
    ray::SetShaderValue(shader, source_loc, src, ray::SHADER_UNIFORM_VEC3);
    ray::SetShaderValue(shader, target_loc, tgt, ray::SHADER_UNIFORM_VEC3);
}

void GenreBG::update(double current_ms) {

}

void GenreBG::draw(float start_position, float end_position) {
    float screen_width = tex.screen_width;

    ray::BeginShaderMode(shader);

    if (start_position > end_position) {
        // Wraps around — two segments
        float draw_start_l = 0.f;
        float offset = 5;
        float draw_end_l   = std::min(end_position, screen_width);
        if (draw_start_l < draw_end_l) {
            float edge_width = tex.textures["box"]["folder_background_edge"]->width;
            float edge_pos = tex.textures["box"]["folder_background_edge"]->x[0];
            if (end_position > 0) {
                tex.draw_texture("box", "folder_background", {.frame=(int)texture_index, .x=draw_start_l, .x2=draw_end_l - edge_width + offset});
                tex.draw_texture("box", "folder_background_edge", {.frame=(int)texture_index, .x=draw_end_l - edge_width + offset});
            } else {
                tex.draw_texture("box", "folder_background", {.frame=(int)texture_index, .x=draw_start_l, .x2=draw_end_l});
            }
        }

        float draw_start_r = std::max(start_position, 0.f);
        float draw_end_r   = screen_width;
        if (draw_start_r < draw_end_r) {
            float edge_width = tex.textures["box"]["folder_background_edge"]->width;
            float edge_pos = tex.textures["box"]["folder_background_edge"]->x[0];
            if (start_position > 0) {
                tex.draw_texture("box", "folder_background", {.frame=(int)texture_index, .x=draw_start_r + edge_width - offset, .x2=draw_end_r});
                tex.draw_texture("box", "folder_background_edge", {.frame=(int)texture_index, .mirror="horizontal", .x=draw_start_r - offset});
            } else {
                tex.draw_texture("box", "folder_background", {.frame=(int)texture_index, .x=draw_start_r, .x2=draw_end_r});
            }
        }
    } else {
        // Normal case — clamp to screen
        float draw_start = std::max(start_position, 0.f);
        float draw_end   = std::min(end_position,   screen_width);
        if (draw_start < draw_end) {
            tex.draw_texture("box", "folder_background", {
                .frame=(int)texture_index, .x=draw_start, .x2=draw_end
            });
        }
    }

    ray::EndShaderMode();
}
