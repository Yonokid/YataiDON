#include "../../../libs/localized_text.h"
#include "box_dan.h"
#include "../../../libs/song_parser.h"
#include "../../../libs/scores.h"
#include <algorithm>
#include <string>

DanBox::DanBox(const fs::path& path, const std::string& title, int color,
               const std::vector<DanSongEntry>& songs_in,
               const std::vector<Exam>& exams_in, int total_notes_in)
    : BaseBox(path, BoxDef{title, static_cast<TextureIndex>(color), GenreIndex::DAN, "", "", std::nullopt, std::nullopt})
    , dan_title(title), dan_color(color)
    , songs(songs_in), exams(exams_in), total_notes(total_notes_in)
{
    text_name = title;
}

static float dan_shrink_font(float font_size) {
    return std::max(font_size - 10.0f * tex.screen_scale, font_size * 0.6f);
}

void DanBox::load_text() {
    BaseBox::load_text();  // populates name (vertical) for closed state
    const SkinInfo* chip_slot = tex.skin_entry("dan_chip_name");
    float base_font = (chip_slot && chip_slot->font_size > 0)
                    ? (float)chip_slot->font_size
                    : (float)tex.skin_config[SC::SONG_BOX_NAME].font_size;
    float name_outline = 5.0f;
    if (utf8_char_count(text_name) >= 30) {
        float shrunk = dan_shrink_font(base_font);
        name_outline = 5.0f * (shrunk / base_font);
        base_font = shrunk;
    }
    const bool chip_black = tex.options[SCO::DAN_CHIP_NAME_BLACK];
    name = std::make_unique<OutlinedText>(text_name, (int)base_font,
                                          chip_black ? ray::BLACK : ray::WHITE,
                                          ray::BLACK, true,
                                          chip_black ? 0.0f : name_outline);
    int font_size = tex.skin_config[SC::DAN_TITLE].font_size;
    hori_name = std::make_unique<OutlinedText>(dan_title, font_size, ray::WHITE, ray::BLACK, false);

    int revealed = 0;
    {
        bool any_hidden = false;
        for (const auto& e : songs) any_hidden |= e.hidden;
        if (any_hidden) {
            auto rec = scores_manager.get_dan_record(
                get_player_id(global_data.player_num), dan_title);
            if (rec && rec->rank > 0) revealed = rec->arrival;
        }
    }

    const std::string& lang = global_data.config->general.language;
    int song_idx = 0;
    const bool have_titles = song_titles.size() == songs.size();
    for (auto& entry : songs) {
        std::string title_str, sub_str;
        if (have_titles) {
            title_str = song_titles[song_idx].first;
            sub_str   = song_titles[song_idx].second;
        } else {
            SongParser sp(entry.song_path);
            title_str = localized_text(sp.metadata.title, lang, LocalizedTextFallback::ENGLISH);
            sub_str   = localized_text(sp.metadata.subtitle, lang, LocalizedTextFallback::NONE);
        }
        if (entry.hidden && song_idx >= revealed) {
            title_str = "？？？";
            sub_str.clear();
        }
        song_idx++;

        int base_sub_font = tex.skin_config[SC::DAN_SUBTITLE].font_size;
        int sub_font = base_sub_font;
        float sub_outline = 5.0f;
        if (utf8_char_count(sub_str) >= 30) {
            float shrunk = dan_shrink_font((float)base_sub_font);
            sub_outline = 5.0f * (shrunk / (float)base_sub_font);
            sub_font = (int)shrunk;
        }

        const bool vertical = !tex.options[SCO::DAN_TITLE_HORIZONTAL];
        song_texts.push_back({
            std::make_unique<OutlinedText>(title_str, font_size, ray::WHITE, ray::BLACK, vertical),
            std::make_unique<OutlinedText>(sub_str,   sub_font,  ray::WHITE, ray::BLACK, vertical, sub_outline)
        });
    }
    text_loaded = true;
}

void DanBox::update(double current_ms) {
    BaseBox::update(current_ms);
    if (yellow_box_active && yellow_box_opened && !is_diff_select)
        is_diff_select = true;
}
