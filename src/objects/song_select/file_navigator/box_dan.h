#pragma once

#include "../../../libs/global_data.h"
#include "box_base.h"

class DanBox : public BaseBox {
public:
    std::string dan_title;
    int dan_color = 0;
    std::vector<bool> song_masked; // hidden songs not yet reached
    int clear_mark = 0;            // 0 none, 1 passed, 2 perfect (from the saved dan record)
    int dan_rank = -1;             // -1 = unearned/not yet ranked
    int dan_index = -1;            // -1 = unselected
    bool gaiden = false;
    bool gen3 = false;
    bool is_back = false; // the "back" folder: no course, leaves to song select
    std::vector<DanSongEntry> songs;
    std::vector<Exam> exams;
    int total_notes = 0;
    std::vector<std::pair<std::string, std::string>> song_titles;

    std::unique_ptr<OutlinedText> hori_name;
    std::vector<std::pair<std::unique_ptr<OutlinedText>, std::unique_ptr<OutlinedText>>> song_texts;

    DanBox(const fs::path& path, const std::string& title, int color, const std::vector<DanSongEntry>& songs,
           const std::vector<Exam>& exams, int total_notes);

    void load_text() override;
    void update(double current_ms) override;
};
