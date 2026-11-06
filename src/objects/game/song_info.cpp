#include "song_info.h"

#include "../../libs/global_data.h"
#include "../enums.h"
#include "../song_select/file_navigator/color_utils.h"

SongInfo::SongInfo(const std::string& song_name, const std::string& subtitle, bool show_subtitle, int genre,
                   int song_num, const std::string& genre_label) {
    genre = genre >= 0 && genre < 9 ? genre : 0;
    GenreIndex genre_bucket = genre < 8 ? static_cast<GenreIndex>(genre + 1) : GenreIndex::DEFAULT;
    auto color_it = DEFAULT_COLORS.find(genre_bucket);
    Color color = (color_it != DEFAULT_COLORS.end() && color_it->second[1].has_value()) ? color_it->second[1].value()
                                                                                        : Color{101, 0, 82, 255};

    if (load("SongInfo", "song_info", song_name, show_subtitle ? subtitle : std::string(), song_num, genre_label,
             sol::as_table(std::array<int, 3>{color.r, color.g, color.b}))) {
        fn_update = lua_object["update"];
        fn_draw = lua_object["draw"];
    }
}

void SongInfo::update(double current_ms) {
    call(fn_update, "SongInfo:update", current_ms);
}

void SongInfo::draw(float dx, float dy) {
    call(fn_draw, "SongInfo:draw", dx, dy);
}

SongNum::SongNum(int song_num) {
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
                                          cfg.outline >= 0 ? cfg.outline : 5.0f);
    width = text->width;
    height = text->height;
}

void SongNum::draw(float x, float y, float fade) {
    if (!text) {
        return;
    }
    text->draw({.x = x, .y = y, .fade = fade});
}
