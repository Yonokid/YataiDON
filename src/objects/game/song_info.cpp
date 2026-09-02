#include "song_info.h"
#include "../../libs/global_data.h"

static float skin_outline(const SkinInfo& s) { return s.outline >= 0 ? s.outline : 5.0f; }

SongInfo::SongInfo(const std::string& song_name, const std::string& subtitle, bool show_subtitle, int genre, int song_num, int song_total)
    : song_name(song_name), genre(genre) {

    song_title = std::make_unique<OutlinedText>(song_name, tex.skin_config[SC::SONG_INFO].font_size, ray::WHITE, ray::BLACK, false,
                                                skin_outline(tex.skin_config[SC::SONG_INFO]));
    if (show_subtitle && !subtitle.empty()) {
        song_subtitle = std::make_unique<OutlinedText>(subtitle, tex.skin_config[SC::SONG_INFO_SUBTITLE].font_size, ray::WHITE, ray::BLACK, false, 5);
    }
    const SkinInfo* plate_cfg = tex.skin_entry("song_num_game");
    this->song_num = std::make_unique<SongNum>(
        song_num, plate_cfg ? plate_cfg->outline : -1.0f);
    if (song_total > 0 && tex.skin_entry("song_num_max"))
        song_max = std::make_unique<SongNum>(song_total, "song_num_max");
    fade = (FadeAnimation*)tex.get_animation(3);
}

void SongInfo::update(double current_ms) {
    fade->update(current_ms);
}

void SongInfo::draw() {
    float text_x = tex.skin_config[SC::SONG_INFO].x;
    float text_y = tex.skin_config[SC::SONG_INFO].y - song_title->height / 2.0f;

    float title_x = text_x - song_title->width;
    if (const SkinInfo* c = tex.skin_entry("song_info_center")) {
        if (c->width > 0 && song_title->width <= c->width)
            title_x = c->x - song_title->width / 2.0f;
    }

    if (const SkinInfo* plate = tex.skin_entry("song_num_game")) {
        song_title->draw({.x=title_x, .y=text_y, .fade=1.0});
        if (genre < 9) {
            tex.draw_texture(SONG_INFO::GENRE, {.frame = genre, .fade = 1 - fade->attribute,});
        }
        if (tex.has_texture("song_info/song_num_plate")) {
            tex.draw_texture(tex.get_enum("song_info/song_num_plate"), {.fade = fade->attribute});
        }
        song_num->draw(plate->x - song_num->width / 2.0f, plate->y - song_num->height / 2.0f, fade->attribute);
        if (song_max) {
            if (const SkinInfo* m = tex.skin_entry("song_num_max_game"))
                song_max->draw(m->x - song_max->width / 2.0f,
                               m->y - song_max->height / 2.0f, fade->attribute);
        }
        return;
    }

    song_num->draw(text_x - song_num->width, text_y, fade->attribute);

    song_title->draw({.x=title_x, .y=text_y, .fade=1 - fade->attribute});

    if (song_subtitle) {
        float sub_y = tex.skin_config[SC::SONG_INFO_SUBTITLE].y - song_subtitle->height / 2.0f;
        song_subtitle->draw({.x=text_x - song_subtitle->width, .y=sub_y, .fade=1 - fade->attribute});
    }

    if (genre < 9) {
        float genre_y_offset = song_subtitle ? song_subtitle->height : 0;
        tex.draw_texture(SONG_INFO::GENRE, {.frame = genre, .y = genre_y_offset, .fade = 1 - fade->attribute,});
    }
}

SongNum::SongNum(int song_num, float outline_override) {
    std::string song_format = tex.skin_config[SC::SONG_NUM].text[global_data.config->general.language];
    size_t pos = song_format.find("{0}");
    if (pos != std::string::npos) {
        song_format.replace(pos, 3, std::to_string(song_num));
    }
    ray::Color outline_color;
    if (global_data.config->general.song_limit > 0 && global_data.config->general.song_limit == song_num) {
        outline_color = ray::RED;
    } else {
        outline_color = ray::BLACK;
    }
    text = std::make_unique<OutlinedText>(song_format, tex.skin_config[SC::SONG_NUM].font_size, ray::WHITE, outline_color, false,
                                          outline_override >= 0 ? outline_override
                                                                : skin_outline(tex.skin_config[SC::SONG_NUM]));
    width = text->width;
    height = text->height;
}

SongNum::SongNum(int value, const std::string& config_key) {
    const SkinInfo* cfg = tex.skin_entry(config_key);
    if (!cfg) return;
    std::string fmt;
    auto it = cfg->text.find(global_data.config->general.language);
    if (it != cfg->text.end()) fmt = it->second;
    else if (!cfg->text.empty()) fmt = cfg->text.begin()->second;
    else fmt = "{0}";
    size_t pos = fmt.find("{0}");
    if (pos != std::string::npos) fmt.replace(pos, 3, std::to_string(value));
    text = std::make_unique<OutlinedText>(fmt, cfg->font_size, ray::WHITE, ray::BLACK, false, skin_outline(*cfg));
    width = text->width;
    height = text->height;
}

void SongNum::draw(float x, float y, float fade) {
    if (!text) return;
    text->draw({.x=x, .y=y, .fade = fade});
}
