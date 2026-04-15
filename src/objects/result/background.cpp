#include "background.h"

ResultBackground::ResultBackground(PlayerNum player_num, float width) : player_num(player_num), width(width) {}

void ResultBackground::draw() {
    float x = 0;
    float footer_height;
    if (player_num == PlayerNum::TWO_PLAYER) {
        footer_height = tex.textures[BACKGROUND::FOOTER_1P]->height;
    } else {
        std::string footer_key = "footer_" + std::to_string((int)player_num) + "p";
        footer_height = tex.textures[tex_id_map.at("background/" + footer_key)]->height;
    }

    if (player_num == PlayerNum::TWO_PLAYER) {
        while (x < width) {
            tex.draw_texture(BACKGROUND::BACKGROUND_1P, {.x=x, .y=-((float)tex.screen_height/2)});
            tex.draw_texture(BACKGROUND::BACKGROUND_2P, {.x=x, .y=(float)tex.screen_height/2});
            tex.draw_texture(BACKGROUND::FOOTER_1P, {.x=x, .y=-(footer_height/2)});
            tex.draw_texture(BACKGROUND::FOOTER_2P, {.x=x, .y=(float)tex.screen_height-(footer_height/2)});
            x += (float)tex.screen_width / 5;
        }
    } else {
        while (x < width) {
            std::string footer_key = "footer_" + std::to_string((int)player_num) + "p";
            std::string background_key = "background_" + std::to_string((int)player_num) + "p";
            tex.draw_texture(tex_id_map.at("background/" + (background_key)), {.x=x, .y=-((float)tex.screen_height/2)});
            tex.draw_texture(tex_id_map.at("background/" + (background_key)), {.x=x, .y=(float)tex.screen_height/2});
            tex.draw_texture(tex_id_map.at("background/" + (footer_key)), {.x=x, .y=-(footer_height/2)});
            tex.draw_texture(tex_id_map.at("background/" + (footer_key)), {.x=x, .y=(float)tex.screen_height-(footer_height/2)});
            x += (float)tex.screen_width / 5;
        }
    }
    tex.draw_texture(BACKGROUND::RESULT_TEXT);
}
