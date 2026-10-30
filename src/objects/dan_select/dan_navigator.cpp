#include "dan_navigator.h"

#include <algorithm>
#include <climits>
#include <tuple>

#include "../../libs/gfx/text.h"
#ifdef SUPPORT_FUMEN
#include "../../libs/optional/gen3.h"
#include "../../libs/optional/gen4.h"
#endif
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iterator>

#include "../../libs/file/filesystem.h"
#include "../../libs/file/scores.h"
#include "../../libs/file/script.h"
#include "../../libs/input/input.h"
#include "../../libs/parsers/song_parser.h"
#include "../../libs/song_library.h"
#include "../song_select/file_navigator/navigator.h"

int DanNavigator::total_notes_for(const std::vector<DanSongEntry>& songs) {
    int total = 0;
    for (const auto& entry : songs) {
        total += song_note_count(entry.song_path, entry.difficulty);
    }
    return total;
}

std::shared_ptr<const TJAMetadata> DanNavigator::song_metadata(const fs::path& path) {
    if (memo_) {
        std::scoped_lock lock(memo_->mutex);
        if (auto it = memo_->metadata.find(path); it != memo_->metadata.end()) {
            return it->second;
        }
    }
    auto meta = std::make_shared<const TJAMetadata>(SongParser(path).metadata);
    if (memo_) {
        std::scoped_lock lock(memo_->mutex);
        memo_->metadata.emplace(path, meta);
    }
    return meta;
}

int DanNavigator::song_note_count(const fs::path& path, int difficulty) {
    if (memo_) {
        std::scoped_lock lock(memo_->mutex);
        if (auto it = memo_->note_counts.find({path, difficulty}); it != memo_->note_counts.end()) {
            return it->second;
        }
    }
    int total = 0;
    try {
        SongParser sp(path);
        auto [notes, bm, be, bn] = sp.notes_to_position(difficulty);
        for (const Note& n : notes.notes) {
            if (n.type >= NoteType::DON && n.type <= NoteType::KAT_L) {
                total++;
            }
        }
        for (auto& sec : bm) {
            for (const Note& n : sec.notes) {
                if (n.type >= NoteType::DON && n.type <= NoteType::KAT_L) {
                    total++;
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("DanNavigator::total_notes_for: failed to parse '{}': {}", path.string(), e.what());
    }
    if (memo_) {
        std::scoped_lock lock(memo_->mutex);
        memo_->note_counts.emplace(std::make_pair(path, difficulty), total);
    }
    return total;
}

int DanNavigator::box_genre(const fs::path& box_def_dir) {
    if (memo_) {
        std::scoped_lock lock(memo_->mutex);
        if (auto it = memo_->genres.find(box_def_dir); it != memo_->genres.end()) {
            return it->second;
        }
    }
    int genre = static_cast<int>(GenreIndex::NAMCO);
    if (fs::exists(box_def_dir / "box.def")) {
        genre = static_cast<int>(Navigator::parse_box_def_uncached(box_def_dir).genre_index);
    }
    if (memo_) {
        std::scoped_lock lock(memo_->mutex);
        memo_->genres.emplace(box_def_dir, genre);
    }
    return genre;
}

static std::string normalize_exam_type(std::string t) {
    std::ranges::transform(t, t.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    static const std::map<std::string, std::string> names = {
        {"judgeperfect", "judgeperfect"},
        {"jp", "judgeperfect"},
        {"judgegood", "judgegood"},
        {"jg", "judgegood"},
        {"judgebad", "judgebad"},
        {"jb", "judgebad"},
        {"score", "score"},
        {"s", "score"},
        {"roll", "renda"},
        {"r", "renda"},
        {"renda", "renda"},
        {"hit", "hit"},
        {"h", "hit"},
        {"combo", "combo"},
        {"c", "combo"},
        {"accuracy", "accuracy"},
        {"a", "accuracy"},
        {"judgeadlib", "judgeadlib"},
        {"ja", "judgeadlib"},
        {"judgemine", "judgemine"},
        {"jm", "judgemine"},
    };
    auto it = names.find(t);
    return it != names.end() ? it->second : "gauge";
}

Exam DanNavigator::parse_exam(const rapidjson::Value& e) {
    Exam exam;
    if (e.HasMember("type") && e["type"].IsString()) {
        exam.type = normalize_exam_type(e["type"].GetString());
    }
    exam.range = e.HasMember("range") && e["range"].IsString() ? e["range"].GetString() : "";
    std::ranges::transform(exam.range, exam.range.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (exam.range != "less") {
        exam.range = "more";
    }
    if (e.HasMember("value") && e["value"].IsArray()) {
        const auto& v = e["value"];
        bool per_song = false;
        for (const auto& x : v.GetArray()) {
            per_song |= x.IsArray() || x.IsNull();
        }
        if (per_song) {
            // per-song borders: [[red, gold], null, [red, gold]] (gold optional; null = no border,
            // kept as a 0 placeholder so the song indices stay aligned)
            for (const auto& pair : v.GetArray()) {
                int red = 0, gold = Exam::GOLD_FULL;
                if (pair.IsArray() && pair.Size() >= 1 && pair[0].IsInt()) {
                    red = pair[0].GetInt();
                    gold = pair.Size() >= 2 && pair[1].IsInt() ? pair[1].GetInt() : Exam::GOLD_FULL;
                }
                exam.song_red.push_back(red);
                exam.song_gold.push_back(gold);
            }
            if (!exam.song_red.empty()) {
                exam.red = exam.song_red[0];
                exam.gold = exam.song_gold[0];
            }
        } else if (v.Size() >= 1 && v[0].IsInt()) {
            exam.red = v[0].GetInt();
            exam.gold = v.Size() >= 2 && v[1].IsInt() ? v[1].GetInt() : exam.red; // a lone value is both borders
        }
    }
    // A gold on the wrong side of its red is not a border (bad data would light the rainbow
    // the moment the value passes it): treat that song as red-only.
    auto sane = [&](int red, int gold) {
        if (gold == Exam::GOLD_FULL) {
            return gold;
        }
        const bool bad = exam.range == "less" ? gold > red : gold < red;
        return bad ? Exam::GOLD_FULL : gold;
    };
    for (size_t i = 0; i < exam.song_gold.size(); i++) {
        exam.song_gold[i] = sane(exam.song_red[i], exam.song_gold[i]);
    }
    exam.gold = sane(exam.red, exam.gold);
    // The shape of `value` says how the exam is judged: one [red, gold] pair = the whole
    // course, one pair per song = each song on its own. (`gothrough` is no longer read.)
    exam.gothrough = !exam.per_song();
    return exam;
}

std::optional<DanSongEntry> DanNavigator::load_song_entry(const rapidjson::Value& chart,
                                                          std::pair<std::string, std::string>* titles_out) {
    try {
        auto str = [&](const char* k) -> std::string {
            return chart.HasMember(k) && chart[k].IsString() ? chart[k].GetString() : "";
        };
        // difficulty: 0-4 or a name ("oni"); anything else is Oni, as in OpenTaiko
        int diff = 3;
        if (chart.HasMember("difficulty")) {
            const auto& d = chart["difficulty"];
            if (d.IsInt()) {
                diff = d.GetInt();
            } else if (d.IsString()) {
                std::string n = d.GetString();
                std::ranges::transform(n, n.begin(),
                                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                static const std::map<std::string, int> names = {{"easy", 0}, {"normal", 1}, {"hard", 2}, {"oni", 3},
                                                                 {"edit", 4}, {"0", 0},      {"1", 1},    {"2", 2},
                                                                 {"3", 3},    {"4", 4}};
                if (auto it = names.find(n); it != names.end()) {
                    diff = it->second;
                }
            }
        }

        // hash first, title/subtitle when the hash is absent or unknown
        std::string chart_title = str("title");
        std::optional<fs::path> path_opt;
        if (std::string hash = str("hash"); !hash.empty()) {
            path_opt = scores_manager.get_path_by_diff_hash(hash);
            if (!path_opt) {
                path_opt = scores_manager.get_path_by_hash(hash);
            }
        }

        if (!path_opt) {
            auto index = memo_ ? memo_->index : get_song_library().snapshot();

            if (index) {
                path_opt = index->find_song_by_title(chart_title, str("subtitle"));
            }
        }

        if (!path_opt) {
            spdlog::warn("DanNavigator: song '{}' not found", chart_title);
            return std::nullopt;
        }

        auto meta = song_metadata(*path_opt);
        int level = meta->course_data.contains(diff) ? meta->course_data.at(diff).level : 10;

        if (titles_out && global_data.config) {
            const std::string& lang = global_data.config->general.language;
            titles_out->first = localized_text(meta->title, lang, LocalizedTextFallback::ENGLISH);
            titles_out->second = localized_text(meta->subtitle, lang, LocalizedTextFallback::NONE);
        }

        int genre = box_genre(path_opt->parent_path().parent_path());

        bool hidden = chart.HasMember("hidden") && chart["hidden"].IsBool() && chart["hidden"].GetBool();
        if (chart.HasMember("title_show") && chart["title_show"].IsBool() && !chart.HasMember("hidden")) {
            hidden = !chart["title_show"].GetBool();
        }

        return DanSongEntry{*path_opt, genre, diff, level, hidden};
    } catch (...) {
        spdlog::warn("DanNavigator: failed to parse song entry");
        return std::nullopt;
    }
}

std::optional<DanBoxData> DanNavigator::load_dan_box_data(const fs::path& json_path) {
    auto doc = read_json_file(json_path);
    if (!doc.IsObject() || !doc.HasMember("title")) {
        spdlog::warn("DanNavigator: {} is missing required title", json_path.string());
        return std::nullopt;
    }
    // title: a string, or a {language: text} object (.tcm)
    std::string title;
    if (doc["title"].IsString()) {
        title = doc["title"].GetString();
    } else if (doc["title"].IsObject()) {
        std::map<std::string, std::string> values;
        for (auto& m : doc["title"].GetObject()) {
            if (m.value.IsString()) {
                values[m.name.GetString()] = m.value.GetString();
            }
        }
        const std::string lang = global_data.config ? global_data.config->general.language : "en";
        title = localized_text(values, lang, LocalizedTextFallback::ENGLISH_OR_FIRST);
    }
    if (title.empty()) {
        spdlog::warn("DanNavigator: {} has no usable title", json_path.string());
        return std::nullopt;
    }
    int color = doc.HasMember("color") && doc["color"].IsInt() ? doc["color"].GetInt() : 0;
    int rank = doc.HasMember("rank_art") && doc["rank_art"].IsInt() ? doc["rank_art"].GetInt() : -1;
    int dan_index = -1;
    if (doc.HasMember("dan_index") && doc["dan_index"].IsInt()) {
        dan_index = doc["dan_index"].GetInt();
    } else {
        try {
            std::string dir = json_path.parent_path().filename().string();
            size_t pos = 0;
            int v = std::stoi(dir, &pos);
            if (pos > 0) {
                dan_index = v;
            }
        } catch (...) {
        }
    }
    if (dan_index < 0 || dan_index > 24) {
        dan_index = -1;
    }

    std::vector<DanSongEntry> songs;
    std::vector<std::pair<std::string, std::string>> song_titles;
    std::vector<size_t> kept; // chart index of each loaded song, to realign per-song exam values
    if (doc.HasMember("charts") && doc["charts"].IsArray()) {
        size_t ci = 0;
        for (auto& chart : doc["charts"].GetArray()) {
            std::pair<std::string, std::string> t;
            if (chart.IsObject()) {
                if (auto entry = load_song_entry(chart, &t)) {
                    songs.push_back(*entry);
                    song_titles.push_back(std::move(t));
                    kept.push_back(ci);
                }
            }
            ci++;
        }
    }
    if (songs.empty()) {
        return std::nullopt;
    }

    std::vector<Exam> exams;
    if (doc.HasMember("exams") && doc["exams"].IsArray()) {
        for (auto& e : doc["exams"].GetArray()) {
            if (!e.IsObject()) {
                continue;
            }
            Exam ex = parse_exam(e);
            if (ex.type.empty()) {
                continue;
            }
            if (ex.per_song()) {
                std::vector<int> red, gold;
                for (size_t idx : kept) {
                    red.push_back(idx < ex.song_red.size() ? ex.song_red[idx] : 0);
                    gold.push_back(idx < ex.song_gold.size() ? ex.song_gold[idx] : Exam::GOLD_FULL);
                }
                ex.song_red = red;
                ex.song_gold = gold;
                ex.red = red[0];
                ex.gold = gold[0];
            }
            exams.push_back(ex);
        }
    }

    DanBoxData d;
    d.json_path = json_path;
    d.title = title;
    d.color = color;
    d.rank = rank;
    d.dan_index = dan_index;
    d.songs = songs;
    d.song_titles = std::move(song_titles);
    d.exams = std::move(exams);
    d.total_notes = total_notes_for(songs);
    d.gaiden = doc.HasMember("gaiden") && doc["gaiden"].IsBool() && doc["gaiden"].GetBool();
    return d;
}

std::unique_ptr<DanBox> DanNavigator::make_box(const DanBoxData& d) {
    auto box = std::make_unique<DanBox>(d.json_path, d.title, d.color, d.songs, d.exams, d.total_notes);
    box->dan_rank = d.rank;
    box->dan_index = d.dan_index;
    box->gaiden = d.gaiden;
    box->song_titles = d.song_titles;
    return box;
}

std::unique_ptr<DanBox> DanNavigator::load_dan_box(const fs::path& json_path) {
    auto d = load_dan_box_data(json_path);
    if (!d) {
        return nullptr;
    }
    return make_box(*d);
}

void DanNavigator::collect_dan_jsons(const fs::path& root_path, std::vector<fs::path>& out) {
    if (root_path.empty()) {
        spdlog::warn("DanNavigator: skipping an empty dan root path");
        return;
    }
    std::error_code ec;
    if (!fs::is_directory(root_path, ec)) {
        spdlog::warn("DanNavigator: skipping dan root '{}': not a directory ({})", root_path.string(),
                     ec ? ec.message() : "no such directory");
        return;
    }
#ifdef SUPPORT_FUMEN
    if (!gen4::find_data_root(root_path).empty() || !gen3::find_data_root(root_path).empty()) {
        return;
    }
#endif

    try {
        auto it = fs::recursive_directory_iterator(root_path, fs::directory_options::skip_permission_denied);
        for (; it != fs::end(it); ++it) {
            if (scan_abort.load()) {
                break;
            }
            const auto& entry = *it;
#ifdef SUPPORT_FUMEN
            if (entry.is_directory() && (gen4::find_data_root(entry.path()) == entry.path() ||
                                         gen3::find_data_root(entry.path()) == entry.path())) {
                it.disable_recursion_pending();
                continue;
            }
#endif
            const auto name = entry.path().filename();
            const auto ext = entry.path().extension();
            if (name == "dan.json" || ext == ".tcm" || ext == ".optktcm") {
                out.push_back(entry.path());
            }
        }
    } catch (const std::exception& ex) {
        spdlog::warn("DanNavigator: error loading {}: {}", root_path.string(), ex.what());
    }
}

// Loads the courses on a few worker threads, in the order given
std::vector<DanBoxData> DanNavigator::load_dan_jsons(const std::vector<fs::path>& jsons) {
    ScanMemo memo;
    memo.index = get_song_library().snapshot();
    memo_ = &memo;
    std::vector<std::optional<DanBoxData>> loaded(jsons.size());
    std::atomic<size_t> cursor{0};
    unsigned pool_size = std::max(2u, std::thread::hardware_concurrency() / 2);
    std::vector<std::thread> pool;
    pool.reserve(pool_size);
    for (unsigned t = 0; t < pool_size; t++) {
        pool.emplace_back([&] {
            for (;;) {
                size_t i = cursor.fetch_add(1);
                if (i >= jsons.size() || scan_abort.load()) {
                    break;
                }
                try {
                    loaded[i] = load_dan_box_data(jsons[i]);
                } catch (const std::exception& ex) {
                    spdlog::warn("DanNavigator: failed to load {}: {}", jsons[i].string(), ex.what());
                }
            }
        });
    }
    for (std::thread& worker : pool) {
        worker.join();
    }
    memo_ = nullptr;

    std::vector<DanBoxData> data;
    for (auto& d : loaded) {
        if (d) {
            data.push_back(std::move(*d));
        }
    }
    return data;
}

// The scan result for the same dan.json files (paths and modification times) over the same song
// library is reused for the rest of the session: re-entering the dojo re-read every course
std::vector<DanBoxData> DanNavigator::scan_roots(const std::vector<fs::path>& roots) {
    if (!get_song_library().wait(scan_abort)) {
        return {};
    }

    struct Cached {
        std::vector<std::pair<fs::path, fs::file_time_type>> signature;
        size_t library_size = 0;
        std::string language; // song titles are read in the UI language
        std::vector<DanBoxData> data;
    };
    static std::mutex cache_mutex;
    static std::map<std::vector<fs::path>, Cached> cache;

    std::vector<fs::path> jsons;
    for (const fs::path& root_path : roots) {
        if (scan_abort.load()) {
            return {};
        }
        collect_dan_jsons(root_path, jsons);
    }
    std::vector<std::pair<fs::path, fs::file_time_type>> signature;
    signature.reserve(jsons.size());
    for (const fs::path& json : jsons) {
        std::error_code ec;
        signature.emplace_back(json, fs::last_write_time(json, ec));
    }
    const size_t library_size = get_song_library().song_file_count();
    const std::string language = global_data.config ? global_data.config->general.language : std::string();

    {
        std::scoped_lock lock(cache_mutex);
        if (auto it = cache.find(roots); it != cache.end() && it->second.signature == signature &&
                                         it->second.library_size == library_size && it->second.language == language) {
            return it->second.data;
        }
    }

    std::vector<DanBoxData> data = load_dan_jsons(jsons);
    if (scan_abort.load()) {
        return data;
    }
    std::scoped_lock lock(cache_mutex);
    cache[roots] = Cached{std::move(signature), library_size, language, data};
    return data;
}

int DanNavigator::scan_root_data(const fs::path& root_path, std::vector<DanBoxData>& out) {
    std::vector<DanBoxData> data = scan_roots({root_path});
    int added = static_cast<int>(data.size());
    for (DanBoxData& d : data) {
        out.push_back(std::move(d));
    }
    return added;
}

int DanNavigator::scan_root(const fs::path& root_path) {
    std::vector<DanBoxData> data;
    int added = scan_root_data(root_path, data);
    for (const DanBoxData& d : data) {
        boxes.push_back(make_box(d));
    }
    return added;
}

std::vector<DanBoxData> DanNavigator::scan_all_data(const std::vector<fs::path>& song_paths) {
    std::vector<DanBoxData> data = scan_roots(song_paths);

    if (data.empty() && global_data.config && !scan_abort.load()) {
        std::vector<fs::path> library;
        for (const fs::path& lib_root : global_data.config->paths.tja_path) {
            if (!std::ranges::contains(song_paths, lib_root)) {
                library.push_back(lib_root);
            }
        }
        data = scan_roots(library);
        if (!data.empty()) {
            spdlog::warn("DanNavigator: the requested dan root yielded nothing; "
                         "recovered {} course(s) by re-scanning the song library",
                         data.size());
        }
    }
    return data;
}

void DanNavigator::publish(std::vector<DanBoxData>&& data) {
    boxes.clear();
    selected_index = 0;
    // Order: courses grouped by the folder they sit in (the dan root's own courses first,
    // then each version set such as "Nijiiro 2026" as one block), and inside a group by the
    // leading number of the course folder, then by name. The cabinet lists one version at a
    // time; here every set is listed, but never interleaved.
    auto order_key = [](const DanBoxData& d) {
        const std::string set = d.json_path.parent_path().parent_path().string();
        const std::string name = d.json_path.parent_path().filename().string();
        size_t i = 0;
        while (i < name.size() && static_cast<unsigned char>(name[i]) >= '0' &&
               static_cast<unsigned char>(name[i]) <= '9') {
            i++;
        }
        int num = INT_MAX;
        if (i > 0) {
            try {
                num = std::stoi(name.substr(0, i));
            } catch (...) {
            }
        }
        return std::make_tuple(set, num, name);
    };
    std::ranges::stable_sort(data,
                             [&](const DanBoxData& x, const DanBoxData& y) { return order_key(x) < order_key(y); });
    boxes.reserve(data.size());
    for (const DanBoxData& d : data) {
        boxes.push_back(make_box(d));
    }

    if (boxes.empty()) {
        spdlog::warn("DanNavigator: no dan courses found");
        return;
    }

    if (tex.has_texture("back/folder")) {
        auto back =
            std::make_unique<DanBox>(fs::path("back"), "back", 0, std::vector<DanSongEntry>{}, std::vector<Exam>{}, 0);
        back->is_back = true;
        boxes.push_back(std::move(back));
    }

    if (!restore_selection_path.empty()) {
        const auto it = std::ranges::find_if(
            boxes, [&](const auto& box) { return !box->is_back && box->path == restore_selection_path; });

        if (it != boxes.end()) {
            selected_index = static_cast<int>(std::ranges::distance(boxes.begin(), it));
        }

        restore_selection_path.clear();
    }

    set_positions(true, 0);
    boxes[selected_index]->expand_box();
    for (auto& b : boxes) {
        b->fade_in(100);
    }
}

void DanNavigator::begin_init(const std::vector<fs::path>& song_paths) {
    DanBox* current = get_current();
    restore_selection_path = current && !current->is_back ? current->path : fs::path{};

    t_cursor = tex.has_texture("box/cursor") ? tex.get_texture("box/cursor") : nullptr;
    t_arrow_r = tex.has_texture("box/arrow_r") ? tex.get_texture("box/arrow_r") : nullptr;
    t_arrow_l = tex.has_texture("box/arrow_l") ? tex.get_texture("box/arrow_l") : nullptr;

    abort_init();
    boxes.clear();
    selected_index = 0;
    scan_done.store(false);
    scan_abort.store(false);
    scan_published = false;
    {
        std::scoped_lock lock(scan_mutex);
        scan_result.clear();
    }

    scan_thread = std::thread([this, song_paths] {
        std::vector<DanBoxData> data;
        try {
            data = scan_all_data(song_paths);
        } catch (const std::exception& ex) {
            spdlog::error("DanNavigator: course scan threw: {}", ex.what());
        } catch (...) {
            spdlog::error("DanNavigator: course scan threw (unknown)");
        }
        {
            std::scoped_lock lock(scan_mutex);
            scan_result = std::move(data);
        }
        scan_done.store(true);
    });
}

bool DanNavigator::poll_init() {
    if (scan_published) {
        return true;
    }
    if (!scan_thread.joinable()) {
        return false;
    }
    if (!scan_done.load()) {
        return false; // acquire
    }
    scan_thread.join();
    std::vector<DanBoxData> data;
    {
        std::scoped_lock lock(scan_mutex);
        data = std::move(scan_result);
        scan_result.clear();
    }
    publish(std::move(data));
    scan_published = true;
    return true;
}

void DanNavigator::abort_init() {
    scan_abort.store(true);
    if (scan_thread.joinable()) {
        scan_thread.join();
    }
    scan_abort.store(false);
    scan_done.store(false);
    scan_published = false;
    {
        std::scoped_lock lock(scan_mutex);
        scan_result.clear();
    }
}

DanNavigator::~DanNavigator() {
    abort_init();
}

void DanNavigator::init(const std::vector<fs::path>& song_paths) {
    begin_init(song_paths);
    while (scan_thread.joinable() && !scan_done.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    poll_init();
}

void DanNavigator::set_positions(bool init, float duration) {
    int n = static_cast<int>(boxes.size());
    if (n == 0) {
        return;
    }
    const RibbonLayout lay = ribbon_layout();
    for (int i = 0; i < n; i++) {
        float offset = i - selected_index;
        if (offset > n / 2.0f) {
            offset -= n;
        } else if (offset < -n / 2.0f) {
            offset += n;
        }

        const float k = lay.legacy ? tex.screen_scale : 1.0f;
        float base = lay.center * k;
        float spacing = lay.spacing * k;
        float side_l = lay.side_l * k;
        float side_r = lay.side_r * k;

        float pos;
        if (lay.legacy) {
            float anchor = base - spacing;
            pos = anchor + offset * spacing;
            if (std::abs(pos - anchor) < 1.0f) {
                pos = base;
            } else if (pos > anchor) {
                pos += side_r;
            } else {
                pos -= side_l;
            }
        } else {
            pos = base + offset * spacing;
            if (offset > 0) {
                pos += side_r;
            } else if (offset < 0) {
                pos -= side_l;
            }
        }

        if (init || std::abs(pos - boxes[i]->position) >= tex.screen_width) {
            boxes[i]->set_position(pos);
        } else {
            boxes[i]->move_box(pos, duration);
        }
    }
}

void DanNavigator::move_left() {
    last_moved = get_current_ms();
    if (boxes.empty()) {
        return;
    }
    boxes[selected_index]->close_box();
    const auto box_count = static_cast<int>(boxes.size());
    selected_index = (selected_index - 1 + box_count) % box_count;
    set_positions(false, 166);
    boxes[selected_index]->expand_box();
}

void DanNavigator::move_right() {
    last_moved = get_current_ms();
    if (boxes.empty()) {
        return;
    }
    boxes[selected_index]->close_box();
    selected_index = (selected_index + 1) % static_cast<int>(boxes.size());
    set_positions(false, 166);
    boxes[selected_index]->expand_box();
}

void DanNavigator::skip(int delta) {
    last_moved = get_current_ms();
    if (boxes.empty()) {
        return;
    }
    boxes[selected_index]->close_box();
    const auto box_count = static_cast<int>(boxes.size());
    selected_index = ((selected_index + delta) % box_count + box_count) % box_count;
    set_positions(true, 0);
    boxes[selected_index]->expand_box();
}

DanBox* DanNavigator::get_current() {
    if (boxes.empty()) {
        return nullptr;
    }
    return boxes[selected_index].get();
}

void DanNavigator::update(double current_ms) {
    if (!paint_tried) {
        load_paint_surface();
    }
    if (fn_update.valid()) {
        fn_update(lua_paint, current_ms);
    }

    bool built_one = false;
    for (auto& b : boxes) {
        bool on_screen =
            b->position > -156 * tex.screen_scale && b->position < tex.screen_width + 144 * tex.screen_scale;
        if (on_screen && !b->text_loaded && !built_one) {
            b->load_text();
            built_one = true;
        }
        b->update(current_ms);
    }
}

static float dan_cursor_alpha(double ms) {
    static const float A[12] = {1.000f, 0.977f, 0.910f, 0.801f, 0.645f, 0.441f,
                                0.301f, 0.520f, 0.699f, 0.836f, 0.934f, 0.988f};
    double f = std::fmod(ms * 0.06, 60.0); // ms -> arcade frames, one loop
    if (f < 0) {
        f += 60.0;
    }
    double t = f / 5.0; // the table is every 5th frame
    int i = static_cast<int>(t);
    float u = t - i;
    return A[i % 12] * (1.0f - u) + A[(i + 1) % 12] * u;
}

void DanNavigator::load_paint_surface() {
    paint_tried = true;
    paint_ok = false;
    if (!script_manager.lua || !script_manager.has_lua_script("dan_select")) {
        return;
    }
    sol::state& lua = *script_manager.lua;
    if (!lua["DanSelect"].valid()) {
        auto r = lua.script_file(script_manager.get_lua_script_path("dan_select"));
        if (!r.valid()) {
            sol::error err = r;
            spdlog::error("dan_select.lua load error: {}", err.what());
            return;
        }
    }
    sol::optional<sol::table> cls = lua["DanSelect"];
    if (!cls) {
        return;
    }
    sol::protected_function ctor = (*cls)["new"];
    if (!ctor.valid()) {
        return;
    }
    auto res = ctor();
    if (!res.valid()) {
        sol::error err = res;
        spdlog::error("DanSelect.new error: {}", err.what());
        return;
    }
    lua_paint = res;
    fn_draw_cursor = lua_paint["draw_cursor"];
    fn_draw_box = lua_paint["draw_box"];
    fn_update = lua_paint["update"];
    paint_ok = fn_draw_cursor.valid();
}

void DanNavigator::draw() {
    if (!paint_tried) {
        load_paint_surface();
    }

    for (auto& b : boxes) {
        float pos = b->position;
        if (pos >= -156 * tex.screen_scale && pos <= tex.screen_width + 144 * tex.screen_scale && fn_draw_box.valid()) {
            fn_draw_box(lua_paint, b.get());
        }
    }

    if (boxes.empty()) {
        return;
    }
    DanBox* cur = boxes[selected_index].get();
    if (!cur) {
        return;
    }
    const float pos = cur->position;
    const double now = get_current_ms();

    if (paint_ok) {
        auto r = fn_draw_cursor(lua_paint, pos, now, last_moved > 0 ? now - last_moved : -1.0);
        if (r.valid()) {
            return;
        }
        sol::error err = r;
        spdlog::error("DanSelect:draw_cursor error ({}), reverting to inline tables", err.what());
        paint_ok = false;
    }

    if (t_cursor) {
        tex.draw_texture(t_cursor, {.x = pos, .fade = dan_cursor_alpha(now)});
    }

    if (last_moved <= 0) {
        return;
    }
    const double af = (now - last_moved) * 0.06;
    if (af < 0.0 || af > 60.0) {
        return;
    }
    const float drift = af / 60.0 * 10.0;
    const float aa = (af <= 30.0) ? 1.0f : (1.0 - (af - 30.0) / 30.0);
    if (t_arrow_r) {
        tex.draw_texture(t_arrow_r, {.x = pos + drift, .fade = aa});
    }
    if (t_arrow_l) {
        tex.draw_texture(t_arrow_l, {.x = pos - drift, .fade = aa});
    }
}
