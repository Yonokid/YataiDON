#pragma once

// Reading the data of the PS3 arcade games (System 357, "Green" era). Unlike
// the gen 4 games nothing here is encrypted: the song list is a plain XML
// serialization, the charts are the same fumen structures written big-endian,
// and the audio is ATRAC3+ inside .nub containers.

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace green {

// One song as musicinfo.xml lists it. The XML carries no star ratings; those
// come from fumen/tuning.bin, matched up by the song id.
struct SongEntry {
    std::string id;          // "10jiku", also the fumen folder name
    int         unique_id = 0;
    std::string title;       // Japanese, the only language the file has
    std::string genre;       // the genre's own name, e.g. "ナムコオリジナル"
    bool        secret = false;
    int         stars[5] = {};   // easy..ura, 0 when tuning.bin has no entry
    // Which difficulties ship a chart, from tuning.bin's active slots - the
    // wheel asks this for every song of a genre, and asking the disk instead
    // is thousands of lookups on what may be a slow mount.
    bool        has_chart[5] = {};
    bool        charts_known = false;   // false when tuning.bin was absent
};

// The library of one game data root, ie. the USRDIR/data folder holding
// fumen/, sound/ and config/. The config folder holds one subfolder per game
// version (ST5100-1 ... S11100-1) and only the newest is read: it lists every
// song the older ones did.
class Library {
public:
    bool load(const fs::path& data_root);

    bool loaded() const { return is_loaded; }
    const fs::path& root() const { return data_root; }

    // What the game calls itself, from its PARAM.SFO - "Taiko no Tatsujin
    // (ST91)" and the like - falling back to the folder name above USRDIR.
    const std::string& game_name() const { return name; }

    const SongEntry* find(const std::string& id) const;
    const std::vector<SongEntry>& songs() const { return entries; }

    // The chart of one difficulty (0..4 = easy..ura), or an empty path.
    fs::path chart_path(const std::string& id, int difficulty) const;
    bool has_difficulty(const std::string& id, int difficulty) const;

    // The song's .nub audio bank, or an empty path when the sound is missing.
    fs::path sound_path(const std::string& id) const;

private:
    bool                   is_loaded = false;
    fs::path               data_root;
    std::string            name;
    std::vector<SongEntry> entries;

    void load_tuning(const fs::path& path);
};

// The genres that actually have songs, in a fixed sensible order.
std::vector<std::string> genres_present(const Library& library);

// GenreIndex value for one of the game's genre names.
int genre_index_for(const std::string& genre);

// The pseudo-path used for a genre inside a data root, and its inverse.
fs::path    genre_path(const fs::path& data_root, const std::string& genre);
std::string genre_of_path(const fs::path& path);

// The library covering a path, loading it the first time it is asked for.
// Returns null when the path is not inside a green data root.
const Library* library_for(const fs::path& path);

// The data root a path belongs to, or an empty path. A root is recognised by
// holding a fumen/ folder next to config/ with at least one musicinfo.xml.
fs::path find_data_root(const fs::path& path);

}  // namespace green
