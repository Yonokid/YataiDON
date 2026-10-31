#pragma once

#include <array>

#include "tja.h"

class OsuParser {
public:
    fs::path file_path;
    TJAMetadata metadata;
    TJAEXData ex_data;
    std::string difficulty_name;

    OsuParser() = default;
    explicit OsuParser(const fs::path& path);
    void get_metadata() {}
    std::string get_difficulty_name() { return difficulty_name; }

    std::tuple<NoteList, std::deque<NoteList>, std::deque<NoteList>, std::deque<NoteList>> notes_to_position(int diff);

    std::string get_song_hash();
    std::string get_diff_hash(int difficulty);

private:
    double slider_multiplier = 1.4;
    // Each timing point: {time_ms, beat_length, meter, effects}
    std::vector<std::array<double, 4>> timing_points;
    // Each hit object: comma-separated fields, kept as strings (positional)
    std::vector<std::vector<std::string>> hit_objects_data;

    NoteList cached_notes;
    bool notes_built = false;

    static std::vector<std::string> read_file_lines(const fs::path& path);
    std::map<std::string, std::string> read_section_dict(const std::vector<std::string>& lines,
                                                         const std::string& section) const;
    std::vector<std::vector<double>> read_section_list(const std::vector<std::string>& lines,
                                                       const std::string& section) const;
    std::vector<std::vector<std::string>> read_hitobjects_fields(const std::vector<std::string>& lines) const;
    double get_scroll_multiplier(double ms) const;
    double get_bpm_at(double ms) const;
    NoteList& get_notes();
};
