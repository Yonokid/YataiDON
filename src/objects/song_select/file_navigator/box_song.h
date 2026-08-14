#pragma once

#include "box_base.h"
#include "score_history.h"
#include "../../../libs/song_parser.h"
#include "../../../libs/audio.h"
#include <atomic>
#include <cmath>

class SongBox : public BaseBox {
public:
    std::array<std::string, 5> hashes;
    std::array<std::optional<Score>, 5> scores;
    SongParser parser;
    bool is_favorite;
    std::string text_subtitle;
    std::unique_ptr<OutlinedText> subtitle;
    std::unique_ptr<OutlinedText> name_black;
    std::unique_ptr<OutlinedText> bpm_text;
    std::optional<ray::Texture2D> preimage;
    bool music_playing = false;
    // Decoding a .nus3bank preview takes over a second, so it runs on its own
    // thread; the box polls this in update and starts the stream when it is
    // ready. Dropping the pointer abandons a load whose result is not wanted
    // any more - the worker holds its own reference and finishes harmlessly.
    struct PreviewLoad {
        std::atomic<bool>       done{false};
        bool                    ok = false;
        AudioEngine::PreparedPCM pcm;
    };
    std::shared_ptr<PreviewLoad> preview_load;
    // One decode per opening: a failed bank would otherwise be retried every
    // frame for as long as the cursor sits on it.
    bool preview_attempted = false;
    std::unique_ptr<ScoreHistory> score_history;
    double box_opened_at = 0.0;
    FadeAnimation* diff_fade_in;
    bool is_ura = false;
    // The genre the song itself belongs to. Same as genre_index for a song
    // sitting in its genre folder, but inside a collection the box keeps the
    // collection's genre (its colours and background are the collection's)
    // while the game screen still needs the song's own genre for its label.
    GenreIndex song_genre_index = GenreIndex::DEFAULT;

    SongBox(const fs::path& path, const BoxDef& box_def, SongParser parser);

    void reset() override;

    void load_text() override;
    void update(double current_time) override;
    void draw_score_history() override;
    void expand_box() override;
    void enter_box() override;
    virtual void close_box() override;
    std::vector<Difficulty> get_diffs();

    void refresh_scores();
    // The chart hash for one difficulty, computed on first use. Arcade charts
    // skip hashing when the box is built (it means parsing the whole chart),
    // so the moment a song is actually picked the hash is filled in here and
    // recorded, and every later lookup finds it stored.
    std::string hash_for(int difficulty);

    const char* lua_kind() const override { return "song"; }
    OutlinedText* horizontal_subtitle() {
        if (!horizontal_subtitle_cache) {
            float font_size = utf8_char_count(text_subtitle) < 30
                ? tex.skin_config[SC::YB_SUBTITLE].font_size
                : tex.skin_config[SC::YB_SUBTITLE].font_size - (int)(10 * tex.screen_scale);
            horizontal_subtitle_cache = std::make_unique<OutlinedText>(text_subtitle, font_size, ray::WHITE, fore_color.value(), false);
        }
        return horizontal_subtitle_cache.get();
    }
    OutlinedText* horizontal_subtitle_large() {
        if (!horizontal_subtitle_large_cache) {
            float font_size = utf8_char_count(text_subtitle) < 30
                ? tex.skin_config[SC::YB_SUBTITLE].font_size
                : tex.skin_config[SC::YB_SUBTITLE].font_size - (int)(10 * tex.screen_scale);
            horizontal_subtitle_large_cache = std::make_unique<OutlinedText>(text_subtitle, (int)(font_size * 1.3f), ray::WHITE, fore_color.value(), false);
        }
        return horizontal_subtitle_large_cache.get();
    }
    bool has_ura() const { return parser.metadata.course_data.count((int)Difficulty::URA) > 0; }
    int ex_data_flag() const {
        if (parser.ex_data.new_audio) return 1;
        if (parser.ex_data.old_audio) return 2;
        if (parser.ex_data.limited_time) return 3;
        if (is_new) return 4;
        return 0;
    }
    struct CourseInfo { bool has_course; int level; bool is_branching; int crown; int rank; };
    CourseInfo course_info(int diff) const {
        auto it = parser.metadata.course_data.find(diff);
        bool has_course = it != parser.metadata.course_data.end();
        CourseInfo info{has_course, 0, false, (int)Crown::NONE, (int)Rank::_NONE};
        if (has_course) {
            info.level = (int)std::round(it->second.level);
            info.is_branching = it->second.is_branching;
        }
        if (diff >= 0 && diff < (int)scores.size() && scores[diff].has_value()) {
            info.crown = (int)scores[diff]->crown;
            info.rank  = (int)scores[diff]->rank;
        }
        return info;
    }

protected:
    std::unique_ptr<OutlinedText> horizontal_subtitle_cache;
    std::unique_ptr<OutlinedText> horizontal_subtitle_large_cache;

    void draw_closed() override;
    void draw_open() override;
    void draw_diff_select() override;
    void draw_text();
};
