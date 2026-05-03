#include "search_box.h"
#include "../../libs/text.h"

SearchBox::SearchBox() {
    current_search = "";
    bg_resize = (TextureChangeAnimation*)tex.get_animation(19);
    diff_fade_in = (FadeAnimation*)tex.get_animation(20);
    bg_resize->start();
    diff_fade_in->start();
    font = font_manager.get_font(current_search, 30);
}

void SearchBox::update(double current_ms) {
    bg_resize->update(current_ms);
    diff_fade_in->update(current_ms);
}

void SearchBox::draw() {
    ray::DrawRectangle(0, 0, tex.screen_width, tex.screen_height, ray::Fade(ray::BLACK, 0.6));
    tex.draw_texture(DIFF_SORT::BACKGROUND, {.scale=(float)bg_resize->attribute, .center=true});

    TextureObject* background = tex.textures[DIFF_SORT::BACKGROUND].get();
    float fade = diff_fade_in->attribute;

    float text_box_width = 400 * tex.screen_scale;
    float text_box_height = 60 * tex.screen_scale;
    float x = (float)background->width / 2 + background->x[0] - text_box_width / 2;
    float y = (float)background->height / 2 + background->y[0] - text_box_height / 2;

    ray::Rectangle text_box = {x, y, text_box_width, text_box_height};
    ray::DrawRectangleRec(text_box, ray::Fade(ray::LIGHTGRAY, fade));
    ray::DrawRectangleLines((int)text_box.x, (int)text_box.y, (int)text_box.width, (int)text_box.height, ray::Fade(ray::DARKGRAY, fade));

    ray::Vector2 text_size = ray::MeasureTextEx(font, current_search.c_str(), (int)(30 * tex.screen_scale), 1);
    ray::DrawTextEx(font, current_search.c_str(),
        ray::Vector2{x + text_box_width / 2 - text_size.x / 2, y + text_box_height / 2 - text_size.y / 2},
        (int)(30 * tex.screen_scale), 1, ray::BLACK);
}
