#include "song_parser.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace {
    std::string lower_extension(const fs::path& path) {
        std::string ext = path.extension().string();
        std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return ext;
    }
} // namespace

SongParser::SongParser(const fs::path& path, int start_delay, PlayerNum player_num)
    : start_delay(start_delay), player_num(player_num) {
    // A gen 4 song is a folder of chart files rather than a single file.
    std::string ext = lower_extension(path);
#ifdef SUPPORT_FUMEN
    std::error_code dir_ec;
    const bool is_dir = fs::is_directory(path, dir_ec);
    if (dir_ec) {
        throw fs::filesystem_error("SongParser: cannot stat chart path", path, dir_ec);
    }
    if (is_dir) {
        impl = FumenParser(path, start_delay);
    } else if (ext == ".osu") {
        impl = OsuParser(path);
    } else if (ext == ".bin") {
        impl = FumenParser(path, start_delay);
    } else if (ext == ".tja") {
        impl = TJAParser(path, start_delay, static_cast<int>(player_num));
    } else {
        spdlog::warn("SongParser: unrecognized chart extension '{}' for {} -- treating as TJA", ext, path.string());
        impl = TJAParser(path, start_delay, static_cast<int>(player_num));
    }
#else
    if (ext == ".osu") {
        impl = OsuParser(path);
    } else {
        if (ext != ".tja") {
            spdlog::warn("SongParser: unrecognized chart extension '{}' for {} -- treating as TJA", ext, path.string());
        }
        impl = TJAParser(path, start_delay, static_cast<int>(player_num));
    }
#endif
    sync();
}

void SongParser::sync() {
    std::visit(
        [this](auto& p) {
            metadata = p.metadata;
            ex_data = p.ex_data;
            file_path = p.file_path;
        },
        impl);
}

std::string SongParser::get_difficulty_name() {
    return std::visit([](auto& p) { return p.get_difficulty_name(); }, impl);
}

std::tuple<NoteList, std::deque<NoteList>, std::deque<NoteList>, std::deque<NoteList>>
SongParser::notes_to_position(int diff) {
    return std::visit([diff](auto& p) { return p.notes_to_position(diff); }, impl);
}

std::string SongParser::get_song_hash() {
    return std::visit([](auto& p) { return p.get_song_hash(); }, impl);
}

std::string SongParser::get_diff_hash(int difficulty) {
    return std::visit([difficulty](auto& p) { return p.get_diff_hash(difficulty); }, impl);
}
