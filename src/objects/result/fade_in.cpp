#include "fade_in.h"

FadeIn::FadeIn(PlayerNum player_num) : player_num(player_num) {
    fade_in = (FadeAnimation*)tex.get_animation(15);
    fade_in->start();
}

void FadeIn::update(double current_ms) {
    fade_in->update(current_ms);
}

void FadeIn::draw() {
    float x = 0;
    float footer_height = tex.textures[BACKGROUND::FOOTER_1P]->height;
    if (player_num == PlayerNum::TWO_PLAYER) {
        while (x < tex.screen_width) {
            tex.draw_texture(BACKGROUND::BACKGROUND_1P, {.x=x, .y=(float)-tex.screen_height/2, .fade=fade_in->attribute});
            tex.draw_texture(BACKGROUND::BACKGROUND_2P, {.x=x, .y=(float)tex.screen_height/2, .fade=fade_in->attribute});
            tex.draw_texture(BACKGROUND::FOOTER_1P, {.x=x, .y=-footer_height, .fade=fade_in->attribute});
            tex.draw_texture(BACKGROUND::FOOTER_2P, {.x=x, .y=tex.screen_height - footer_height, .fade=fade_in->attribute});
            x += (float)tex.screen_width / 5;
        }
    } else {
        while (x < tex.screen_width) {
            std::string player_str = std::to_string(static_cast<int>(player_num)) + "p";
            tex.draw_texture(tex_id_map.at("background/background_" + player_str), {.x=x, .y=(float)-tex.screen_height/2, .fade=fade_in->attribute});
            tex.draw_texture(tex_id_map.at("background/background_" + player_str), {.x=x, .y=(float)tex.screen_height/2, .fade=fade_in->attribute});
            tex.draw_texture(tex_id_map.at("background/footer_" + player_str), {.x=x, .y=-footer_height, .fade=fade_in->attribute});
            x += (float)tex.screen_width / 5;
        }
    }
}

bool FadeIn::is_finished() {
    return fade_in->is_finished;
}
