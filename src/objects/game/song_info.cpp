#include "song_info.h"
#include "../../libs/global_data.h"

SongInfo::SongInfo(const std::string& song_name, int genre)
    : song_name(song_name), genre(genre) {

    song_title = std::make_unique<OutlinedText>(song_name, tex.skin_config[SC::SONG_INFO].font_size, ray::WHITE, ray::BLACK, false, 5);
    song_num = std::make_unique<SongNum>(global_data.songs_played + 1);
    fade = (FadeAnimation*)tex.get_animation(3);
}

void SongInfo::update(double current_ms) {
    fade->update(current_ms);
}

void SongInfo::draw() {
    float text_x = tex.skin_config[SC::SONG_INFO].x;
    float text_y = tex.skin_config[SC::SONG_INFO].y - song_title->height / 2.0f;
    song_num->draw(text_x - song_num->width, text_y, fade->attribute);

    song_title->draw({.x=text_x - song_title->width, .y=text_y, .fade=1 - fade->attribute});

    if (genre < 9) {
        tex.draw_texture(SONG_INFO::GENRE, {.frame = genre, .fade = 1 - fade->attribute,});
    }
}

SongNum::SongNum(int song_num) {
    std::string song_format = tex.skin_config[SC::SONG_NUM].text[global_data.config->general.language];
    size_t pos = song_format.find("{0}");
    if (pos != std::string::npos) {
        song_format.replace(pos, 3, std::to_string(global_data.songs_played + 1));
    }
    text = std::make_unique<OutlinedText>(song_format, tex.skin_config[SC::SONG_NUM].font_size, ray::WHITE, ray::BLACK, false, 5);
    width = text->width;
    height = text->height;
}

void SongNum::draw(float x, float y, float fade) {
    text->draw({.x=x, .y=y, .fade = fade});
}
