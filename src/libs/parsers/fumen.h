#pragma once
#include "tja.h"

class FumenParser {
public:
    fs::path   file_path;
    TJAMetadata metadata;
    TJAEXData   ex_data;

    FumenParser() = default;
    explicit FumenParser(const fs::path& path);
    void get_metadata() {}
    std::string get_difficulty_name() { return ""; }

    std::tuple<NoteList, std::deque<NoteList>, std::deque<NoteList>, std::deque<NoteList>>
    notes_to_position(int diff);

    std::string get_song_hash();
    std::string get_diff_hash(int difficulty);

private:
    bool     parsed = false;
    NoteList cached_notes;

    void build_notes();
};
