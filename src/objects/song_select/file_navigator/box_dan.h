#pragma once

#include "box_base.h"
#include "../../../libs/global_data.h"
#include "../../game/exam_caption.h"

class DanBox : public BaseBox {
public:
    std::string dan_title;
    int dan_color = 0;
    int dan_rank = -1;
    int dan_index = -1;
    bool gaiden = false;
    std::vector<DanSongEntry> songs;
    std::vector<Exam> exams;
    int total_notes = 0;
    std::vector<std::pair<std::string, std::string>> song_titles;

    ExamCaptionCache exam_captions;

    std::unique_ptr<OutlinedText> hori_name;
    std::vector<std::pair<std::unique_ptr<OutlinedText>, std::unique_ptr<OutlinedText>>> song_texts;

    DanBox(const fs::path& path, const std::string& title, int color,
           const std::vector<DanSongEntry>& songs, const std::vector<Exam>& exams,
           int total_notes);

    void load_text() override;
    void update(double current_ms) override;

protected:
    void draw_chip();
    void draw_closed() override;
    void draw_open() override;
    void draw_diff_select() override { draw_open(); }

private:
    void draw_exam_box();
    void draw_exam_grid();
    void draw_digit_counter(const std::string& digits, float margin_x, float y, TexID digit_tex);
};
