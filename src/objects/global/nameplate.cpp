#include "nameplate.h"

Nameplate::Nameplate(std::string name, std::string title, PlayerNum player_num, int dan, bool is_gold, bool is_rainbow, int title_bg)
 : dan_index(dan), player_num(player_num), is_gold(is_gold), is_rainbow(is_rainbow), title_bg(title_bg) {
     this->name = new OutlinedText(name, global_tex.skin_config["nameplate_text_name"].font_size, ray::WHITE, ray::BLACK, 3.0);
     this->title = new OutlinedText(title, global_tex.skin_config["nameplate_text_title"].font_size, ray::BLACK, ray::BLANK, 0.0);
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
    int frame;
    float title_offset;
    if ((int)player_num == 0) {
        frame = 2;
        title_offset = 0;
    } else {
        frame = title_bg;
        title_offset = global_tex.skin_config["nameplate_title_offset"].x;
    }
    if (is_rainbow) {
        if (0 < rainbow_animation->attribute && rainbow_animation->attribute < 6) {
            global_tex.draw_texture("nameplate", "frame_top_rainbow", {.frame=(int)rainbow_animation->attribute-1, .x=x, .y=y, .fade=fade});
        }
        global_tex.draw_texture("nameplate", "frame_top_rainbow", {.frame=(int)rainbow_animation->attribute, .x=x, .y=y, .fade=fade});
    } else {
        global_tex.draw_texture("nameplate", "frame_top", {.frame=frame, .x=x, .y=y, .fade=fade});
    }
    global_tex.draw_texture("nameplate", "outline", {.x=x, .y=y, .fade=fade});
    int offset = 0;
    if (dan_index != -1) {
        global_tex.draw_texture("nameplate", "dan_emblem_bg", {.x=x, .y=y, .fade=fade});
        if (is_gold) {
            global_tex.draw_texture("nameplate", "dan_emblem_gold", {.frame=dan_index, .x=x, .y=y, .fade=fade});
        } else {
            global_tex.draw_texture("nameplate", "dan_emblem", {.frame=dan_index, .x=x, .y=y, .fade=fade});
        }
        offset = global_tex.skin_config["nameplate_dan_offset"].x;
    }
    if ((int)player_num != 0) {
        std::string player_num_str = std::to_string((int)player_num);
        global_tex.draw_texture("nameplate", player_num_str + "p", {.x=x, .y=y, .fade=fade});
    }

    name->draw({.x=x+global_tex.skin_config["nameplate_text_name"].x - (std::min(global_tex.skin_config["nameplate_text_name"].width - offset*4, name->width)/2) + offset, .y=y+global_tex.skin_config["nameplate_text_name"].y + name->height/2, .x2=std::min(global_tex.skin_config["nameplate_text_name"].width - offset*4, name->width)-name->width, .fade=fade});
    title->draw({.x=x+global_tex.skin_config["nameplate_text_title"].x - (std::min(global_tex.skin_config["nameplate_text_title"].width - offset*2, title->width)/2) + title_offset, .y=y+global_tex.skin_config["nameplate_text_title"].y + title->height/2, .x2=std::min(global_tex.skin_config["nameplate_text_title"].width - offset*2, title->width)-title->width, .fade=fade});
}
