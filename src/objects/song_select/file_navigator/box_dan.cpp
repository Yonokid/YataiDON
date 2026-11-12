#include "box_dan.h"

#include <algorithm>
#include <array>
#include <string>

#include "../../../libs/file/scores.h"
#include "../../../libs/gfx/text.h"
#include "../../../libs/parsers/song_parser.h"

static BoxDef dan_box_def(const std::string& title, int color) {
    static const std::array<std::vector<const char*>, 5> GRADIENTS = {{
        {"#D3DBEC", "#D3DBEC", "#DEE3EF", "#E4EBF4", "#E9F0F7", "#E7F3F4", "#E7F3F4", "#E1F3F4", "#DEF3F1", "#DCF3F1",
         "#DBF3F1", "#D6EBEF", "#CEE1EC", "#C6D7E7", "#BDCFE7", "#B5C5E7", "#B5C5E7"},
        {"#F79242", "#F79242", "#F7A860", "#FFC17E", "#FFD999", "#FFEDAD", "#FFECAF", "#FFE0A7", "#FFD19C", "#FFC494",
         "#FFB68C", "#F79C84", "#E9777B", "#DE5673", "#D0356B", "#C61863", "#C61863"},
        {"#FF596E", "#FF596E", "#FF4682", "#ED35A8", "#D228D9", "#AE2DFF", "#756BFF", "#3FA6FF", "#24CCB7", "#0AF16F",
         "#2CFF4E", "#78FF44", "#B9F938", "#E3EA28", "#FFD823", "#FFC431", "#FFC431"},
        {"#D2A60D", "#D2A60D", "#C0A100", "#B5A200", "#CEB80D", "#E8D329", "#FFEC47", "#FDE240", "#EFC92D", "#DCAC1C",
         "#EFC931", "#FFF552", "#FFFF72", "#FFFF95", "#E8DE58", "#CAB11F", "#CAB11F"},
        {"#5AFD8E", "#5AFD8E", "#57FB94", "#52F79C", "#4CF3A5", "#4AF3AD", "#44EFB5", "#42EBBD", "#42E7C3", "#39E7CE",
         "#39E3D3", "#31DFD8", "#31DFE4", "#29DBEC", "#21D7EF", "#21D3F7", "#21D3F7"},
    }};
    BoxDef def;
    def.name = title;
    def.genre_index = GenreIndex::DAN;
    if (color == 3 || color == 4) {
        def.texture_index = color - 2;
    } else if (color >= 1 && color <= 5) {
        for (const char* hex : GRADIENTS[color - 1]) {
            def.box_color.push_back(parse_hex_color(hex));
        }
    }
    return def;
}

DanBox::DanBox(const fs::path& path, const std::string& title, int color, const std::vector<DanSongEntry>& songs_in,
               const std::vector<Exam>& exams_in, int total_notes_in)
    : BaseBox(path, dan_box_def(title, color)), dan_title(title), dan_color(color), songs(songs_in), exams(exams_in),
      total_notes(total_notes_in) {
    text_name = title;
}

static float dan_shrink_font(float font_size) {
    return std::max(font_size - 10.0f * tex.screen_scale, font_size * 0.6f);
}

void DanBox::load_text() {
    if (is_back) {
        text_loaded = true;
        return;
    }
    BaseBox::load_text(); // populates name (vertical) for closed state
    const SkinInfo* chip_slot = tex.get_skin_config(SC::DAN_SELECT_CHIP_NAME);
    float base_font;
    if (chip_slot && chip_slot->font_size > 0) {
        base_font = chip_slot->font_size;
    } else {
        base_font = tex.get_skin_config(SC::SONG_SELECT_BOX_NAME)->font_size;
    }

    float name_outline = 5.0f;
    if (utf8_char_count(text_name) >= 30) {
        float shrunk = dan_shrink_font(base_font);
        name_outline = 5.0f * (shrunk / base_font);
        base_font = shrunk;
    }
    const bool chip_black = tex.options[SCO::DAN_CHIP_NAME_BLACK];
    name =
        std::make_unique<OutlinedText>(text_name, static_cast<int>(base_font), chip_black ? Color::BLACK : Color::WHITE,
                                       Color::BLACK, true, chip_black ? 0.0f : name_outline);
    int font_size = tex.get_skin_config(SC::DAN_SELECT_TITLE)->font_size;
    hori_name = std::make_unique<OutlinedText>(dan_title, font_size, Color::WHITE, Color::BLACK, false);

    int revealed = 0;
    {
        // rank: 1 = played/failed, 2..7 = passes (7 = rainbow)
        auto rec = scores_manager.get_dan_record(get_player_id(global_data.player_num), dan_course_id(songs, exams));
        if (rec) {
            clear_mark = rec->rank >= 7 ? 2 : (rec->rank >= 2 ? 1 : 0);
        }
        bool any_hidden = false;
        for (const auto& e : songs) {
            any_hidden |= e.hidden;
        }
        if (any_hidden && rec && rec->rank > 0) {
            revealed = rec->arrival;
        }
    }

    const std::string& lang = global_data.config->general.language;
    int song_idx = 0;
    song_masked.clear();
    const bool have_titles = song_titles.size() == songs.size();
    for (auto& entry : songs) {
        std::string title_str, sub_str;
        if (have_titles) {
            title_str = song_titles[song_idx].first;
            sub_str = song_titles[song_idx].second;
        } else {
            SongParser sp(entry.song_path);
            title_str = localized_text(sp.metadata.title, lang, LocalizedTextFallback::ENGLISH);
            sub_str = localized_text(sp.metadata.subtitle, lang, LocalizedTextFallback::NONE);
        }
        const bool masked = entry.hidden && song_idx >= revealed;
        song_masked.push_back(masked);
        if (masked) {
            title_str = "？？？";
            sub_str.clear();
        }
        song_idx++;

        int base_sub_font = tex.get_skin_config(SC::DAN_SELECT_SUBTITLE)->font_size;
        int sub_font = base_sub_font;
        float sub_outline = 5.0f;
        if (utf8_char_count(sub_str) >= 30) {
            float shrunk = dan_shrink_font(base_sub_font);
            sub_outline = 5.0f * (shrunk / base_sub_font);
            sub_font = static_cast<int>(shrunk);
        }

        const bool vertical = !tex.options[SCO::DAN_TITLE_HORIZONTAL];
        song_texts.emplace_back(
            std::make_unique<OutlinedText>(title_str, font_size, Color::WHITE, Color::BLACK, vertical),
            std::make_unique<OutlinedText>(sub_str, sub_font, Color::WHITE, Color::BLACK, vertical, sub_outline));
    }
    text_loaded = true;
}

void DanBox::update(double current_ms) {
    BaseBox::update(current_ms);
    if (yellow_box_active && yellow_box_opened && !is_diff_select) {
        is_diff_select = true;
    }
}
