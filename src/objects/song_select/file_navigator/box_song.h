#pragma once
#include "box_base.h"

#include "../../libs/texture.h"
#include "../../libs/audio.h"
#include "../../libs/scores.h"

class SongBox : public BaseBox {
public:
    std::array<std::string, 5> hashes;
    std::array<std::optional<Score>, 5> scores;
    TJAParser parser;
    bool is_favorite;
    std::string text_subtitle;
    std::unique_ptr<OutlinedText> subtitle;
    std::unique_ptr<OutlinedText> name_black;
    bool music_playing = false;

    SongBox(const fs::path& path, const BoxDef& box_def, TJAParser tja);

    void reset() override;

    void load_text() override;
    void update(double current_time) override;
    //void draw_score_history() override;
    void enter_box() override;
    virtual void close_box() override;
    std::vector<Difficulty> get_diffs();

protected:
    FadeAnimation* diff_fade_in;
    void draw_closed() override;
    void draw_open() override;
    void draw_diff_select(bool is_ura) override;
    void draw_text();
};
