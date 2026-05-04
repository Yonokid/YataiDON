#include "song_parser.h"

SongParser::SongParser(const fs::path& path, int start_delay, PlayerNum player_num) {
    if (path.extension() == ".osu")
        impl = OsuParser(path);
    else
        impl = TJAParser(path, start_delay, player_num);
    sync();
}

void SongParser::sync() {
    std::visit([this](auto& p) {
        metadata  = p.metadata;
        ex_data   = p.ex_data;
        file_path = p.file_path;
    }, impl);
}

std::string SongParser::get_difficulty_name() {
    return std::visit([](auto& p) { return p.get_difficulty_name(); }, impl);
}

std::tuple<NoteList, std::deque<NoteList>, std::deque<NoteList>, std::deque<NoteList>>
SongParser::notes_to_position(int diff) {
    return std::visit([diff](auto& p) {
        return p.notes_to_position(diff);
    }, impl);
}

std::string SongParser::get_song_hash() {
    return std::visit([](auto& p) { return p.get_song_hash(); }, impl);
}

std::string SongParser::get_diff_hash(int difficulty) {
    return std::visit([difficulty](auto& p) { return p.get_diff_hash(difficulty); }, impl);
}
