#include "song_info.h"

SongInfo::SongInfo(const std::string& song_name, int genre)
    : song_name(song_name), genre(genre) {

    // song_title = new OutlinedText(song_name, tex.skin_config["song_info"].font_size, ray::WHITE, 5); // TODO: Implement OutlinedText class
    fade = (FadeAnimation*)tex.get_animation(3);
}

void SongInfo::update(double current_ms) {
    fade->update(current_ms);
}

void SongInfo::draw() {
    tex.draw_texture("song_info", "song_num", {.frame = global_data.songs_played % 4, .fade = fade->attribute});

    // TODO: Implement OutlinedText drawing
    // float text_x = tex.skin_config["song_info"].x - song_title->texture.width;
    // float text_y = tex.skin_config["song_info"].y - song_title->texture.height / 2.0f;
    // song_title->draw(ray::BLACK, text_x, text_y, ray::Fade(ray::WHITE, 1 - fade->attribute));

    if (genre < 9) {
        tex.draw_texture("song_info", "genre", {.frame = genre, .fade = 1 - fade->attribute,});
    }
}
