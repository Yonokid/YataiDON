#include "song_info.h"

#include <stdexcept>

#include "../../libs/global_data.h"
#include "../enums.h"
#include "../song_select/file_navigator/color_utils.h"

static const Color GENRE_PLATE_TEMPLATE_COLOR{60, 103, 0, 255};

static float skin_outline(const SkinInfo& s) {
    return s.outline >= 0 ? s.outline : 5.0f;
}

SongInfo::SongInfo(const std::string& song_name, const std::string& subtitle, bool show_subtitle, int genre,
                   int song_num, int song_total, const std::string& genre_label)
    : song_name(song_name), genre(genre >= 0 && genre < 9 ? genre : 0) {

    song_title =
        std::make_unique<OutlinedText>(song_name, tex.get_skin_config(SC::GAME_SONG_INFO)->font_size, Color::WHITE,
                                       Color::BLACK, false, skin_outline(*tex.get_skin_config(SC::GAME_SONG_INFO)));
    if (show_subtitle && !subtitle.empty()) {
        song_subtitle =
            std::make_unique<OutlinedText>(subtitle, tex.get_skin_config(SC::GAME_SONG_INFO_SUBTITLE)->font_size,
                                           Color::WHITE, Color::BLACK, false, 5);
    }
    const SkinInfo* plate_cfg = tex.get_skin_config(SC::SONG_NUM_GAME);
    this->song_num = std::make_unique<SongNum>(song_num, plate_cfg ? plate_cfg->outline : -1.0f);
    if (song_total > 0 && tex.get_skin_config(SC::SONG_NUM_MAX_GAME)) {
        song_max = std::make_unique<SongNum>(song_total, SC::SONG_NUM_MAX_GAME, "song_num_max_game");
    }
    fade = dynamic_cast<FadeAnimation*>(tex.get_animation(3));
    if (!fade) {
        throw std::runtime_error("SongInfo: animation 3 has an unexpected type");
    }

    t_genre = tex.get_texture("song_info/genre");
    t_song_num_plate =
        tex.has_texture("song_info/song_num_plate") ? tex.get_texture("song_info/song_num_plate") : nullptr;

    int font_size = 18 * tex.screen_scale;
    genre_text = std::make_unique<OutlinedText>(genre_label, font_size, Color::WHITE, Color::BLANK, false, 3, 1);

    GenreIndex genre_bucket = this->genre < 8 ? static_cast<GenreIndex>(this->genre + 1) : GenreIndex::DEFAULT;
    auto color_it = DEFAULT_COLORS.find(genre_bucket);
    Color target = (color_it != DEFAULT_COLORS.end() && color_it->second[1].has_value()) ? color_it->second[1].value()
                                                                                         : Color{101, 0, 82, 255};

    genre_shader = load_shader("shader/dummy.vs", "shader/colortransform.fs");
    genre_shader_loaded = genre_shader.id != 0;
    if (genre_shader_loaded) {
        float src[3] = {GENRE_PLATE_TEMPLATE_COLOR.r / 255.0f, GENRE_PLATE_TEMPLATE_COLOR.g / 255.0f,
                        GENRE_PLATE_TEMPLATE_COLOR.b / 255.0f};
        float tgt[3] = {target.r / 255.0f, target.g / 255.0f, target.b / 255.0f};
        set_shader_value(genre_shader, get_shader_location(genre_shader, "sourceColor"), src, shader_uniform::VEC3);
        set_shader_value(genre_shader, get_shader_location(genre_shader, "targetColor"), tgt, shader_uniform::VEC3);
    }
}

SongInfo::SongInfo(SongInfo&& other) noexcept
    : song_name(std::move(other.song_name)), genre(other.genre), fade(other.fade),
      song_title(std::move(other.song_title)), song_subtitle(std::move(other.song_subtitle)),
      genre_text(std::move(other.genre_text)), song_num(std::move(other.song_num)), song_max(std::move(other.song_max)),
      t_genre(other.t_genre), t_song_num_plate(other.t_song_num_plate), genre_shader(other.genre_shader),
      genre_shader_loaded(other.genre_shader_loaded) {
    other.genre_shader_loaded = false;
}

SongInfo& SongInfo::operator=(SongInfo&& other) noexcept {
    if (this != &other) {
        if (genre_shader_loaded) {
            unload_shader(genre_shader);
        }
        song_name = std::move(other.song_name);
        genre = other.genre;
        fade = other.fade;
        song_title = std::move(other.song_title);
        song_subtitle = std::move(other.song_subtitle);
        genre_text = std::move(other.genre_text);
        song_num = std::move(other.song_num);
        song_max = std::move(other.song_max);
        t_genre = other.t_genre;
        t_song_num_plate = other.t_song_num_plate;
        genre_shader = other.genre_shader;
        genre_shader_loaded = other.genre_shader_loaded;
        other.genre_shader_loaded = false;
    }
    return *this;
}

SongInfo::~SongInfo() {
    if (genre_shader_loaded) {
        unload_shader(genre_shader);
    }
}

void SongInfo::update(double current_ms) {
    if (fade) {
        fade->update(current_ms);
    }
}

void SongInfo::draw(float dx, float dy) {
    if (!fade) {
        return;
    }
    float text_x = tex.get_skin_config(SC::GAME_SONG_INFO)->x + dx;
    float text_y = tex.get_skin_config(SC::GAME_SONG_INFO)->y + dy - song_title->height / 2.0f;

    // Optional skin key game_song_info_max_width: a longer title is squeezed horizontally to fit.
    float title_w = song_title->width;
    float title_x2 = 0.0f;
    if (const SkinInfo* m = tex.get_skin_config(SC::GAME_SONG_INFO_MAX_WIDTH);
        m && m->width > 0 && title_w > m->width) {
        title_x2 = m->width - title_w;
        title_w = m->width;
    }

    float title_x = text_x - title_w;
    if (const SkinInfo* c = tex.get_skin_config(SC::GAME_SONG_INFO_CENTER); c && c->width > 0 && title_w <= c->width) {
        title_x = c->x + dx - title_w / 2.0f;
    }

    if (const SkinInfo* plate = tex.get_skin_config(SC::SONG_NUM_GAME)) {
        song_title->draw({.x = title_x, .y = text_y, .x2 = title_x2, .fade = 1 - fade->attribute});
        if (genre_text && t_genre) {
            if (genre_shader_loaded) {
                begin_shader_mode(genre_shader);
            }
            tex.draw_texture(t_genre, {.x = dx, .y = dy, .fade = 1 - fade->attribute});
            if (genre_shader_loaded) {
                end_shader_mode();
            }
            genre_text->draw({.x = t_genre->x[0] + dx + t_genre->width / 2.0f - genre_text->width / 2.0f,
                              .y = t_genre->y[0] + dy + t_genre->height / 2.0f - genre_text->height / 2.0f,
                              .fade = 1 - fade->attribute});
        }
        if (t_song_num_plate) {
            tex.draw_texture(t_song_num_plate, {.x = dx, .y = dy, .fade = fade->attribute});
        }
        song_num->draw(plate->x + dx - song_num->width / 2.0f, plate->y + dy - song_num->height / 2.0f,
                       fade->attribute);
        if (const SkinInfo* m = tex.get_skin_config(SC::SONG_NUM_MAX_GAME); song_max && m) {
            song_max->draw(m->x + dx - song_max->width / 2.0f, m->y + dy - song_max->height / 2.0f, fade->attribute);
        }
        return;
    }

    song_num->draw(text_x - song_num->width, text_y, fade->attribute);

    song_title->draw({.x = title_x, .y = text_y, .x2 = title_x2, .fade = 1 - fade->attribute});

    if (song_subtitle) {
        float sub_y = tex.get_skin_config(SC::GAME_SONG_INFO_SUBTITLE)->y + dy - song_subtitle->height / 2.0f;
        song_subtitle->draw({.x = text_x - song_subtitle->width, .y = sub_y, .fade = 1 - fade->attribute});
    }

    if (genre_text && t_genre) {
        float genre_y_offset = (song_subtitle ? song_subtitle->height : 0) + dy;
        if (genre_shader_loaded) {
            begin_shader_mode(genre_shader);
        }
        tex.draw_texture(t_genre, {.x = dx, .y = genre_y_offset, .fade = 1 - fade->attribute});
        if (genre_shader_loaded) {
            end_shader_mode();
        }
        genre_text->draw({.x = t_genre->x[0] + dx + t_genre->width / 2.0f - genre_text->width / 2.0f,
                          .y = t_genre->y[0] + genre_y_offset + t_genre->height / 2.0f - genre_text->height / 2.0f,
                          .fade = 1 - fade->attribute});
    }
}

SongNum::SongNum(int song_num, float outline_override) {
    static const SkinInfo default_info{};
    const SkinInfo* cfg_ptr = font_manager.get_text_info("song_num");
    const SkinInfo& cfg = cfg_ptr ? *cfg_ptr : default_info;

    std::string song_format = cfg.text.empty() ? "{0}" : font_manager.get_text_label("song_num");
    size_t pos = song_format.find("{0}");
    if (pos != std::string::npos) {
        song_format.replace(pos, 3, std::to_string(song_num));
    }
    Color outline_color;
    if (global_data.config->limits.song_limit > 0 && global_data.config->limits.song_limit == song_num) {
        outline_color = Color::RED;
    } else {
        outline_color = Color::BLACK;
    }
    text = std::make_unique<OutlinedText>(song_format, cfg.font_size, Color::WHITE, outline_color, false,
                                          outline_override >= 0 ? outline_override : skin_outline(cfg));
    width = text->width;
    height = text->height;
}

SongNum::SongNum(int value, SC key, const std::string& config_key) {
    const SkinInfo* cfg = tex.get_skin_config(key);
    if (!cfg) {
        width = 0.0f;
        height = 0.0f;
        return;
    }
    const SkinInfo* text_cfg = font_manager.get_text_info(config_key);
    std::string fmt = text_cfg && !text_cfg->text.empty() ? font_manager.get_text_label(config_key) : "{0}";
    size_t pos = fmt.find("{0}");
    if (pos != std::string::npos) {
        fmt.replace(pos, 3, std::to_string(value));
    }
    text = std::make_unique<OutlinedText>(fmt, cfg->font_size, Color::WHITE, Color::BLACK, false, skin_outline(*cfg));
    width = text->width;
    height = text->height;
}

void SongNum::draw(float x, float y, float fade) {
    if (!text) {
        return;
    }
    text->draw({.x = x, .y = y, .fade = fade});
}
