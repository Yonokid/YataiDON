#pragma once

#include "box_base.h"
#include "score_history.h"
#include "../../../libs/parsers/song_parser.h"

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
    std::unique_ptr<ScoreHistory> score_history;
    double box_opened_at = 0.0;

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

protected:
    FadeAnimation* diff_fade_in;
    void draw_closed() override;
    void draw_open() override;
    void draw_diff_select(bool is_ura) override;
    void draw_text();
};
