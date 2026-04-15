#include "note_arc.h"

std::unordered_map<NoteArc::CacheKey, std::vector<std::pair<int, int>>, NoteArc::CacheKeyHash> NoteArc::_arc_points_cache;

NoteArc::NoteArc(NoteType note_type, double current_ms, PlayerNum player_num, bool big, bool is_balloon, float start_x, float start_y)
    : note_type(note_type), start_ms(current_ms), player_num(player_num), is_big(big), is_balloon(is_balloon)
{
    arc_points = 100;
    arc_duration = 22;
    current_progress = 0;

    float curve_height = 425 * tex.screen_scale;
    this->start_x = start_x + (350 * tex.screen_scale);
    this->start_y = start_y + (8 * tex.screen_scale);
    end_x = 1158 * tex.screen_scale;
    end_y = -83 * tex.screen_scale;

    if (player_num == PlayerNum::P2) {
        this->start_y += (176 * tex.screen_scale);
        end_y += (372 * tex.screen_scale);
    }

    if (player_num == PlayerNum::P1) {
        // Control point influences the curve shape
        control_x = (this->start_x + end_x) / 2;
        control_y = std::min(this->start_y, end_y) - curve_height;  // Arc upward
    } else {
        control_x = (this->start_x + end_x) / 2;
        control_y = std::max(this->start_y, end_y) + curve_height;  // Arc downward
    }

    x_i = this->start_x;
    y_i = this->start_y;

    // Create cache key
    CacheKey cache_key = {this->start_x, this->start_y, end_x, end_y, control_x, control_y, arc_points};

    // Check if arc points are already cached
    if (_arc_points_cache.find(cache_key) == _arc_points_cache.end()) {
        std::vector<std::pair<int, int>> arc_points_list;
        arc_points_list.reserve(arc_points + 1);

        for (int i = 0; i <= arc_points; ++i) {
            float t = static_cast<float>(i) / arc_points;
            float t_inv = 1.0f - t;

            int x = static_cast<int>(t_inv * t_inv * this->start_x +
                                     2 * t_inv * t * control_x +
                                     t * t * end_x);
            int y = static_cast<int>(t_inv * t_inv * this->start_y +
                                     2 * t_inv * t * control_y +
                                     t * t * end_y);

            arc_points_list.emplace_back(x, y);
        }

        _arc_points_cache[cache_key] = arc_points_list;
    }

    arc_points_cache = &_arc_points_cache[cache_key];
}

void NoteArc::update(double current_ms) {
    double elapsed_time = (current_ms - start_ms) / 16.67;
    elapsed_time = std::max(0.0, std::min(elapsed_time, (double)arc_duration));

    current_progress = elapsed_time / arc_duration;

    int point_index = current_progress * arc_points;
    if (point_index < arc_points_cache->size()) {
        x_i = (*arc_points_cache)[point_index].first;
        y_i = (*arc_points_cache)[point_index].second;
    } else {
        x_i = arc_points_cache->back().first;
        y_i = arc_points_cache->back().second;
    }
}

void NoteArc::draw(float y, ray::Shader mask_shader) {
    if (is_balloon) {
        std::shared_ptr<TextureObject> rainbow = tex.textures[BALLOON::RAINBOW];
        float rainbow_height;
        if (player_num == PlayerNum::P2) {
            rainbow_height = -rainbow->height;
        } else {
            rainbow_height = rainbow->height;
        }
        float trail_length_ratio = 0.5f;
        float trail_start_progress = std::max(0.0f, current_progress - trail_length_ratio);
        float trail_end_progress = current_progress;

        if (trail_end_progress > trail_start_progress) {
            float crop_start_x = int(trail_start_progress * rainbow->width);
            float crop_end_x = int(trail_end_progress * rainbow->width);
            float crop_width = crop_end_x - crop_start_x;

            if (crop_width > 0) {
                ray::Rectangle src = {crop_start_x, 0, crop_width, rainbow_height};
                std::string mirror;
                float y_pos;
                if (player_num == PlayerNum::P2) {
                    mirror = "vertical";
                    y_pos = (435 * tex.screen_scale);
                } else {
                    mirror = "";
                    y_pos = 0;
                }
                ray::BeginShaderMode(mask_shader);
                tex.draw_texture(BALLOON::RAINBOW_MASK, {.mirror=mirror, .x=crop_start_x, .y=y + y_pos, .x2=-rainbow->width + crop_width, .src=src});
                ray::EndShaderMode();
            }
        }
    }
    tex.draw_texture(tex_id_map.at("notes/" + (std::to_string((int)note_type))), {.x=x_i, .y=y + y_i});
}

bool NoteArc::is_finished() const {
    return current_progress >= 1.0;
}
