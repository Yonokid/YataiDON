#include "green.h"
#include "../objects/enums.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <mutex>

namespace green {

namespace {

// Chart file suffixes in difficulty order: easy, normal, hard, oni, ura.
const char* DIFF_SUFFIX[5] = { "_e", "_n", "_h", "_m", "_x" };

// "ST5100-1" -> {5, 1}, "S11100-1" -> {11, 1}. The digits before the fixed
// "100" are the game version; the digit after the dash orders revisions.
bool parse_config_version(const std::string& name, int& version, int& revision) {
    size_t i = 0;
    while (i < name.size() && !isdigit((unsigned char)name[i])) i++;
    size_t hundred = name.find("100-", i);
    if (i == 0 || hundred == std::string::npos || hundred == i) return false;
    try {
        version  = std::stoi(name.substr(i, hundred - i));
        revision = std::stoi(name.substr(hundred + 4));
    } catch (...) { return false; }
    return true;
}

// The folder holding the newest song list. The later games keep one list per
// game version under config/ - every version lists the full library up to
// itself, so only the newest is worth reading - while the earlier ones keep a
// single musicinfo.xml at the top of the data folder.
fs::path newest_config(const fs::path& data_root) {
    fs::path best;
    int best_ver = -1;
    uintmax_t best_size = 0;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(data_root / "config", ec)) {
        if (!entry.is_directory(ec)) continue;
        int ver = 0, rev = 0;
        if (!parse_config_version(entry.path().filename().string(), ver, rev)) continue;
        uintmax_t size = fs::file_size(entry.path() / "musicinfo.xml", ec);
        if (ec) { ec.clear(); continue; }
        // The digit after the dash is a variant, not a revision - the "-1"
        // list can be the larger one - so within the newest version the
        // fuller song list wins.
        if (ver > best_ver || (ver == best_ver && size > best_size)) {
            best_ver  = ver;
            best_size = size;
            best = entry.path();
        }
    }
    if (best.empty() && fs::exists(data_root / "musicinfo.xml", ec))
        best = data_root;
    return best;
}

// The game's own name, from the PARAM.SFO two levels above the data folder
// (<game>/USRDIR/data). The file is a little-endian key table; TITLE is the
// name the game declares for itself, version code and all.
std::string sfo_title(const fs::path& data_root) {
    std::ifstream f(data_root.parent_path().parent_path() / "PARAM.SFO", std::ios::binary);
    if (!f) return "";
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
    auto le32 = [&](size_t o) -> uint32_t {
        if (o + 4 > d.size()) return 0;
        return (uint32_t)d[o] | ((uint32_t)d[o+1] << 8) |
               ((uint32_t)d[o+2] << 16) | ((uint32_t)d[o+3] << 24);
    };
    auto le16 = [&](size_t o) -> uint16_t {
        if (o + 2 > d.size()) return 0;
        return (uint16_t)(d[o] | (d[o+1] << 8));
    };
    if (d.size() < 20 || memcmp(d.data(), "\0PSF", 4) != 0) return "";
    uint32_t keys = le32(8), values = le32(12), count = le32(16);
    for (uint32_t i = 0; i < count; i++) {
        size_t e = 20 + (size_t)i * 16;
        size_t k = keys + le16(e);
        if (k >= d.size()) break;
        size_t kend = k;
        while (kend < d.size() && d[kend]) kend++;
        if (std::string(d.begin() + k, d.begin() + kend) != "TITLE") continue;
        size_t v = values + le32(e + 12), vmax = v + le32(e + 4);
        size_t vend = v;
        while (vend < d.size() && vend < vmax && d[vend]) vend++;
        return std::string(d.begin() + v, d.begin() + vend);
    }
    return "";
}

// One tag's text content, from pos onward and before end. The XML is a
// machine-written boost serialization, so plain string search holds up.
std::string tag_text(const std::string& xml, const char* tag, size_t pos, size_t end) {
    std::string open = std::string("<") + tag + ">";
    size_t a = xml.find(open, pos);
    if (a == std::string::npos || a >= end) return "";
    a += open.size();
    size_t b = xml.find('<', a);
    if (b == std::string::npos || b > end) return "";
    return xml.substr(a, b - a);
}

}  // namespace

bool Library::load(const fs::path& root) {
    is_loaded = false;
    entries.clear();
    data_root = root;

    fs::path config = newest_config(root);
    if (config.empty()) {
        spdlog::warn("green: no config folder with a musicinfo.xml under {}", root.string());
        return false;
    }

    std::ifstream f(config / "musicinfo.xml", std::ios::binary);
    if (!f) {
        spdlog::warn("green: cannot open {}", (config / "musicinfo.xml").string());
        return false;
    }
    std::string xml((std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());

    size_t pos = 0;
    while ((pos = xml.find("<musicid>", pos)) != std::string::npos) {
        size_t end = xml.find("<musicid>", pos + 1);
        if (end == std::string::npos) end = xml.size();

        SongEntry e;
        e.id     = tag_text(xml, "musicid", pos, end);
        e.title  = tag_text(xml, "musicname", pos, end);
        e.genre  = tag_text(xml, "genrename", pos, end);
        e.secret = tag_text(xml, "secret", pos, end) == "1";
        try { e.unique_id = std::stoi(tag_text(xml, "uniqueid", pos, end)); } catch (...) {}
        if (!e.id.empty()) entries.push_back(std::move(e));

        pos = end;
    }

    load_tuning(root / "fumen" / "tuning.bin");

    // The game says what it is called; the folder name is the last resort.
    name = sfo_title(root);
    if (name.empty()) {
        fs::path above = root.parent_path().parent_path();
        name = above.empty() ? root.filename().string() : above.filename().string();
    }

    is_loaded = !entries.empty();
    if (is_loaded)
        spdlog::info("green: {} songs from {} [{}] ({})", entries.size(), root.string(),
                     name, config == root ? "musicinfo.xml" : config.filename().string());
    return is_loaded;
}

// fumen/tuning.bin carries per-chart parameters, star ratings among them:
// a big-endian count, then 2316-byte records of 579 ints - three string-pool
// offsets (musicid first) and 2 players x 9 charts x 32 ints, where the
// slots run easy, normal, hard, oni, ura and int 1 of a chart is its stars.
// (Format established by TaikoGen3SongManager.)
void Library::load_tuning(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        spdlog::warn("green: no tuning.bin, songs will show no star ratings");
        return;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    auto be32 = [&](size_t off) -> int32_t {
        if (off + 4 > data.size()) return -1;
        return (int32_t)((data[off] << 24) | (data[off+1] << 16) |
                         (data[off+2] << 8) | data[off+3]);
    };

    constexpr size_t RECORD_SIZE = 2316;
    int32_t count = be32(0);
    if (count <= 0 || 4 + (size_t)count * RECORD_SIZE > data.size()) {
        spdlog::warn("green: tuning.bin does not look like itself, ignoring it");
        return;
    }
    size_t pool = 4 + (size_t)count * RECORD_SIZE;

    auto pool_string = [&](int32_t off) -> std::string {
        if (off < 0 || pool + (size_t)off >= data.size()) return "";
        size_t a = pool + (size_t)off, b = a;
        while (b < data.size() && data[b] != 0) b++;
        return std::string(data.begin() + a, data.begin() + b);
    };

    std::map<std::string, SongEntry*> by_id;
    for (SongEntry& e : entries) by_id[e.id] = &e;

    for (int32_t r = 0; r < count; r++) {
        size_t base = 4 + (size_t)r * RECORD_SIZE;
        std::string id = pool_string(be32(base));

        // An ura chart is its own record, "ex_" + the song id, and its stars
        // sit in that record's oni slot; the song's own record leaves the ura
        // slot empty.
        if (id.rfind("ex_", 0) == 0) {
            auto it = by_id.find(id.substr(3));
            if (it == by_id.end()) continue;
            int32_t stars = be32(base + (3 + 3 * 32 + 1) * 4);
            if (stars > 0 && stars <= 10) it->second->stars[4] = (int)stars;
            if (be32(base + (3 + 3 * 32) * 4) >= 0) it->second->has_chart[4] = true;
            continue;
        }

        auto it = by_id.find(id);
        if (it == by_id.end()) continue;
        // Player 1's five difficulty slots; ints 3.. are the charts, 32 ints
        // each. Int 0 is the chart's name offset (negative when the slot is
        // empty) and int 1 its star rating.
        it->second->charts_known = true;
        for (int d = 0; d < 5; d++) {
            int32_t stars = be32(base + (3 + d * 32 + 1) * 4);
            if (stars > 0 && stars <= 10) it->second->stars[d] = (int)stars;
            if (d < 4 && be32(base + (3 + d * 32) * 4) >= 0)
                it->second->has_chart[d] = true;
        }
    }
}

const SongEntry* Library::find(const std::string& id) const {
    for (const SongEntry& e : entries)
        if (e.id == id) return &e;
    return nullptr;
}

fs::path Library::chart_path(const std::string& id, int difficulty) const {
    if (difficulty < 0 || difficulty > 4) return {};

    // Ura moved home between games: the earlier ones keep it as its own
    // "ex_<id>" song folder with an oni-slot file, the later ones as the
    // song's own "_x" chart. Whichever exists is the one.
    if (difficulty == 4) {
        fs::path x = data_root / "fumen" / id / "solo" / (id + "_x.bin");
        std::error_code ec;
        if (fs::exists(x, ec)) return x;
        fs::path ex = data_root / "fumen" / ("ex_" + id) / "solo" / ("ex_" + id + "_m.bin");
        if (fs::exists(ex, ec)) return ex;
        return x;
    }

    return data_root / "fumen" / id / "solo" / (id + DIFF_SUFFIX[difficulty] + ".bin");
}

bool Library::has_difficulty(const std::string& id, int difficulty) const {
    if (difficulty < 0 || difficulty > 4) return false;
    // The tables already say which charts a song ships; only a song the
    // tuning file does not know needs the disk asked.
    if (const SongEntry* e = find(id); e && e->charts_known)
        return e->has_chart[difficulty];
    std::error_code ec;
    fs::path p = chart_path(id, difficulty);
    return !p.empty() && fs::exists(p, ec);
}

fs::path Library::sound_path(const std::string& id) const {
    std::string upper = id;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    // Just the name: the preview path checks existence when it matters, and
    // checking here again would be one more disk lookup per listed song.
    return data_root / "sound" / "bgm" / "nub" / ("SONG_" + upper + ".nub");
}

std::vector<std::string> genres_present(const Library& library) {
    // The game's own genre order, as its song select shows them. メドレー is
    // left out: those entries are the medley-mode assemblies, not songs.
    static const char* ORDER[] = {
        "J-POP", "アニメ", "ボーカロイド", "童謡", "バラエティ",
        "クラシック", "ゲームミュージック", "ナムコオリジナル",
    };
    std::vector<std::string> found;
    for (const char* genre : ORDER) {
        for (const SongEntry& e : library.songs()) {
            if (e.genre == genre) { found.push_back(genre); break; }
        }
    }
    // Anything the fixed order does not know about still gets listed.
    for (const SongEntry& e : library.songs()) {
        if (e.genre == "メドレー") continue;
        if (std::find(found.begin(), found.end(), e.genre) == found.end())
            found.push_back(e.genre);
    }
    return found;
}

int genre_index_for(const std::string& genre) {
    if (genre == "J-POP")            return (int)GenreIndex::JPOP;
    if (genre == "アニメ")           return (int)GenreIndex::ANIME;
    if (genre == "ボーカロイド")     return (int)GenreIndex::VOCALOID;
    if (genre == "童謡")             return (int)GenreIndex::CHILDREN;
    if (genre == "バラエティ")       return (int)GenreIndex::VARIETY;
    if (genre == "メドレー")         return (int)GenreIndex::VARIETY;
    if (genre == "クラシック")       return (int)GenreIndex::CLASSICAL;
    if (genre == "ゲームミュージック") return (int)GenreIndex::GAME;
    if (genre == "ナムコオリジナル") return (int)GenreIndex::NAMCO;
    return (int)GenreIndex::DEFAULT;
}

// A genre inside a green root is spelled as a folder that does not exist:
// <root>/@genre@<name>. The marker cannot collide with a real folder, since
// @ never starts one in these dumps.
fs::path genre_path(const fs::path& data_root, const std::string& genre) {
    return data_root / ("@genre@" + genre);
}

std::string genre_of_path(const fs::path& path) {
    std::string name = path.filename().string();
    if (name.rfind("@genre@", 0) != 0) return "";
    return name.substr(7);
}

// Whether one directory is a data root, remembered: the walk below runs for
// every directory the song scan meets, and the same ancestors come up over
// and over, each check costing real disk time.
static bool is_root_dir(const fs::path& p) {
    static std::mutex              mutex;
    static std::map<fs::path, bool> cache;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = cache.find(p);
        if (it != cache.end()) return it->second;
    }
    std::error_code ec;
    bool ok = fs::is_directory(p / "fumen", ec) &&
              !newest_config(p).empty();
    std::lock_guard<std::mutex> lock(mutex);
    cache[p] = ok;
    return ok;
}

fs::path find_data_root(const fs::path& path) {
    for (fs::path p = path; !p.empty() && p != p.root_path(); p = p.parent_path()) {
        if (is_root_dir(p)) return p;
        if (p.parent_path() == p) break;
    }
    return {};
}

const Library* library_for(const fs::path& path) {
    fs::path root = find_data_root(path);
    if (root.empty()) return nullptr;

    static std::mutex           mutex;
    static std::map<fs::path, Library> libraries;
    std::lock_guard<std::mutex> lock(mutex);
    auto it = libraries.find(root);
    if (it == libraries.end()) {
        Library lib;
        lib.load(root);
        it = libraries.emplace(root, std::move(lib)).first;
    }
    return it->second.loaded() ? &it->second : nullptr;
}

}  // namespace green
