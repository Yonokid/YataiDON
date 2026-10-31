#include "navigation_load.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <ranges>
#include <type_traits>
#include <utility>

#include "../../../libs/file/filesystem.h"
#include "../../../libs/file/scores.h"
#include "../../../libs/song_library.h"
#include "../../../libs/song_library_loader.h"
#include "color_utils.h"
#include "navigation_scan.h"
#ifdef SUPPORT_FUMEN
#include "../../../libs/optional/gen3.h"
#include "../../../libs/optional/gen4.h"
#endif

namespace ch = std::chrono;

static void prepare_folder_summaries(const LoadRequest& request, const std::stop_token& stop,
                                     const std::function<void(PreparedEntry)>& emit);

static std::unique_ptr<PreparedSong> make_song_box(const fs::path& path, const BoxDef& box_def, SongParser parser) {
    return std::make_unique<PreparedSong>(path, box_def, std::move(parser));
}

static std::unordered_map<std::string, std::unique_ptr<SongParser>>
parse_songs_parallel(const std::vector<fs::path>& paths, std::stop_token stop) {
    std::vector<std::unique_ptr<SongParser>> parsed(paths.size());
    unsigned workers = std::clamp(std::thread::hardware_concurrency(), 2u, 8u);
    std::atomic<size_t> next{0};
    std::vector<std::jthread> pool;
    for (unsigned t = 0; t < workers && t < paths.size(); t++) {
        pool.emplace_back([&] {
            for (size_t i; (i = next.fetch_add(1)) < paths.size();) {
                if (stop.stop_requested()) {
                    break;
                }

                try {
                    parsed[i] = std::make_unique<SongParser>(paths[i]);
                } catch (const std::exception& e) {
                    spdlog::warn("Failed to parse {}: {}", paths[i].string(), e.what());
                }
            }
        });
    }
    for (auto& th : pool) {
        th.join();
    }

    std::unordered_map<std::string, std::unique_ptr<SongParser>> out;
    for (size_t i = 0; i < paths.size(); i++) {
        if (parsed[i]) {
            out.emplace(paths[i].string(), std::move(parsed[i]));
        }
    }
    return out;
}

static SongParser take_parser(std::unordered_map<std::string, std::unique_ptr<SongParser>>& preparsed,
                              const fs::path& path) {
    auto it = preparsed.find(path.string());
    if (it != preparsed.end()) {
        SongParser p = std::move(*it->second);
        preparsed.erase(it);
        return p;
    }
    return SongParser(path);
}

static std::unique_ptr<PreparedBack> make_back_box(const fs::path& path, const BoxDef* folder = nullptr) {
    BoxDef def{};
    def.genre_index = folder ? folder->genre_index : GenreIndex::NAMCO;
    return std::make_unique<PreparedBack>(path, def);
}

static void apply_song_genre(PreparedSong* song, const BoxDef& box_def) {
    song->song_genre = box_def;
}

class NavigationLoad {
    std::shared_ptr<const SongIndex> index;
    const LoadRequest& request;
    std::function<void(PreparedEntry)> emit;
    std::atomic<bool> abort_loading{false};
    std::stop_token cancellation_token;
    std::stop_callback<std::function<void()>> cancellation;
    std::vector<fs::path> root_paths;
    std::string language;
    std::string player_1;
    std::string current_search;
    bool hide_dan;
    bool reloading_roots;
    bool genre_box_defs_built = false;
    std::map<GenreIndex, BoxDef> genre_box_defs;
    BoxDef inline_back_def;
    std::optional<std::array<int, 3>> diff_sort_filter;
    navigation_scan::FolderScanner folder_scanner;
    std::unordered_map<std::string, BoxDef> box_def_cache;

    bool ensure_index() {
        if (abort_loading) {
            return false;
        }

        if (!index) {
            auto& library = get_song_library();

            if (!library.wait(abort_loading)) {
                if (abort_loading) {
                    return false;
                }

                throw std::runtime_error("Song indexing did not complete");
            }

            index = library.snapshot();
        }

        return true;
    }

    template<class Entry>
    void enqueue_box(std::unique_ptr<Entry> entry) {
        if constexpr (std::is_same_v<Entry, PreparedFolder>) {
            entry->directory_definition = parse_box_def(entry->path);

            try {
                entry->has_children = folder_scanner.has_child_folders(entry->path);
            } catch (const std::exception& e) {
                spdlog::warn("Folder scan of {} failed: {}", entry->path.string(), e.what());
            }
        }

        std::error_code ec;
        auto last_write = fs::last_write_time(entry->path, ec);
        entry->is_new = !ec && last_write < fs::file_time_type::clock::now() - ch::weeks(2);
        emit(std::move(*entry));
    }

    template<class Entry>
    void enqueue_inline_box(std::unique_ptr<Entry> entry) {
        if constexpr (std::is_same_v<Entry, PreparedFolder>) {
            entry->directory_definition = parse_box_def(entry->path);
        }

        emit(std::move(*entry));
    }

    void parse_song_list(const fs::path& path, const BoxDef& box_def, bool inline_mode);
    void load_current_directory_async(const fs::path& path);
    std::vector<fs::path> find_new_songs(const fs::path& path) const;
    void load_collection_new(const fs::path& path, const BoxDef& box_def);
    void load_collection_difficulty(const fs::path& path, const BoxDef& box_def, int course, int level, int order = 1);
    void load_from_song_list(const fs::path& path, const BoxDef& box_def, bool mark_favorite);
    void load_collection_recommended(const fs::path& path, const BoxDef& box_def);
    void load_collection_search(const fs::path& path, const BoxDef& box_def);
    void load_collection_play_count(const fs::path& path, const BoxDef& box_def);
    void scan_song_tree(const fs::path& path, std::vector<fs::path>& song_paths,
                        std::unordered_map<std::string, std::vector<std::pair<bool, fs::path>>>& plan);
    void load_songs_inline_async(const fs::path& path, const BoxDef& box_def);
    BoxDef parse_box_def(const fs::path& path);

    bool is_song_file(const fs::path& path) { return is_library_song_file(path); }
    const BoxDef* box_def_for_genre(GenreIndex genre);
    bool only_gen4_songs();
    void load_gen4_genres(const fs::path& data_root);
    bool load_gen4_genre_songs(const fs::path& genre_path, const BoxDef& box_def);
    bool is_gen3_song_folder(const fs::path& path);
    void load_gen3_genres(const fs::path& data_root);
    bool load_gen3_genre_songs(const fs::path& genre_path, const BoxDef& box_def);
    bool is_gen4_song_folder(const fs::path& path);

public:
    NavigationLoad(const LoadRequest& request, const std::stop_token& stop, std::function<void(PreparedEntry)> emit)
        : index(request.index), request(request), emit(std::move(emit)), cancellation_token(stop),
          cancellation(stop, [this] { abort_loading = true; }), root_paths(request.roots), language(request.language),
          player_1(request.player_1), current_search(request.search), hide_dan(request.hide_dan),
          reloading_roots(request.kind == LoadRequest::Kind::ROOTS), inline_back_def(request.back_def),
          diff_sort_filter(request.difficulty), folder_scanner(stop) {}

    void run() {
        if (request.kind == LoadRequest::Kind::SUMMARIES) {
            prepare_folder_summaries(request, cancellation_token, emit);
            return;
        }

        if (request.kind == LoadRequest::Kind::INLINE) {
            load_songs_inline_async(request.path, request.box_def);
        } else if (reloading_roots) {
            std::string failures;

            for (const auto& root : root_paths) {
                if (abort_loading) {
                    break;
                }

                try {
                    load_current_directory_async(root);
                } catch (const std::exception& e) {
                    failures += root.string() + ": " + e.what() + "\n";
                } catch (...) {
                    failures += root.string() + ": unknown loading error\n";
                }
            }

            if (!failures.empty() && !abort_loading) {
                throw std::runtime_error("Failed to load song roots:\n" + failures);
            }
        } else {
            load_current_directory_async(request.path);
        }
    }
};

void NavigationLoad::parse_song_list(const fs::path& path, const BoxDef& box_def, bool inline_mode) {
    auto snapshot = read_song_list_snapshot(path);
    const auto& entries = snapshot.entries;
    std::vector<SongListEntry> out_entries;
    bool needs_rewrite = false;
    int songs_added = 0;

    // Resolve every entry first so the songs can be parsed in parallel
    std::vector<fs::path> resolved(entries.size());
    for (size_t i = 0; i < entries.size() && !abort_loading; i++) {
        const auto& entry = entries[i];
        fs::path& final_path = resolved[i];

        // Hash is the primary reference. If several songs share it, title and
        // subtitle disambiguate only among candidates with that same hash.
        if (auto found = scores_manager.get_path_by_hash(entry.hash)) {
            final_path = *found;
        }
        if (!entry.title.empty() || !entry.subtitle.empty()) {
            if (auto by_title = index->find_song_by_title(entry.title, entry.subtitle);
                by_title && scores_manager.get_single_hash(*by_title) == entry.hash) {
                final_path = *by_title;
            }
        }
        if (final_path.empty()) {
            if (auto song_path_opt = index->find_song_by_title(entry.title, entry.subtitle)) {
                final_path = *song_path_opt;
            }
        }
    }
    std::vector<fs::path> to_parse;
    for (const fs::path& p : resolved) {
        if (!p.empty()) {
            to_parse.push_back(p);
        }
    }
    auto preparsed = parse_songs_parallel(to_parse, cancellation_token);

    for (size_t i = 0; i < entries.size() && !abort_loading; i++) {
        const auto& entry = entries[i];
        const fs::path& final_path = resolved[i];
        if (final_path.empty()) {
            out_entries.push_back(entry);
            spdlog::warn("No song found for: {} | {}", entry.title, entry.subtitle);
            continue;
        }

        SongParser parser = take_parser(preparsed, final_path);
        const auto& titles = parser.metadata.title;
        const auto& subtitles = parser.metadata.subtitle;
        const std::string title = titles.contains("en") ? titles.at("en") : titles.begin()->second;
        const std::string subtitle = subtitles.contains("en") ? subtitles.at("en") : subtitles.begin()->second;
        const std::string correct_hash = scores_manager.get_single_hash(final_path);
        out_entries.push_back({correct_hash, title, subtitle});
        needs_rewrite |= entry.hash != correct_hash || entry.title != title || entry.subtitle != subtitle;

        auto box = make_song_box(final_path, box_def, std::move(parser));
        box->preserve_order = true;

        if (inline_mode && songs_added > 0 && songs_added % 10 == 0) {
            enqueue_inline_box(make_back_box(path.parent_path().parent_path(), &inline_back_def));
        }

        if (inline_mode) {
            enqueue_inline_box(std::move(box));
        } else {
            enqueue_box(std::move(box));
        }
        songs_added++;
    }

    if (needs_rewrite && !abort_loading) {
        normalize_song_list(path, std::move(snapshot), std::move(out_entries), cancellation_token);
    }
}

void NavigationLoad::load_current_directory_async(const fs::path& path) {
    BoxDef box_def = parse_box_def(path);

    inline_back_def = box_def;

#ifdef SUPPORT_FUMEN
    fs::path own_root = gen4::find_data_root(path);
    bool own_root_is_gen3 = false;
    if (own_root.empty() || !gen4::library_for(own_root)) {
        fs::path gen3_root = gen3::find_data_root(path);
        if (!gen3_root.empty() && gen3::library_for(gen3_root)) {
            own_root = gen3_root;
            own_root_is_gen3 = true;
        }
    }
    bool is_a_root = std::find(root_paths.begin(), root_paths.end(), path) != root_paths.end();
    if (!own_root.empty() && is_a_root && (own_root_is_gen3 || gen4::library_for(own_root))) {
        if (reloading_roots && !only_gen4_songs()) {
            BoxDef gen4_def = box_def;
            gen4_def.name = own_root.filename().string();
            gen4_def.genre_index = GenreIndex::DEFAULT;
            enqueue_box(std::make_unique<PreparedFolder>(own_root, gen4_def));

            return;
        }
        for (const fs::path& root : root_paths) {
            if (!gen4::find_data_root(root).empty()) {
                continue;
            }
            enqueue_box(make_back_box(root));
            break;
        }
        try {
            if (own_root_is_gen3) {
                load_gen3_genres(own_root);
            } else {
                load_gen4_genres(own_root);
            }
        } catch (const std::exception& e) {
            spdlog::error("Error listing arcade genres of {}: {}", own_root.string(), e.what());
            throw;
        }

        return;
    }
#endif

    if (navigation_scan::is_gen4_root(path) || navigation_scan::is_gen3_root(path)) {
        try {
            if (navigation_scan::is_gen3_root(path)) {
                load_gen3_genres(path);
            } else {
                load_gen4_genres(path);
            }
        } catch (const std::exception& e) {
            spdlog::error("Error listing gen4 genres of {}: {}", path.string(), e.what());
            throw;
        } catch (...) {
            spdlog::error("Unknown error listing gen4 genres of {}", path.string());
            throw;
        }

        return;
    }

    std::vector<fs::path> song_paths;
    try {
        for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
            if (abort_loading) {
                break;
            }
            if (!fs::is_directory(entry.path()) && is_song_file(entry.path())) {
                song_paths.push_back(entry.path());
            }
        }
    } catch (const fs::filesystem_error&) { /* main loop reports errors */
    }
    auto preparsed = parse_songs_parallel(song_paths, cancellation_token);

    try {
        for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
            if (abort_loading) {
                break;
            }
            const fs::path& curr_path = entry.path();
            try {
                if (!fs::is_directory(curr_path)) {
                    if (curr_path.filename() == "song_list.txt") {
                        if (!ensure_index()) {
                            break;
                        }

                        BoxDef entry_box_def = parse_box_def(curr_path);
                        parse_song_list(curr_path, entry_box_def, false);
                        continue;
                    }
                    if (is_song_file(curr_path)) {
                        enqueue_box(make_song_box(curr_path, box_def, take_parser(preparsed, curr_path)));
                    }
                    continue;
                }
                fs::path groot = navigation_scan::gen3_root_at(curr_path);
                if (navigation_scan::is_gen4_root(curr_path) || !groot.empty()) {
                    if (only_gen4_songs()) {
                        if (!groot.empty()) {
                            load_gen3_genres(groot);
                        } else {
                            load_gen4_genres(curr_path);
                        }
                        continue;
                    }
                    BoxDef gen4_def = box_def;
                    gen4_def.name = curr_path.filename().string();
#ifdef SUPPORT_FUMEN
                    if (!groot.empty()) {
                        if (const gen3::Library* lib = gen3::library_for(groot)) {
                            gen4_def.name = lib->game_name();
                        }
                    }
#endif
                    gen4_def.genre_index = GenreIndex::DEFAULT;
                    enqueue_box(std::make_unique<PreparedFolder>(groot.empty() ? curr_path : groot, gen4_def));
                    continue;
                }
                if (is_gen3_song_folder(curr_path)) {
                    enqueue_box(make_song_box(curr_path, box_def, SongParser(curr_path)));
                    continue;
                }
                if (is_gen4_song_folder(curr_path)) {
                    enqueue_box(make_song_box(curr_path, box_def, SongParser(curr_path)));
                    continue;
                }
                if (folder_scanner.has_def_file(curr_path)) {
                    BoxDef entry_box_def = parse_box_def(curr_path);
                    if (hide_dan && entry_box_def.genre_index == GenreIndex::DAN) {
                        continue;
                    }
                    auto folder = std::make_unique<PreparedFolder>(curr_path, entry_box_def);
                    if (entry_box_def.collection == "RECOMMENDED") {
                        folder->tja_count = 10;
                    }
                    enqueue_box(std::move(folder));
                } else if (navigation_scan::is_osu_song_folder(curr_path)) {
                    BoxDef osu_box_def = box_def;
                    auto it = fs::directory_iterator(curr_path);
                    auto end = fs::end(it);
                    while (it != end && it->path().extension() != ".osu") {
                        ++it;
                    }
                    if (it == end) {
                        continue;
                    }
                    OsuParser title_parser = OsuParser(it->path());
                    title_parser.get_metadata();
                    osu_box_def.name = title_parser.metadata.title[language];
                    std::unique_ptr<PreparedFolder> osu_folder =
                        std::make_unique<PreparedFolder>(curr_path, osu_box_def);
                    osu_folder->is_osu_folder = true;
                    enqueue_box(std::move(osu_folder));
                } else {
                    std::error_code ec;
                    auto it = fs::recursive_directory_iterator(curr_path, ec);
                    while (it != fs::end(it)) {
                        if (abort_loading) {
                            break;
                        }
                        try {
                            if (fs::is_directory(it->path()) && navigation_scan::is_osu_song_folder(it->path())) {
                                it.disable_recursion_pending();
                                BoxDef osu_box_def = box_def;
                                osu_box_def.name = it->path().filename().string();
                                enqueue_box(std::make_unique<PreparedFolder>(it->path(), osu_box_def));
                            } else if (is_song_file(it->path())) {
                                enqueue_box(make_song_box(it->path(), box_def, SongParser(it->path())));
                            }
                        } catch (const std::exception& inner) {
                            spdlog::warn("Skipping song: {}", inner.what());
                        }
                        it.increment(ec);
                        if (ec) {
                            ec.clear();
                        }
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("Skipping entry: {}", e.what());
            }
        }
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Error loading directory: {}", e.what());
        throw;
    } catch (const std::exception& e) {
        spdlog::error("Error loading directory {}: {}", path.string(), e.what());
        throw;
    } catch (...) {
        spdlog::error("Unknown error loading directory {}", path.string());
        throw;
    }
}

std::vector<fs::path> NavigationLoad::find_new_songs(const fs::path& path) const {
    std::vector<fs::path> hits;
    const fs::path parent = path.parent_path();
    const auto two_weeks_ago = fs::file_time_type::clock::now() - ch::weeks(2);
    for (const auto& song_path : index->song_paths_under(parent, &abort_loading)) {
        if (abort_loading) {
            break;
        }

        fs::path rel = song_path.lexically_relative(parent);
        fs::path sibling = parent / *rel.begin();
        if (sibling == path) {
            continue;
        }
        std::error_code ec;
        auto last_write = fs::last_write_time(song_path.parent_path(), ec);
        if (ec || last_write < two_weeks_ago) {
            continue;
        }
        hits.push_back(song_path);
    }
    return hits;
}

void NavigationLoad::load_collection_new(const fs::path& path, const BoxDef& box_def) {
    std::vector<fs::path> hits;
    std::unordered_map<std::string, std::unique_ptr<SongParser>> preparsed;
    if (!ensure_index()) {
        return;
    }

    hits = find_new_songs(path);
    preparsed = parse_songs_parallel(hits, cancellation_token);

    const fs::path parent = path.parent_path();
    int songs_added = 0;
    for (const auto& h : hits) {
        if (abort_loading) {
            break;
        }
        if (songs_added > 0 && songs_added % 10 == 0) {
            enqueue_inline_box(make_back_box(parent, &inline_back_def));
        }
        auto song = make_song_box(h, box_def, take_parser(preparsed, h));
        apply_song_genre(song.get(), parse_box_def(parent / *h.lexically_relative(parent).begin()));
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
        songs_added++;
    }
}

void NavigationLoad::load_collection_difficulty(const fs::path& path, const BoxDef& box_def, int course, int level,
                                                int order) {
    struct Hit {
        fs::path path;
        fs::path sibling;
        bool unmet;
    };
    std::vector<Hit> hits;

    const int crown_needed = (order == 2)   ? static_cast<int>(Crown::CLEAR)
                             : (order == 3) ? static_cast<int>(Crown::FC)
                             : (order == 4) ? static_cast<int>(Crown::DFC)
                                            : 0;

    if (!ensure_index()) {
        return;
    }

    const fs::path parent = path.parent_path();
    for (const auto& song_path : index->filter_difficulty(course, level, &abort_loading)) {
        if (abort_loading) {
            break;
        }

        // the song must sit below one of the collection folder's siblings
        fs::path rel = song_path.lexically_relative(parent);
        if (rel.empty() || rel.begin()->string() == "..") {
            continue;
        }
        fs::path sibling = parent / *rel.begin();
        if (sibling == path || !fs::is_directory(sibling)) {
            continue;
        }

        bool unmet = false;
        if (crown_needed > 0) {
            unmet = true;
            const auto& hashes = scores_manager.get_hashes(song_path);
            std::string hash = hashes[std::min(course, 4)];
            if (!hash.empty()) {
                auto score = scores_manager.get_score(hash, course, player_1);
                if (score.has_value() && static_cast<int>(score->crown) >= crown_needed) {
                    unmet = false;
                }
            }
        }
        hits.push_back(Hit{song_path, sibling, unmet});
    }

    if (crown_needed > 0) {
        std::ranges::stable_partition(hits, [](const Hit& h) { return h.unmet; });
    }

    std::vector<fs::path> hit_paths;
    hit_paths.reserve(hits.size());
    for (const auto& h : hits) {
        hit_paths.push_back(h.path);
    }
    auto preparsed = parse_songs_parallel(hit_paths, cancellation_token);

    int songs_added = 0;
    for (const auto& h : hits) {
        if (abort_loading) {
            break;
        }
        if (songs_added > 0 && songs_added % 10 == 0) {
            enqueue_inline_box(make_back_box(parent, &inline_back_def));
        }
        auto song = make_song_box(h.path, box_def, take_parser(preparsed, h.path));
        apply_song_genre(song.get(), parse_box_def(h.sibling));
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
        songs_added++;
    }
}

void NavigationLoad::load_from_song_list(const fs::path& path, const BoxDef& box_def, bool mark_favorite) {
    int songs_added = 0;
    for (const auto& entry : read_song_list(path / "song_list.txt")) {
        if (abort_loading) {
            break;
        }
        fs::path song_path;
        if (auto found = scores_manager.get_path_by_hash(entry.hash)) {
            song_path = *found;
        }
        if (!entry.title.empty() || !entry.subtitle.empty()) {
            auto by_title = index->find_exact_title(entry.title, entry.subtitle);
            if (by_title && scores_manager.get_single_hash(*by_title) == entry.hash) {
                song_path = *by_title;
            }
        }
        if (song_path.empty()) {
            continue;
        }
        if (songs_added > 0 && songs_added % 10 == 0) {
            enqueue_inline_box(make_back_box(path.parent_path(), &inline_back_def));
        }
        auto song = make_song_box(song_path, box_def, SongParser(song_path));
        song->preserve_order = true;
        if (mark_favorite) {
            song->is_favorite = true;
        }
        fs::path genre_folder = navigation_scan::find_box_def_folder(song_path);
        if (!genre_folder.empty()) {
            apply_song_genre(song.get(), parse_box_def(genre_folder));
        }
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
        songs_added++;
    }
}

void NavigationLoad::load_collection_recommended(const fs::path& path, const BoxDef& box_def) {
    auto all_songs = index->songs_by_title(&abort_loading);

    std::mt19937 rng(std::random_device{}());
    std::shuffle(all_songs.begin(), all_songs.end(), rng);
    int count = std::min(static_cast<int>(all_songs.size()), 10);
    for (int i = 0; i < count; i++) {
        if (abort_loading) {
            break;
        }
        const fs::path& song_path = all_songs[i];
        auto song = make_song_box(song_path, box_def, SongParser(song_path));
        song->preserve_order = true;
        fs::path genre_folder = navigation_scan::find_box_def_folder(song_path);
        if (!genre_folder.empty()) {
            apply_song_genre(song.get(), parse_box_def(genre_folder));
        }
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
    }
}

void NavigationLoad::load_collection_search(const fs::path& path, const BoxDef& box_def) {
    auto hits = index->search(current_search, &abort_loading);
    auto preparsed = parse_songs_parallel(hits, cancellation_token);

    int songs_added = 0;
    for (const fs::path& song_path : hits) {
        if (abort_loading) {
            break;
        }
        if (songs_added > 0 && songs_added % 10 == 0) {
            enqueue_inline_box(make_back_box(path.parent_path(), &inline_back_def));
        }
        auto song = make_song_box(song_path, box_def, take_parser(preparsed, song_path));
        fs::path genre_folder = navigation_scan::find_box_def_folder(song_path);
        if (!genre_folder.empty()) {
            apply_song_genre(song.get(), parse_box_def(genre_folder));
        }
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
        songs_added++;
    }
}

void NavigationLoad::load_collection_play_count(const fs::path& path, const BoxDef& box_def) {
    std::unordered_map<fs::path, int> plays;
    for (const auto& [hash, count] : scores_manager.get_play_counts(player_1)) {
        if (auto song_path = scores_manager.get_path_by_diff_hash(hash)) {
            plays[*song_path] += count;
        }
    }
    std::vector<std::pair<fs::path, int>> hits(plays.begin(), plays.end());
    std::ranges::sort(hits, [](const auto& a, const auto& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });

    std::vector<fs::path> hit_paths;
    hit_paths.reserve(hits.size());
    for (const auto& h : hits) {
        hit_paths.push_back(h.first);
    }
    auto preparsed = parse_songs_parallel(hit_paths, cancellation_token);

    int songs_added = 0;
    for (const fs::path& song_path : hit_paths) {
        if (abort_loading) {
            break;
        }
        if (songs_added > 0 && songs_added % 10 == 0) {
            enqueue_inline_box(make_back_box(path.parent_path(), &inline_back_def));
        }
        auto song = make_song_box(song_path, box_def, take_parser(preparsed, song_path));
        song->preserve_order = true;
        fs::path genre_folder = navigation_scan::find_box_def_folder(song_path);
        if (!genre_folder.empty()) {
            apply_song_genre(song.get(), parse_box_def(genre_folder));
        }
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
        songs_added++;
    }
}

void NavigationLoad::scan_song_tree(const fs::path& path, std::vector<fs::path>& song_paths,
                                    std::unordered_map<std::string, std::vector<std::pair<bool, fs::path>>>& plan) {
    std::error_code ec;
    std::vector<std::pair<fs::path, std::string>> work; // (dir to list, top-level key)
    auto scan_dir = [&](const fs::path& dir, const std::string& key) {
        std::vector<fs::path> here, subdirs;
        bool is_osu = false;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
            if (abort_loading) {
                return;
            }
            const fs::directory_entry& e = *it;
            std::error_code tec;
            if (e.is_directory(tec)) {
                subdirs.push_back(e.path());
                continue;
            }
            if (!e.is_regular_file(tec)) {
                continue;
            }
            auto ext = e.path().extension();
            if (ext == ".osu") {
                is_osu = true;
            }
            if (ext == ".tja" || ext == ".osu") {
                here.push_back(e.path());
            }
        }
        ec.clear();
        if (is_osu) {
            plan[key].emplace_back(true, dir);
            return;
        } // one box per osu folder, no recursion
        for (const auto& f : here) {
            song_paths.push_back(f);
            plan[key].emplace_back(false, f);
        }
        for (const auto& d : subdirs) {
            work.emplace_back(d, key);
        }
    };
    for (fs::directory_iterator it(path, ec), end; !ec && it != end; it.increment(ec)) {
        if (abort_loading) {
            break;
        }
        const fs::directory_entry& e = *it;
        std::error_code tec;
        if (e.is_directory(tec)) {
            std::string key = e.path().string();
            plan[key];
            work.emplace_back(e.path(), key);
        } else if (e.is_regular_file(tec)) {
            auto ext = e.path().extension();
            if (ext == ".tja" || ext == ".osu") {
                song_paths.push_back(e.path());
            }
        }
    }
    ec.clear();
    for (size_t i = 0; i < work.size() && !abort_loading; i++) {
        auto [dir, key] = work[i];
        scan_dir(dir, key);
    }
}

void NavigationLoad::load_songs_inline_async(const fs::path& path, const BoxDef& box_def) {
    if (load_gen4_genre_songs(path, box_def)) {
        return;
    }
    if (load_gen3_genre_songs(path, box_def)) {
        return;
    }

    if (request.requires_index && !ensure_index()) {
        return;
    }

    int songs_added = 0;
    std::unordered_map<std::string, std::unique_ptr<SongParser>> preparsed;

    if (box_def.collection == "RECOMMENDED") {
        try {
            load_collection_recommended(path, box_def);
        } catch (const std::exception& e) {
            spdlog::error("Error loading recommended collection {}: {}", path.string(), e.what());
            throw;
        }
        return;
    }

    if (box_def.collection == "FAVORITE") {
        try {
            load_from_song_list(path, box_def, true);
        } catch (const std::exception& e) {
            spdlog::error("Error loading favorite collection {}: {}", path.string(), e.what());
            throw;
        }
        return;
    }

    if (box_def.collection == "RECENT") {
        try {
            load_from_song_list(path, box_def, false);
        } catch (const std::exception& e) {
            spdlog::error("Error loading recent collection {}: {}", path.string(), e.what());
            throw;
        }
        return;
    }

    if (box_def.collection == "DIFFICULTY") {
        if (diff_sort_filter) {
            try {
                load_collection_difficulty(path, box_def, (*diff_sort_filter)[0], (*diff_sort_filter)[1],
                                           (*diff_sort_filter)[2]);
            } catch (const std::exception& e) {
                spdlog::error("Error loading difficulty collection {}: {}", path.string(), e.what());
                throw;
            }
            diff_sort_filter.reset();
        }
        return;
    }

    if (box_def.collection == "NEW") {
        try {
            load_collection_new(path, box_def);
        } catch (const std::exception& e) {
            spdlog::error("Error loading new collection {}: {}", path.string(), e.what());
            throw;
        }
        return;
    }

    if (box_def.collection == "SEARCH") {
        try {
            load_collection_search(path, box_def);
        } catch (const std::exception& e) {
            spdlog::error("Error loading search collection {}: {}", path.string(), e.what());
            throw;
        }
        return;
    }

    if (box_def.collection == "PLAY_COUNT") {
        try {
            load_collection_play_count(path, box_def);
        } catch (const std::exception& e) {
            spdlog::error("Error loading play count collection {}: {}", path.string(), e.what());
            throw;
        }
        return;
    }

    std::vector<fs::path> song_paths;
    std::unordered_map<std::string, std::vector<std::pair<bool, fs::path>>> plan;
    scan_song_tree(path, song_paths, plan);
    preparsed = parse_songs_parallel(song_paths, cancellation_token);

    try {
        for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
            if (abort_loading) {
                break;
            }
            const fs::path& curr_path = entry.path();
            try {
                std::error_code tec;
                if (!entry.is_directory(tec)) {
                    if (curr_path.filename() == "song_list.txt") {
                        parse_song_list(curr_path, box_def, true);
                        continue;
                    }
                    if (is_song_file(curr_path)) {
                        enqueue_inline_box(make_song_box(curr_path, box_def, take_parser(preparsed, curr_path)));
                    }
                    continue;
                }
                auto pit = plan.find(curr_path.string());
                if (pit == plan.end()) {
                    continue; // not seen by the scan (removed meanwhile)
                }
                for (const auto& [is_osu, p] : pit->second) {
                    if (abort_loading) {
                        break;
                    }
                    try {
                        if (is_osu) {
                            BoxDef osu_box_def = box_def;
                            osu_box_def.name = p.filename().string();
                            auto folder = std::make_unique<PreparedFolder>(p, osu_box_def);
                            folder->fade_in(266);
                            enqueue_inline_box(std::move(folder));
                        } else {
                            if (songs_added > 0 && songs_added % 10 == 0) {
                                enqueue_inline_box(make_back_box(path.parent_path(), &inline_back_def));
                            }
                            auto box = make_song_box(p, box_def, take_parser(preparsed, p));
                            box->fade_in(266);
                            enqueue_inline_box(std::move(box));
                            songs_added++;
                        }
                    } catch (const std::exception& inner) {
                        spdlog::warn("Skipping song: {}", inner.what());
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("Skipping entry: {}, {}", e.what(), curr_path.string());
            }
        }
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Error loading directory: {}", e.what());
        throw;
    }
}

BoxDef NavigationLoad::parse_box_def(const fs::path& path) {
    return cached_navigation_box_def(box_def_cache, path, language);
}

BoxDef cached_navigation_box_def(std::unordered_map<std::string, BoxDef>& cache, const fs::path& path,
                                 const std::string& language) {
    const auto key = path.string();
    const auto cached = cache.find(key);

    if (cached != cache.end()) {
        return cached->second;
    }

    BoxDef definition = parse_navigation_box_def(path, language);
    cache.emplace(key, definition);
    return definition;
}

BoxDef parse_navigation_box_def(const fs::path& path, const std::string& language) {
    std::ifstream boxDef(path / "box.def");
    std::string line;
    BoxDef result;
    result.name = path.filename().string();
    result.texture_index = TextureIndex::DEFAULT;
    result.genre_index = GenreIndex::DEFAULT;
    result.collection = "";
    bool title_localized = false;
    std::array<bool, 3> explanation_localized = {false, false, false};

    while (std::getline(boxDef, line)) {
        if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }
        line = utf8_from_utf8_or_sjis(line); // box.def files from older simulators are Shift-JIS
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        auto get_value = [&](const std::string& prefix) -> std::string { return line.substr(prefix.size()); };

        if (line.starts_with("#GENRE:")) {
            std::string genre = get_value("#GENRE:");
            auto it = TEXTURE_MAP.find(genre);
            if (it != TEXTURE_MAP.end()) {
                result.texture_index = it->second;
            }
            result.genre_index = get_genre_index(genre);
            result.genre_label = genre;
        } else if (line.starts_with("#TITLE:")) {
            if (!title_localized) {
                result.name = get_value("#TITLE:");
            }
        } else if (line.starts_with("#TITLE")) {
            const std::string& lang = language;
            std::string lang_upper = lang;
            std::ranges::transform(lang_upper, lang_upper.begin(), ::toupper);
            std::string lang_prefix = "#TITLE" + lang_upper + ":";
            if (line.starts_with(lang_prefix)) {
                result.name = get_value(lang_prefix);
                title_localized = true;
            }
        } else if (line.starts_with("#COLLECTION:")) {
            result.collection = get_value("#COLLECTION:");
            auto it = TEXTURE_MAP.find(result.collection);
            if (it != TEXTURE_MAP.end()) {
                result.texture_index = it->second;
            }
            result.genre_index = get_genre_index(result.collection);
        } else if (line.starts_with("#BOXCOLOR:")) {
            result.box_color = parse_hex_color(get_value("#BOXCOLOR:"));
            result.texture_index = TextureIndex::NONE;
        } else if (line.starts_with("#BACKCOLOR:")) {
            result.back_color = parse_hex_color(get_value("#BACKCOLOR:"));
            result.texture_index = TextureIndex::NONE;
        } else if (line.starts_with("#FORECOLOR:")) {
            result.fore_color = parse_hex_color(get_value("#FORECOLOR:"));
        } else if (line.starts_with("#BOXEXPLANATION")) {
            for (int n = 0; n < 3; n++) {
                std::string plain_prefix = "#BOXEXPLANATION" + std::to_string(n + 1) + ":";
                if (line.starts_with(plain_prefix) && !explanation_localized[n]) {
                    result.explanation[n] = get_value(plain_prefix);
                }
            }
            const std::string& lang = language;
            std::string lang_upper = lang;
            std::ranges::transform(lang_upper, lang_upper.begin(), ::toupper);
            for (int n = 0; n < 3; n++) {
                std::string lang_prefix = "#BOXEXPLANATION" + lang_upper + std::to_string(n + 1) + ":";
                if (line.starts_with(lang_prefix)) {
                    result.explanation[n] = get_value(lang_prefix);
                    explanation_localized[n] = true;
                }
            }
        }
    }
    return result;
}

const BoxDef* NavigationLoad::box_def_for_genre(GenreIndex genre) {
    if (!genre_box_defs_built) {
        genre_box_defs_built = true;
        for (const fs::path& root : root_paths) {
            std::error_code ec;
#ifdef SUPPORT_FUMEN
            if (!gen4::find_data_root(root).empty() || !gen3::find_data_root(root).empty()) {
                continue;
            }
#endif
            if (!fs::is_directory(root, ec)) {
                continue;
            }
            for (const auto& entry : fs::directory_iterator(root, ec)) {
                if (abort_loading) {
                    return nullptr;
                }
                if (!fs::is_directory(entry.path(), ec)) {
                    continue;
                }
                if (navigation_scan::is_gen4_root(entry.path()) ||
                    !navigation_scan::gen3_root_at(entry.path()).empty()) {
                    continue;
                }
                if (!folder_scanner.has_def_file(entry.path())) {
                    continue;
                }
                BoxDef def = parse_box_def(entry.path());
                genre_box_defs.emplace(def.genre_index, def);
            }
        }
    }
    auto it = genre_box_defs.find(genre);
    return it != genre_box_defs.end() ? &it->second : nullptr;
}

bool NavigationLoad::only_gen4_songs() {
#ifdef SUPPORT_FUMEN
    int games = 0;
    std::error_code ec;
    for (const fs::path& root : root_paths) {
        if (!gen4::find_data_root(root).empty() || !gen3::find_data_root(root).empty()) {
            games++;
            continue;
        }
        if (!fs::is_directory(root, ec)) {
            continue;
        }
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (navigation_scan::is_gen4_root(entry.path()) || !navigation_scan::gen3_root_at(entry.path()).empty()) {
                games++;
                continue;
            }
            return false;
        }
    }
    return games == 1;
#else
    return false;
#endif
}

void NavigationLoad::load_gen4_genres(const fs::path& data_root) {
#ifdef SUPPORT_FUMEN
    const gen4::Library* library = gen4::library_for(data_root);
    if (!library) {
        return;
    }

    const std::string& lang = language;
    for (int genre_no : gen4::genres_present(*library)) {
        if (abort_loading) {
            break;
        }
        GenreIndex genre = (GenreIndex)gen4::genre_index_for(genre_no);

        BoxDef def;
        if (const BoxDef* like = box_def_for_genre(genre)) {
            def = *like;
        } else {
            def.texture_index = (genre == GenreIndex::VOCALOID) ? TextureIndex::VOCALOID : TextureIndex::NONE;
            if (auto colors = DEFAULT_COLORS.find(genre); colors != DEFAULT_COLORS.end()) {
                def.back_color = colors->second[0];
                def.fore_color = colors->second[1];
            }
        }
        def.name = gen4::genre_name(genre_no, lang);
        def.genre_index = genre;
        auto folder = std::make_unique<PreparedFolder>(gen4::genre_path(data_root, genre_no), def);
        folder->tja_count = 0;
        for (const gen4::OrderEntry& listing : library->order()) {
            if (listing.genre_no == genre_no) {
                folder->tja_count++;
            }
        }
        enqueue_box(std::move(folder));
    }
#else
    (void)data_root;
#endif
}

bool NavigationLoad::load_gen4_genre_songs(const fs::path& path, const BoxDef& box_def) {
#ifdef SUPPORT_FUMEN
    int genre_no = gen4::genre_of_path(path);
    if (genre_no < 0) {
        return false;
    }

    fs::path data_root = gen4::find_data_root(path);
    const gen4::Library* library = gen4::library_for(data_root);
    if (!library) {
        return false;
    }

    GenreIndex genre = (GenreIndex)gen4::genre_index_for(genre_no);

    BoxDef def = box_def;
    if (const BoxDef* like = box_def_for_genre(genre)) {
        def = *like;
    } else if (auto colors = DEFAULT_COLORS.find(genre); colors != DEFAULT_COLORS.end()) {
        def.texture_index = (genre == GenreIndex::VOCALOID) ? TextureIndex::VOCALOID : TextureIndex::NONE;
        def.back_color = colors->second[0];
        def.fore_color = colors->second[1];
    }
    def.name = box_def.name;
    def.genre_index = genre;

    int songs_added = 0;
    for (const gen4::OrderEntry& listing : library->order()) {
        if (abort_loading) {
            break;
        }
        if (listing.genre_no != genre_no) {
            continue;
        }
        fs::path song_folder = data_root / "fumen" / listing.id;
        std::error_code ec;
        if (!fs::is_directory(song_folder, ec)) {
            continue;
        }
        if (songs_added > 0 && songs_added % 10 == 0) {
            enqueue_inline_box(make_back_box(data_root, &inline_back_def));
        }
        auto box = make_song_box(song_folder, def, SongParser(song_folder));
        box->preserve_order = true;
        box->fade_in(266);
        enqueue_inline_box(std::move(box));
        songs_added++;
    }
    return true;
#else
    (void)path;
    (void)box_def;
    return false;
#endif
}

bool NavigationLoad::is_gen3_song_folder(const fs::path& path) {
#ifdef SUPPORT_FUMEN
    if (path.parent_path().filename() != "fumen") {
        return false;
    }
    const gen3::Library* library = gen3::library_for(path);
    return library && library->find(path.filename().string()) != nullptr;
#else
    (void)path;
    return false;
#endif
}

void NavigationLoad::load_gen3_genres(const fs::path& data_root) {
#ifdef SUPPORT_FUMEN
    const gen3::Library* library = gen3::library_for(data_root);
    if (!library) {
        return;
    }

    for (const std::string& genre_name : gen3::genres_present(*library)) {
        if (abort_loading) {
            break;
        }
        GenreIndex genre = (GenreIndex)gen3::genre_index_for(genre_name);

        BoxDef def;
        if (const BoxDef* like = box_def_for_genre(genre)) {
            def = *like;
        } else {
            def.texture_index = (genre == GenreIndex::VOCALOID) ? TextureIndex::VOCALOID : TextureIndex::NONE;
            if (auto colors = DEFAULT_COLORS.find(genre); colors != DEFAULT_COLORS.end()) {
                def.back_color = colors->second[0];
                def.fore_color = colors->second[1];
            }
        }
        def.name = genre_name;
        def.genre_index = genre;
        auto folder = std::make_unique<PreparedFolder>(gen3::genre_path(data_root, genre_name), def);
        folder->tja_count = 0;
        for (const gen3::SongEntry& e : library->songs()) {
            if (e.genre == genre_name) {
                folder->tja_count++;
            }
        }
        enqueue_box(std::move(folder));
    }
#else
    (void)data_root;
#endif
}

bool NavigationLoad::load_gen3_genre_songs(const fs::path& path, const BoxDef& box_def) {
#ifdef SUPPORT_FUMEN
    std::string genre_name = gen3::genre_of_path(path);
    if (genre_name.empty()) {
        return false;
    }

    fs::path data_root = gen3::find_data_root(path);
    const gen3::Library* library = gen3::library_for(data_root);
    if (!library) {
        return false;
    }

    GenreIndex genre = (GenreIndex)gen3::genre_index_for(genre_name);

    BoxDef def = box_def;
    if (const BoxDef* like = box_def_for_genre(genre)) {
        def = *like;
    } else if (auto colors = DEFAULT_COLORS.find(genre); colors != DEFAULT_COLORS.end()) {
        def.texture_index = (genre == GenreIndex::VOCALOID) ? TextureIndex::VOCALOID : TextureIndex::NONE;
        def.back_color = colors->second[0];
        def.fore_color = colors->second[1];
    }
    def.name = box_def.name;
    def.genre_index = genre;

    int songs_added = 0;
    for (const gen3::SongEntry& listing : library->songs()) {
        if (abort_loading) {
            break;
        }
        if (listing.genre != genre_name) {
            continue;
        }
        fs::path song_folder = data_root / "fumen" / listing.id;
        std::error_code ec;
        if (!fs::is_directory(song_folder, ec)) {
            continue;
        }
        if (songs_added > 0 && songs_added % 10 == 0) {
            enqueue_inline_box(make_back_box(data_root, &inline_back_def));
        }
        auto box = make_song_box(song_folder, def, SongParser(song_folder));
        // The file's own order is the game's order.
        box->preserve_order = true;
        box->fade_in(266);
        enqueue_inline_box(std::move(box));
        songs_added++;
    }
    return true;
#else
    (void)path;
    (void)box_def;
    return false;
#endif
}

bool NavigationLoad::is_gen4_song_folder(const fs::path& path) {
#ifdef SUPPORT_FUMEN
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
        return false;
    }

    std::string id = path.filename().string();
    static const char* suffixes[] = {"_e", "_n", "_h", "_m", "_x"};
    bool has_chart = false;
    for (const char* suffix : suffixes) {
        if (fs::exists(path / (id + suffix + ".bin"), ec)) {
            has_chart = true;
            break;
        }
    }
    if (!has_chart) {
        return false;
    }

    return gen4::library_for(path) != nullptr;
#else
    (void)path;
    return false;
#endif
}

void prepare_navigation(const LoadRequest& request, const std::stop_token& stop,
                        const std::function<void(PreparedEntry)>& emit) {
    NavigationLoad(request, stop, emit).run();
}

PreparedSong::PreparedSong(const fs::path& path, BoxDef def, SongParser parser)
    : PreparedPresentation(path, std::move(def)), parser(std::move(parser)) {
    hashes = scores_manager.get_hashes(path);

#ifdef SUPPORT_FUMEN
    if (std::holds_alternative<FumenParser>(this->parser.impl)) {
        return;
    }
#endif

    bool generated_hashes = false;

    for (const auto& [course, data] : this->parser.metadata.course_data) {
        if (course >= 0 && course < static_cast<int>(hashes.size()) && hashes[course].empty()) {
            hashes[course] = this->parser.get_diff_hash(course);
            generated_hashes |= !hashes[course].empty();
        }
    }

    if (generated_hashes) {
        scores_manager.add_path_binding(path, hashes);
    }
}

static void prepare_folder_summaries(const LoadRequest& request, const std::stop_token& stop,
                                     const std::function<void(PreparedEntry)>& emit) {
    for (const auto& path : request.summary_paths) {
        if (stop.stop_requested()) {
            return;
        }

        try {
            auto summary = scan_folder(path, request.player_1, request.player_2, stop);

            if (!stop.stop_requested()) {
                emit(PreparedFolderSummary{path, std::move(summary), request.score_revision});
            }
        } catch (const std::exception& e) {
            if (!stop.stop_requested()) {
                emit(PreparedFolderSummaryFailure{path, e.what(), request.score_revision});
            }
        } catch (...) {
            if (!stop.stop_requested()) {
                emit(PreparedFolderSummaryFailure{path, "Unknown folder scan error", request.score_revision});
            }
        }
    }
}
