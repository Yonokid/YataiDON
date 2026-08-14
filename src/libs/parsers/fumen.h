#pragma once
#include "tja.h"
#include "../gen4.h"

// The chart format of the gen 4 arcade games. A song is a folder of up to five
// chart files, one per difficulty, and everything else about it -- title, star
// ratings, which difficulties exist -- comes from the game's datatable rather
// than from the charts themselves.
//
// A path to a single chart file also works, for looking at one chart on its
// own; there is no metadata to be had that way.
class FumenParser {
public:
    fs::path   file_path;
    TJAMetadata metadata;
    TJAEXData   ex_data;

    FumenParser() = default;
    explicit FumenParser(const fs::path& path, int start_delay = 0);
    void get_metadata() {}
    std::string get_difficulty_name() { return ""; }

    std::tuple<NoteList, std::deque<NoteList>, std::deque<NoteList>, std::deque<NoteList>>
    notes_to_position(int diff);

    std::string get_song_hash();
    std::string get_diff_hash(int difficulty);

    // True when the path was a song folder that the datatable knows about.
    bool is_library_song() const { return library != nullptr; }

private:
    const gen4::Library* library = nullptr;
    std::string          song_id;
    // The lead-in the game plays before the chart starts. TJA charts carry it
    // in their note times, and the engine starts the music by it, so a chart
    // without it runs a second ahead of its music.
    double               start_delay = 0.0;

    int      cached_diff = -1;
    NoteList cached_notes;

    void build_notes(int diff);
    std::vector<uint8_t> read_chart(int diff);
};
