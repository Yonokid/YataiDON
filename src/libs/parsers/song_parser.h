#pragma once
#include "tja.h"
#include "osu.h"
#include <variant>

// Unified parser type that handles both .tja and .osu files.
// Exposes the same public interface as TJAParser so existing code
// can be updated by substituting SongParser for TJAParser.
class SongParser {
public:
    TJAMetadata metadata;
    TJAEXData   ex_data;
    fs::path    file_path;

    SongParser() = default;
    SongParser(const fs::path& path, int start_delay = 0, PlayerNum player_num = PlayerNum::ALL);
    void get_metadata() {}

    std::tuple<NoteList, std::deque<NoteList>, std::deque<NoteList>, std::deque<NoteList>>
    notes_to_position(int diff);

    std::string get_song_hash();
    std::string get_diff_hash(int difficulty);

private:
    std::variant<TJAParser, OsuParser> impl;
    void sync();
};
