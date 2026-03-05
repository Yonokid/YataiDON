#pragma once
#include "box_base.h"

#include "../../libs/texture.h"

class SongBox : public BaseBox {
public:
    std::map<int, ScoreRow*> scores;
    std::map<int, std::string> hash;
    TJAParser parser;
    bool is_favorite;
    std::string text_subtitle;
    std::unique_ptr<OutlinedText> subtitle;
    std::unique_ptr<OutlinedText> name_black;

    SongBox(const fs::path& path,
            const std::optional<ray::Color>& back_color,
            const std::optional<ray::Color>& fore_color,
            TextureIndex texture_index,
            TJAParser tja);

    void load_text() override;
    //void get_scores() override;
    void update(double current_time) override;
    //void draw_score_history() override;

protected:
    void draw_closed(float outer_fade_override) override;
    void draw_open(std::optional<float> fade_override) override;
    void draw_diff_select(std::optional<float> fade_override) override;
    void draw_text();
};
