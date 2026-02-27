#include "nameplate.h"

Nameplate::Nameplate(std::string name, std::string title, PlayerNum player_num, int dan, bool is_gold, bool is_rainbow, int title_bg)
 : dan_index(dan), player_num(player_num), is_gold(is_gold), is_rainbow(is_rainbow), title_bg(title_bg) {
     this->name = new OutlinedText(name, global_tex.skin_config["nameplate_text_name"].font_size, ray::WHITE, ray::BLACK, false, 3.0);
     this->title = new OutlinedText(title, global_tex.skin_config["nameplate_text_title"].font_size, ray::BLACK, ray::BLANK, false, 0.0);
     rainbow_animation = (TextureChangeAnimation*)global_tex.get_animation(12);
 }

void Nameplate::update(double current_ms) {
    rainbow_animation->update(current_ms);
}

void Nameplate::draw(float x, float y, float fade) {
    global_tex.draw_texture("nameplate", "shadow", {.x=x, .y=y, .fade=std::min(0.5f, fade)});
    if (player_num == PlayerNum::AI) {
        global_tex.draw_texture("nameplate", "ai", {.x=x, .y=y});
        return;
    }
    int frame = ((int)player_num == 0) ? 2 : title_bg;
    if (is_rainbow) {
        if (0 < rainbow_animation->attribute && rainbow_animation->attribute < 6) {
            global_tex.draw_texture("nameplate", "frame_top_rainbow", {.frame=(int)rainbow_animation->attribute-1, .x=x, .y=y, .fade=fade});
        }
        global_tex.draw_texture("nameplate", "frame_top_rainbow", {.frame=(int)rainbow_animation->attribute, .x=x, .y=y, .fade=fade});
    } else {
        global_tex.draw_texture("nameplate", "frame_top", {.frame=frame, .x=x, .y=y, .fade=fade});
    }
    global_tex.draw_texture("nameplate", "outline", {.x=x, .y=y, .fade=fade});
    float dan_offset = 0;
    float padding = global_tex.skin_config["nameplate_text_padding"].x;
    if (dan_index != -1) {
        global_tex.draw_texture("nameplate", "dan_emblem_bg", {.x=x, .y=y, .fade=fade});
        if (is_gold) {
            global_tex.draw_texture("nameplate", "dan_emblem_gold", {.frame=dan_index, .x=x, .y=y, .fade=fade});
        } else {
            global_tex.draw_texture("nameplate", "dan_emblem", {.frame=dan_index, .x=x, .y=y, .fade=fade});
        }
        dan_offset = global_tex.textures["nameplate"]["dan_emblem"]->width;
    }

    float offset = 0;
    if ((int)player_num != 0) {
        std::string player_num_str = std::to_string((int)player_num);
        global_tex.draw_texture("nameplate", player_num_str + "p", {.x=x, .y=y, .fade=fade});
        offset = (global_tex.textures["nameplate"][player_num_str + "p"]->width / 2.0f);
    }

    float name_width = std::min(name->width, (float)global_tex.textures["nameplate"]["frame_top"]->width - padding - offset - dan_offset);
    float name_x = x + offset + dan_offset + ((global_tex.textures["nameplate"]["outline"]->width - offset - dan_offset) / 2.0f) - (name_width / 2);
    float name_y = y + (global_tex.textures["nameplate"]["outline"]->height / 2.0f) + global_tex.skin_config["nameplate_name_offset"].y;
    name->draw({.x=name_x, .y=name_y, .x2=name_width - name->width, .fade=fade});

    float title_width = std::min(title->width, (float)global_tex.textures["nameplate"]["frame_top"]->width - padding - offset);
    float title_x = x + offset + ((global_tex.textures["nameplate"]["frame_top"]->width - offset) / 2.0f) - (title_width / 2);
    float title_y = y + (global_tex.textures["nameplate"]["frame_top"]->height / 2.0f) - (title->height / 2) + global_tex.skin_config["nameplate_title_offset"].y;
    title->draw({.x=title_x, .y=title_y, .x2=title_width - title->width, .fade=fade});
}
