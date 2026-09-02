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
    std::array<std::optional<Score>, 5> scores_p2;
    SongParser parser;
    bool is_favorite;
    std::string text_subtitle;
    std::unique_ptr<OutlinedText> subtitle;
    std::unique_ptr<OutlinedText> name_black;
    std::unique_ptr<OutlinedText> bpm_text;
    std::optional<ray::Texture2D> preimage;
    bool music_playing = false;
    struct PreviewLoad {
        std::atomic<bool>       done{false};
        bool                    ok = false;
        AudioEngine::PreparedPCM pcm;
    };
    std::shared_ptr<PreviewLoad> preview_load;
    bool preview_attempted = false;
    std::unique_ptr<ScoreHistory> score_history;
    double box_opened_at = 0.0;
    FadeAnimation* diff_fade_in;
    bool is_ura = false;
    GenreIndex song_genre_index = GenreIndex::DEFAULT;

    SongBox(const fs::path& path, const BoxDef& box_def, SongParser parser);
    ~SongBox() override { release_preview_slot(); }

    static void service_bgm_resume(double current_ms);
    static void reset_bgm_slot();
    void release_preview_slot();
    bool holds_preview_slot = false;

    void reset() override;

    void load_text() override;
    void update(double current_time) override;
    void draw_score_history() override;
    void expand_box() override;
    void enter_box() override;
    virtual void close_box() override;
    std::vector<Difficulty> get_diffs();

    void refresh_scores();
    std::string hash_for(int difficulty);

    const char* lua_kind() const override { return "song"; }
    OutlinedText* horizontal_subtitle() {
        if (!horizontal_subtitle_cache) {
            float font_size = utf8_char_count(text_subtitle) < 30
                ? tex.skin_config[SC::YB_SUBTITLE].font_size
                : tex.skin_config[SC::YB_SUBTITLE].font_size - (int)(10 * tex.screen_scale);
            horizontal_subtitle_cache = std::make_unique<OutlinedText>(text_subtitle, font_size, text_color, fore_color.value(), false);
        }
        return horizontal_subtitle_cache.get();
    }
    OutlinedText* horizontal_subtitle_large() {
        if (!horizontal_subtitle_large_cache) {
            float font_size = utf8_char_count(text_subtitle) < 30
                ? tex.skin_config[SC::YB_SUBTITLE].font_size
                : tex.skin_config[SC::YB_SUBTITLE].font_size - (int)(10 * tex.screen_scale);
            horizontal_subtitle_large_cache = std::make_unique<OutlinedText>(text_subtitle, (int)(font_size * 1.3f), text_color, fore_color.value(), false);
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
    void draw_box_crown(float x, float y, double fade_val);
    void draw_diff_crown(int diff, float x, float y, double fade_val);
    void draw_diff_outline(float x, float y, double fade_val);
};
