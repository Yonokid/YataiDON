#include "song_info.h"

SongInfo::SongInfo(const std::string& song_name, int genre)
    : song_name(song_name), genre(genre) {

    song_title = new OutlinedText(song_name, tex.skin_config["song_info"].font_size, ray::WHITE, ray::BLACK, 5);
    fade = (FadeAnimation*)tex.get_animation(3);
}

void SongInfo::update(double current_ms) {
    fade->update(current_ms);
}

void SongInfo::draw() {
    tex.draw_texture("song_info", "song_num", {.frame = global_data.songs_played % 4, .fade = fade->attribute});

    float text_x = tex.skin_config["song_info"].x - song_title->width;
    float text_y = tex.skin_config["song_info"].y - song_title->height / 2.0f;
    song_title->draw(text_x, text_y, 1 - fade->attribute);

    if (genre < 9) {
        tex.draw_texture("song_info", "genre", {.frame = genre, .fade = 1 - fade->attribute,});
    }
}
