#include "navigator.h"
#include "box_song_osu.h"
#include "box_back.h"
#include "color_utils.h"
#include "../song_select_script.h"
#include "../../../libs/filesystem.h"
#include "../../../libs/gen4.h"
#include "../../../libs/green.h"
#include <random>
#include <cmath>

static std::unique_ptr<SongBox> make_song_box(const fs::path& path, const BoxDef& box_def, SongParser parser) {
    if (path.extension() == ".osu")
        return std::make_unique<SongBoxOsu>(path, box_def, std::move(parser));
    return std::make_unique<SongBox>(path, box_def, std::move(parser));
}

static std::unordered_map<std::string, std::unique_ptr<SongParser>>
parse_songs_parallel(const std::vector<fs::path>& paths, std::atomic<bool>& abort_flag) {
    std::vector<std::unique_ptr<SongParser>> parsed(paths.size());
    unsigned workers = std::clamp(std::thread::hardware_concurrency(), 2u, 8u);
    std::atomic<size_t> next{0};
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < workers && t < paths.size(); t++) {
        pool.emplace_back([&] {
            for (size_t i; (i = next.fetch_add(1)) < paths.size();) {
                if (abort_flag.load()) break;
                try {
                    parsed[i] = std::make_unique<SongParser>(paths[i]);
                } catch (const std::exception& e) {
                    spdlog::warn("Failed to parse {}: {}", paths[i].string(), e.what());
                }
            }
        });
    }
    for (auto& th : pool) th.join();

    std::unordered_map<std::string, std::unique_ptr<SongParser>> out;
    for (size_t i = 0; i < paths.size(); i++)
        if (parsed[i]) out.emplace(paths[i].string(), std::move(parsed[i]));
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

static std::unique_ptr<BackBox> make_back_box(const fs::path& parent_path) {
    BoxDef d;
    d.back_color    = BackBox::COLOR;
    d.fore_color    = BackBox::COLOR;
    d.texture_index = TextureIndex::NONE;
    d.genre_index   = GenreIndex::NAMCO;
    return std::make_unique<BackBox>(parent_path, d);
}

static bool alpha_less(const std::string& a, const std::string& b) {
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = std::tolower(static_cast<unsigned char>(a[i]));
        unsigned char cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb) return ca < cb;
    }
    return a.size() < b.size();
}

// Record where the song really lives. A song listed inside a collection is
// built from that collection's BoxDef, so the box keeps the collection's
// genre on purpose - its colours and the wheel background stay the
// collection's - but the game screen's genre label needs the song's own.
static void apply_song_genre(SongBox* song, const BoxDef& box_def) {
    song->song_genre_index = box_def.genre_index;
    if (box_def.fore_color.has_value())
        song->fore_color = box_def.fore_color;
    else if (box_def.back_color.has_value())
        song->fore_color = darken_color(box_def.back_color.value());
}

namespace ch = std::chrono;

Navigator::Navigator() {
}

Navigator::~Navigator() {
    join_loader();
    if (song_files_thread.joinable())
        song_files_thread.join();
}

void Navigator::wait_for_song_files() {
    if (song_files_thread.joinable())
        song_files_thread.join();
}

void Navigator::load_all_roots() {
    join_loader();
    // Everything on the wheel is going away, so the genre band and inline
    // listing that pointed into it go too - draw would otherwise index boxes
    // that no longer exist. Coming back out of a game's genre is exactly
    // this: the band is still up when the rebuild starts.
    is_inline = false;
    inline_state.reset();
    pending_inline_path.reset();
    pending_inline_folder = nullptr;
    genre_bg.reset();
    genre_bg_end_pos.reset();
    inline_streaming = false;
    items.clear();
    open_index = 0;
    def_file_cache.clear();
    box_def_cache.clear();
    is_processing    = true;
    loading_complete = false;
    reloading_roots  = true;

    auto walk_roots = [this] {
        for (const fs::path& root : root_paths) {
            if (abort_loading) break;
            loading_complete = false;
            load_current_directory_async(root);
        }
        reloading_roots  = false;
        loading_complete = true;
        // With every level of the wheel up, catch up on the folder scans the
        // boxes put off - crowns and counts fill in as each one finishes.
        FolderBox::run_deferred_scans(abort_loading);
    };

#ifndef __EMSCRIPTEN__
    loader_thread = std::thread(walk_roots);
#else
    walk_roots();
#endif
}

void Navigator::preload(std::vector<fs::path> songs_paths) {
    if (is_preloaded) return;
    root_paths = songs_paths;
    open_index = 0;

#ifndef __EMSCRIPTEN__
    song_files_thread = std::thread([this, songs_paths]() {
        // The walk itself is quick now that game data roots are stepped over;
        // the cost is opening and parsing every chart for its title. That is
        // embarrassingly parallel, so a small pool splits the library.
        std::vector<fs::path> files = get_song_files(songs_paths);
        std::mutex map_mutex;
        std::atomic<size_t> cursor{0};
        unsigned pool_size = std::max(2u, std::thread::hardware_concurrency() / 2);
        std::vector<std::thread> pool;
        for (unsigned t = 0; t < pool_size; t++) {
            pool.emplace_back([&]() {
                for (;;) {
                    size_t i = cursor.fetch_add(1);
                    if (i >= files.size() || abort_loading) break;
                    const fs::path& file = files[i];
                    if (!is_song_file(file)) continue;
                    try {
                        SongParser parsed_entry = SongParser(file);
                        parsed_entry.get_metadata();
                        // Dan (course 6) and Tower (course 5) charts are played
                        // from their own screens, never from a song box: they
                        // have no normal difficulty to select and picking one
                        // out of a collection breaks. Keep them out of the
                        // index the collections draw from.
                        bool playable = false;
                        for (const auto& [course, data] : parsed_entry.metadata.course_data)
                            if (course >= 0 && course <= 4) { playable = true; break; }
                        if (playable) {
                            std::lock_guard<std::mutex> lock(map_mutex);
                            song_files[{parsed_entry.metadata.title["en"], parsed_entry.metadata.subtitle["en"]}] = file;
                        }
                    } catch (const std::exception& inner) {
                        spdlog::warn("Skipping song during scan: {}", inner.what());
                    }
                }
            });
        }
        for (std::thread& worker : pool) worker.join();
    });
#endif // !__EMSCRIPTEN__

    for (const fs::path& root_path : songs_paths) {
        for (const auto& entry : fs::directory_iterator(root_path)) {
            if (!fs::is_directory(entry) || !has_def_file(entry.path())) continue;
            BoxDef bd = parse_box_def(entry.path());
            if (bd.collection == "RECENT" && !recent_folder_path)
                recent_folder_path = entry.path();
            if (bd.collection == "FAVORITE" && !favorite_folder_path) {
                favorite_folder_path = entry.path();
                for (const auto& e : read_song_list(entry.path() / "song_list.txt"))
                    favorite_songs.insert(e.title + "|" + e.subtitle);
            }
        }
    }
    is_preloaded = true;
}

void Navigator::init(std::vector<fs::path> songs_paths) {
    if (is_init && hide_dan != built_hide_dan) {
        join_loader();
        is_inline = false;
        inline_state.reset();
        pending_inline_path.reset();
        pending_inline_folder = nullptr;
        genre_bg.reset();
        genre_bg_end_pos.reset();
        awaiting_diff_sort = false;
        diff_sort_filter.reset();
        reopen_folder_path.reset();
        reopen_song_path.reset();
        items.clear();
        open_index = 0;
        is_init = false;
    }
    built_hide_dan = hide_dan;

    if (!is_init) {
        if (!is_preloaded) {
            preload(songs_paths);
        }

        load_all_roots();
        is_init = true;
    } else {
        if (global_data.config->general.song_limit > 0) {
            join_loader();
            is_inline = false;
            inline_state.reset();
            pending_inline_path.reset();
            pending_inline_folder = nullptr;
            genre_bg.reset();
            awaiting_diff_sort = false;
            diff_sort_filter.reset();
            load_all_roots();
        } else {
            bool from_result = global_data.returned_from_result;
            global_data.returned_from_result = false;
            if (from_result && inline_state.has_value()) {
                if (open_index >= 0 && open_index < (int)items.size()) {
                    if (dynamic_cast<SongBox*>(items[open_index].get()))
                        reopen_song_path = items[open_index]->path;
                }
                reopen_folder_path = inline_state->saved_folder_box
                                   ? std::optional<fs::path>(inline_state->saved_folder_box->path)
                                   : std::nullopt;
                if (!reopen_folder_path) reopen_song_path.reset();
                collapse_inline_now();
            }
            for (auto& item : items) {
                item->reset();
                item->fade_in(0);
            }
            if (pending_inline_folder != nullptr) {
                pending_inline_folder->reset();
                pending_inline_folder->expand_box();
            }
            set_positions(false, 0);
            if (!items.empty()) get_current_item()->expand_box();
        }
    }
    vertical_gallery = tex.options.count(SCO::HORIZONTAL_SONG_SELECT) && tex.options.at(SCO::HORIZONTAL_SONG_SELECT);
    background_move = (MoveAnimation*)tex.get_animation(0);
    background_fade_change = (FadeAnimation*)tex.get_animation(5);
    bg_genre_index = items.empty() ? GenreIndex::TUTORIAL : items[open_index]->genre_index;
    last_bg_genre_index = bg_genre_index;
    if (genre_bg.has_value()) genre_bg->fade_in();
}

void Navigator::collapse_inline_now() {
    if (!inline_state.has_value()) return;
    join_loader();

    InlineState& state = *inline_state;
    int end = state.first_song_index + state.songs_count;
    if (state.first_song_index < 0 || end > (int)items.size() ||
        state.folder_index < 0 || state.folder_index >= (int)items.size() ||
        !state.saved_folder_box) {
        return;
    }

    items.erase(items.begin() + state.first_song_index, items.begin() + end);
    items.erase(items.begin() + state.folder_index);
    items.insert(items.begin() + state.folder_index, std::move(state.saved_folder_box));
    open_index = state.folder_index;
    items[open_index]->exit_box();

    {
        std::lock_guard<std::mutex> lock(pending_mutex);
        std::queue<std::unique_ptr<BaseBox>>().swap(pending_inline_boxes);
    }

    inline_state.reset();
    pending_inline_folder = nullptr;
    pending_inline_path.reset();
    genre_bg.reset();
    genre_bg_end_pos.reset();
    is_inline = false;
    is_processing = false;
    inline_streaming = false;
    loading_complete = false;
}

void Navigator::join_loader() {
    abort_loading = true;
    if (loader_thread.joinable())
        loader_thread.join();
    abort_loading = false;
}

void Navigator::enqueue_box(std::unique_ptr<BaseBox> box) {
    // Not every box stands for something on disk: a gen 4 genre is a property
    // of a song, not a folder, so it has no file time and simply is not new.
    std::error_code ec;
    auto last_write = fs::last_write_time(box->path, ec);
    if (!ec) {
        auto last_write_sys = ch::system_clock::now() +
            std::chrono::duration_cast<ch::system_clock::duration>(
                last_write - std::filesystem::file_time_type::clock::now());
        auto two_weeks_ago = ch::system_clock::now() - ch::weeks(2);
        if (last_write_sys < two_weeks_ago)
            box->is_new = true;
    }

    std::lock_guard<std::mutex> lock(pending_mutex);
    if (auto* song = dynamic_cast<SongBox*>(box.get())) {
        auto& t = song->parser.metadata.title;
        auto& s = song->parser.metadata.subtitle;
        std::string key = (t.count("en") ? t.at("en") : t.begin()->second)
                        + "|"
                        + (s.count("en") ? s.at("en") : s.begin()->second);
        if (favorite_songs.count(key)) song->is_favorite = true;
    }
    pending_boxes.push(std::move(box));
}

void Navigator::enqueue_inline_box(std::unique_ptr<BaseBox> box) {
    std::lock_guard<std::mutex> lock(pending_mutex);
    if (auto* song = dynamic_cast<SongBox*>(box.get())) {
        auto& t = song->parser.metadata.title;
        auto& s = song->parser.metadata.subtitle;
        std::string key = (t.count("en") ? t.at("en") : t.begin()->second)
                        + "|"
                        + (s.count("en") ? s.at("en") : s.begin()->second);
        if (favorite_songs.count(key)) song->is_favorite = true;
    }
    pending_inline_boxes.push(std::move(box));
}

void sort_items(std::vector<std::unique_ptr<BaseBox>>& items, int first_index, int last_index) {
    if (first_index >= 0 && last_index > first_index &&
        last_index < static_cast<int>(items.size())) {

        auto begin = items.begin() + first_index;
        auto end   = items.begin() + last_index + 1;

        std::vector<int> back_box_positions;
        std::vector<std::unique_ptr<BaseBox>> sortable;

        for (int i = first_index; i <= last_index; i++) {
            if (items[i]->text_name == "BACK_BOX" || items[i]->preserve_order) {
                back_box_positions.push_back(i - first_index);
            } else {
                sortable.push_back(std::move(items[i]));
            }
        }

        std::sort(sortable.begin(), sortable.end(),
            [](const std::unique_ptr<BaseBox>& a, const std::unique_ptr<BaseBox>& b) {
                return alpha_less(a->text_name, b->text_name);
            });

        int sortable_idx = 0;
        for (int i = first_index; i <= last_index; i++) {
            int relative = i - first_index;
            if (std::find(back_box_positions.begin(), back_box_positions.end(), relative)
                != back_box_positions.end()) {
                // BACK_BOX slot — item was never moved, nothing to do
            } else {
                items[i] = std::move(sortable[sortable_idx++]);
            }
        }
    }
}

void Navigator::refresh_scores() {
    FolderBox::invalidate_scan_cache();
    SongBox* curr_item = dynamic_cast<SongBox*>(get_current_item());
    if (curr_item) {
        curr_item->refresh_scores();
    }
    if (pending_inline_folder) {
        pending_inline_folder->refresh_scores(song_files);
    }
}

void Navigator::flush_pending_boxes() {
    std::lock_guard<std::mutex> lock(pending_mutex);

    while (!pending_boxes.empty()) {
        items.push_back(std::move(pending_boxes.front()));
        pending_boxes.pop();
    }

    if (inline_state.has_value()) {
        if (!pending_inline_boxes.empty()) {
            std::vector<std::unique_ptr<BaseBox>> batch;
            while (!pending_inline_boxes.empty()) {
                batch.push_back(std::move(pending_inline_boxes.front()));
                pending_inline_boxes.pop();
            }
            int insert_pos = inline_state->first_song_index + inline_state->songs_count;
            items.insert(items.begin() + insert_pos,
                         std::make_move_iterator(batch.begin()),
                         std::make_move_iterator(batch.end()));
            inline_state->songs_count += (int)batch.size();
        }
        genre_bg_start = inline_state->first_song_index - 1;
        genre_bg_end = inline_state->first_song_index + inline_state->songs_count - 1;
    }

    if (loading_complete.load()) {
        if (open_index >= items.size()) open_index = 0;
        if (inline_state.has_value()) {
            sort_items(items, genre_bg_start, genre_bg_end);
        } else if (items.size() > 1) {
            std::vector<int> sortable_indices;
            std::vector<std::unique_ptr<BaseBox>> sortable;
            int first_sortable = dynamic_cast<BackBox*>(items[0].get()) ? 1 : 0;
            for (int i = first_sortable; i < (int)items.size(); i++) {
                if (!items[i]->preserve_order) {
                    sortable_indices.push_back(i);
                    sortable.push_back(std::move(items[i]));
                }
            }
            std::sort(sortable.begin(), sortable.end(),
                [](const std::unique_ptr<BaseBox>& a, const std::unique_ptr<BaseBox>& b) {
                    return alpha_less(a->path.filename().string(), b->path.filename().string());
                });
            for (int j = 0; j < (int)sortable_indices.size(); j++)
                items[sortable_indices[j]] = std::move(sortable[j]);
        }
        // A rebuilt wheel starts the cursor at zero; put it back on the box
        // it was on when the rebuild began.
        if (restore_cursor_path && !inline_state.has_value()) {
            for (int i = 0; i < (int)items.size(); i++)
                if (items[i]->path == *restore_cursor_path) { open_index = i; break; }
            restore_cursor_path.reset();
        }

        if (inline_state.has_value() && reopen_song_path && reopen_folder_path) {
            const auto& folder = inline_state->saved_folder_box;
            if (folder && folder->path == *reopen_folder_path) {
                int first = inline_state->first_song_index;
                int end   = std::min(first + inline_state->songs_count, (int)items.size());
                for (int i = first; i < end; i++) {
                    if (items[i]->path == *reopen_song_path) { open_index = i; break; }
                }
            }
            reopen_folder_path.reset();
            reopen_song_path.reset();
        }

        set_positions(false, 800);
        if (!items.empty()) items[open_index]->expand_box();
        is_processing = false;
        inline_streaming = false;
        loading_complete = false;
    }
}

void Navigator::parse_song_list(const fs::path& path, BoxDef box_def, bool inline_mode) {
    auto entries = read_song_list(path);
    std::vector<SongListEntry> out_entries;
    bool needs_rewrite = false;
    int songs_added = 0;

    for (const auto& entry : entries) {
        fs::path final_path;

        if (auto found = scores_manager.get_path_by_hash(entry.hash)) {
            final_path = *found;
            out_entries.push_back(entry);
        } else {
            auto song_path_opt = find_song_by_title(entry.title, entry.subtitle);
            if (!song_path_opt) {
                out_entries.push_back(entry);
                spdlog::warn("No song found for: {} | {}", entry.title, entry.subtitle);
                continue;
            }
            final_path = *song_path_opt;
            std::string correct_hash = scores_manager.get_single_hash(final_path);
            out_entries.push_back({correct_hash, entry.title, entry.subtitle});
            spdlog::info("Found song: {} | {} with hash {}", entry.title, entry.subtitle, correct_hash);
            needs_rewrite = true;
        }

        auto box = make_song_box(final_path, box_def, SongParser(final_path));
        box->preserve_order = true;
        if (songs_added > 0 && songs_added % 10 == 0)
            enqueue_inline_box(make_back_box(path.parent_path().parent_path()));
        if (inline_mode)
            enqueue_inline_box(std::move(box));
        else
            enqueue_box(std::move(box));
        songs_added++;
    }

    if (needs_rewrite)
        write_song_list(path, out_entries);
}

void Navigator::load_current_directory_async(const fs::path path) {
    // Not waiting for the song index here: the wheel's boxes need nothing
    // from it, and waiting meant the whole wheel sat empty behind the
    // library-wide metadata scan. The one thing below that does need it -
    // a song_list.txt collection - waits for itself.
    BoxDef box_def = parse_box_def(path);

    setup_back_box(path, true);

    // A song path pointing at a game, either at its data root or at the fumen
    // folder inside it. While the roots are being walked this is a level of
    // the wheel like any other, so it gets a box to open - unless the game is
    // all there is, when its genres are the wheel itself. Opening that box
    // comes back here with reloading_roots clear and lists the genres.
    fs::path own_root = gen4::find_data_root(path);
    bool own_root_is_green = false;
    if (own_root.empty() || !gen4::library_for(own_root)) {
        fs::path green_root = green::find_data_root(path);
        if (!green_root.empty() && green::library_for(green_root)) {
            own_root = green_root;
            own_root_is_green = true;
        }
    }
    bool is_a_root = std::find(root_paths.begin(), root_paths.end(), path) != root_paths.end();
    if (!own_root.empty() && is_a_root &&
        (own_root_is_green || gen4::library_for(own_root))) {
        if (reloading_roots && !only_gen4_songs()) {
            BoxDef gen4_def = box_def;
            gen4_def.name        = own_root.filename().string();
            gen4_def.genre_index = GenreIndex::DEFAULT;
            enqueue_box(std::make_unique<FolderBox>(own_root, gen4_def, song_files));
            loading_complete = true;
            current_path = path;
            return;
        }
        // setup_back_box gives a root level no way out, since a root has no
        // folder above it. This one does: the rest of the library. The box
        // points at another root, and opening a root rebuilds them all.
        for (const fs::path& root : root_paths) {
            if (!gen4::find_data_root(root).empty()) continue;
            items.push_back(make_back_box(root));
            break;
        }
        try {
            if (own_root_is_green) load_green_genres(own_root);
            else                   load_gen4_genres(own_root);
        } catch (const std::exception& e) {
            spdlog::error("Error listing arcade genres of {}: {}", own_root.string(), e.what());
        }
        loading_complete = true;
        current_path = path;
        return;
    }

    // A PS3 game nests its data as <game>/USRDIR/data; the back box the
    // generic setup pointed at USRDIR would just show this same game again,
    // so it leads out beside the game's folder instead.
    if (is_green_root(path) && path.filename() == "data" &&
        path.parent_path().filename() == "USRDIR" && !items.empty()) {
        if (auto* back = dynamic_cast<BackBox*>(items.front().get()))
            back->path = path.parent_path().parent_path().parent_path();
    }

    // A data root lists its own genres and nothing else, so the songs of this
    // game replace the ones that were on the wheel until the back box is used.
    if (is_gen4_root(path) || is_green_root(path)) {
        try {
            if (is_green_root(path)) load_green_genres(path);
            else                     load_gen4_genres(path);
        } catch (const std::exception& e) {
            spdlog::error("Error listing gen4 genres of {}: {}", path.string(), e.what());
        } catch (...) {
            spdlog::error("Unknown error listing gen4 genres of {}", path.string());
        }
        loading_complete = true;
        current_path = path;
        return;
    }

    // Pre-parse the level's song files on a worker pool (see
    // parse_songs_parallel) so the streaming loop below only assembles boxes.
    std::vector<fs::path> song_paths;
    try {
        for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
            if (abort_loading) break;
            if (!fs::is_directory(entry.path()) && is_song_file(entry.path()))
                song_paths.push_back(entry.path());
        }
    } catch (const fs::filesystem_error&) { /* main loop reports errors */ }
    auto preparsed = parse_songs_parallel(song_paths, abort_loading);

    try {
        for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
            if (abort_loading) break;
            const fs::path& curr_path = entry.path();
            try {
                if (!fs::is_directory(curr_path)) {
                    if (curr_path.filename() == "song_list.txt") {
                        wait_for_song_files();
                        BoxDef entry_box_def = parse_box_def(curr_path);
                        parse_song_list(curr_path, entry_box_def, false);
                        continue;
                    }
                    if (is_song_file(curr_path))
                        enqueue_box(make_song_box(curr_path, box_def, take_parser(preparsed, curr_path)));
                    continue;
                }
                fs::path groot = green_root_at(curr_path);
                if (is_gen4_root(curr_path) || !groot.empty()) {
                    if (only_gen4_songs()) {
                        if (!groot.empty()) load_green_genres(groot);
                        else                load_gen4_genres(curr_path);
                        continue;
                    }
                    BoxDef gen4_def = box_def;
                    gen4_def.name = curr_path.filename().string();
                    // The game says what it is called; the folder is often
                    // just a serial like SCEEXE009.
                    if (!groot.empty())
                        if (const green::Library* lib = green::library_for(groot))
                            gen4_def.name = lib->game_name();
                    gen4_def.genre_index = GenreIndex::DEFAULT;
                    enqueue_box(std::make_unique<FolderBox>(groot.empty() ? curr_path : groot,
                                                            gen4_def, song_files));
                    continue;
                }
                if (is_green_song_folder(curr_path)) {
                    enqueue_box(make_song_box(curr_path, box_def, SongParser(curr_path)));
                    continue;
                }
                if (is_gen4_song_folder(curr_path)) {
                    enqueue_box(make_song_box(curr_path, box_def, SongParser(curr_path)));
                    continue;
                }
                if (has_def_file(curr_path)) {
                    BoxDef entry_box_def = parse_box_def(curr_path);
                    // Practice has no dan mode to enter, so the dojo is left
                    // out of the wheel rather than leading somewhere that
                    // cannot honour it.
                    if (hide_dan && entry_box_def.genre_index == GenreIndex::DAN)
                        continue;
                    auto folder = std::make_unique<FolderBox>(curr_path, entry_box_def, song_files);
                    if (entry_box_def.collection == "RECOMMENDED")
                        folder->tja_count = 10;
                    enqueue_box(std::move(folder));
                } else if (is_osu_song_folder(curr_path)) {
                    BoxDef osu_box_def = box_def;
                    auto it = fs::directory_iterator(curr_path);
                    auto end = fs::end(it);
                    while (it != end && it->path().extension() != ".osu")
                        ++it;
                    if (it == end) continue;
                    OsuParser title_parser = OsuParser(it->path());
                    title_parser.get_metadata();
                    osu_box_def.name = title_parser.metadata.title[global_data.config->general.language];
                    std::unique_ptr<FolderBox> osu_folder = std::make_unique<FolderBox>(curr_path, osu_box_def, song_files);
                    osu_folder->is_osu_folder = true;
                    enqueue_box(std::move(osu_folder));
                } else {
                    std::error_code ec;
                    auto it = fs::recursive_directory_iterator(curr_path, ec);
                    while (it != fs::end(it)) {
                        if (abort_loading) break;
                        try {
                            if (fs::is_directory(it->path()) && is_osu_song_folder(it->path())) {
                                it.disable_recursion_pending();
                                BoxDef osu_box_def = box_def;
                                osu_box_def.name = it->path().filename().string();
                                enqueue_box(std::make_unique<FolderBox>(it->path(), osu_box_def, song_files));
                            } else if (is_song_file(it->path())) {
                                enqueue_box(make_song_box(it->path(), box_def, SongParser(it->path())));
                            }
                        } catch (const std::exception& inner) {
                            spdlog::warn("Skipping song: {}", inner.what());
                        }
                        it.increment(ec);
                        if (ec) { ec.clear(); }
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("Skipping entry: {}", e.what());
            }
        }
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Error loading directory: {}", e.what());
    } catch (const std::exception& e) {
        // This runs on the loader thread, where an escaping exception takes
        // the process down instead of failing one directory.
        spdlog::error("Error loading directory {}: {}", path.string(), e.what());
    } catch (...) {
        spdlog::error("Unknown error loading directory {}", path.string());
    }
    loading_complete = true;
    current_path = path;
    if (!reloading_roots) FolderBox::run_deferred_scans(abort_loading);
}

void Navigator::load_collection_new(const fs::path& path, const BoxDef& box_def) {
    auto two_weeks_ago = ch::system_clock::now() - ch::weeks(2);
    int songs_added = 0;
    for (const auto& sibling : fs::directory_iterator(path.parent_path())) {
        if (abort_loading) break;
        if (!fs::is_directory(sibling) || sibling.path() == path) continue;
        BoxDef sibling_box_def = parse_box_def(sibling.path());
        for (const auto& entry : fs::recursive_directory_iterator(sibling)) {
            if (abort_loading) break;
            if (!is_song_file(entry.path())) continue;
            auto last_write = fs::last_write_time(entry.path().parent_path());
            auto last_write_sys = ch::system_clock::now() +
                std::chrono::duration_cast<ch::system_clock::duration>(
                    last_write - std::filesystem::file_time_type::clock::now());
            if (last_write_sys < two_weeks_ago) continue;
            if (songs_added > 0 && songs_added % 10 == 0)
                enqueue_inline_box(make_back_box(path.parent_path()));
            auto song = make_song_box(entry.path(), box_def, SongParser(entry.path()));
            apply_song_genre(song.get(), sibling_box_def);
            song->fade_in(266);
            enqueue_inline_box(std::move(song));
            songs_added++;
        }
    }
}

void Navigator::load_collection_difficulty(const fs::path& path, const BoxDef& box_def, int course, int level) {
    int songs_added = 0;
    for (const auto& sibling : fs::directory_iterator(path.parent_path())) {
        if (abort_loading) break;
        if (!fs::is_directory(sibling) || sibling.path() == path) continue;
        BoxDef sibling_box_def = parse_box_def(sibling.path());
        for (const auto& entry : fs::recursive_directory_iterator(sibling)) {
            if (abort_loading) break;
            if (!is_song_file(entry.path())) continue;
            SongParser parser(entry.path());
            parser.get_metadata();
            auto it = parser.metadata.course_data.find(course);
            if (it == parser.metadata.course_data.end()) continue;
            if ((int)it->second.level != level) continue;
            if (songs_added > 0 && songs_added % 10 == 0)
                enqueue_inline_box(make_back_box(path.parent_path()));
            auto song = make_song_box(entry.path(), box_def, parser);
            apply_song_genre(song.get(), sibling_box_def);
            song->fade_in(266);
            enqueue_inline_box(std::move(song));
            songs_added++;
        }
    }
}

void Navigator::apply_diff_sort(int course, int level) {
    last_diff_sort_result = {course, level};
    diff_sort_filter      = {course, level};
    awaiting_diff_sort    = false;
    begin_inline_load();
}

void Navigator::cancel_diff_sort() {
    awaiting_diff_sort = false;
    diff_sort_filter.reset();
    inline_state.reset();
    pending_inline_path.reset();
    pending_inline_folder = nullptr;
}

void Navigator::load_from_song_list(const fs::path& path, const BoxDef& box_def, bool mark_favorite) {
    int songs_added = 0;
    for (const auto& entry : read_song_list(path / "song_list.txt")) {
        if (abort_loading) break;
        fs::path song_path;
        if (auto found = scores_manager.get_path_by_hash(entry.hash)) {
            song_path = *found;
        } else {
            auto it = song_files.find({entry.title, entry.subtitle});
            if (it == song_files.end()) continue;
            song_path = it->second;
        }
        if (songs_added > 0 && songs_added % 10 == 0)
            enqueue_inline_box(make_back_box(path.parent_path()));
        auto song = make_song_box(song_path, box_def, SongParser(song_path));
        // The text file is the order: most recent first for RECENT, the
        // order they were added for FAVORITE. Without this the completion
        // sort alphabetises them and that meaning is lost.
        song->preserve_order = true;
        if (mark_favorite) song->is_favorite = true;
        fs::path genre_folder = find_box_def_folder(song_path);
        if (!genre_folder.empty())
            apply_song_genre(song.get(), parse_box_def(genre_folder));
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
        songs_added++;
    }
}

void Navigator::toggle_favorite(SongBox* song) {
    if (!favorite_folder_path) return;
    FolderBox::invalidate_scan_cache();  // song_list.txt counts change
    auto& t = song->parser.metadata.title;
    auto& s = song->parser.metadata.subtitle;
    std::string title    = t.count("en") ? t.at("en") : t.begin()->second;
    std::string subtitle = s.count("en") ? s.at("en") : s.begin()->second;
    std::string key      = title + "|" + subtitle;

    fs::path song_list_path = *favorite_folder_path / "song_list.txt";
    auto entries = read_song_list(song_list_path);

    bool was_favorite = favorite_songs.count(key) > 0;
    if (was_favorite) {
        favorite_songs.erase(key);
        entries.erase(std::remove_if(entries.begin(), entries.end(),
            [&](const SongListEntry& e) { return e.title == title && e.subtitle == subtitle; }),
            entries.end());
        song->is_favorite = false;
    } else {
        favorite_songs.insert(key);
        std::string hash = scores_manager.get_single_hash(song->parser.file_path);
        entries.insert(entries.begin(), {std::move(hash), title, subtitle});
        song->is_favorite = true;
    }

    write_song_list(song_list_path, entries);
}

fs::path Navigator::find_box_def_folder(const fs::path& song_path) {
    fs::path current = song_path.parent_path();
    while (!current.empty() && current != current.parent_path()) {
        if (fs::exists(current / "box.def")) return current;
        current = current.parent_path();
    }
    return fs::path{};
}


void Navigator::add_to_recent(const SongBox* song) {
    if (!recent_folder_path) return;
    FolderBox::invalidate_scan_cache();  // song_list.txt counts change
    fs::path song_list_path = *recent_folder_path / "song_list.txt";

    auto& titles    = song->parser.metadata.title;
    auto& subtitles = song->parser.metadata.subtitle;
    std::string title    = titles.count("en")    ? titles.at("en")    : titles.begin()->second;
    std::string subtitle = subtitles.count("en") ? subtitles.at("en") : subtitles.begin()->second;
    std::string hash     = scores_manager.get_single_hash(song->parser.file_path);

    auto entries = read_song_list(song_list_path);
    entries.erase(std::remove_if(entries.begin(), entries.end(),
        [&](const SongListEntry& e) { return e.title == title && e.subtitle == subtitle; }),
        entries.end());
    entries.insert(entries.begin(), {std::move(hash), title, subtitle});
    if ((int)entries.size() > 25) entries.resize(25);

    write_song_list(song_list_path, entries);
    promote_recent_box(song);
}

void Navigator::promote_recent_box(const SongBox* song) {
    if (!inline_state.has_value() || pending_inline_folder == nullptr) return;
    if (pending_inline_folder->collection != "RECENT") return;

    int first = inline_state->first_song_index;
    int end   = first + inline_state->songs_count;
    if (first < 0 || end > (int)items.size()) return;

    std::vector<int> slots;
    for (int i = first; i < end; i++)
        if (dynamic_cast<SongBox*>(items[i].get())) slots.push_back(i);

    int pos = -1;
    for (int i = 0; i < (int)slots.size(); i++)
        if (items[slots[i]]->path == song->path) { pos = i; break; }
    if (pos <= 0) return;  // not in this listing, or already first

    auto moved = std::move(items[slots[pos]]);
    for (int i = pos; i > 0; i--) items[slots[i]] = std::move(items[slots[i - 1]]);
    items[slots[0]] = std::move(moved);

    // Keep the cursor on the same song rather than on whatever slid into
    // its slot.
    if (open_index == slots[pos]) {
        open_index = slots[0];
    } else {
        for (int i = 0; i < pos; i++)
            if (open_index == slots[i]) { open_index = slots[i + 1]; break; }
    }
}

void Navigator::load_collection_recommended(const fs::path& path, const BoxDef& box_def) {
    std::vector<fs::path> all_songs;
    all_songs.reserve(song_files.size());
    for (const auto& [key, song_path] : song_files)
        all_songs.push_back(song_path);

    std::mt19937 rng(std::random_device{}());
    std::shuffle(all_songs.begin(), all_songs.end(), rng);
    int count = std::min((int)all_songs.size(), 10);
    for (int i = 0; i < count; i++) {
        if (abort_loading) break;
        const fs::path& song_path = all_songs[i];
        auto song = make_song_box(song_path, box_def, SongParser(song_path));
        fs::path genre_folder = find_box_def_folder(song_path);
        if (!genre_folder.empty())
            apply_song_genre(song.get(), parse_box_def(genre_folder));
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
    }
}

void Navigator::load_collection_search(const fs::path& path, const BoxDef& box_def) {
    if (current_search.empty()) return;
    std::string query = current_search;
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);
    int songs_added = 0;
    for (const auto& [key, song_path] : song_files) {
        if (abort_loading) break;
        std::string title = key.first;
        std::transform(title.begin(), title.end(), title.begin(), ::tolower);
        if (title.find(query) == std::string::npos) continue;
        if (songs_added > 0 && songs_added % 10 == 0)
            enqueue_inline_box(make_back_box(path.parent_path()));
        auto song = make_song_box(song_path, box_def, SongParser(song_path));
        fs::path genre_folder = find_box_def_folder(song_path);
        if (!genre_folder.empty())
            apply_song_genre(song.get(), parse_box_def(genre_folder));
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
        songs_added++;
    }
}

void Navigator::load_songs_inline_async(const fs::path path, BoxDef box_def) {
    if (load_gen4_genre_songs(path, box_def)) {
        loading_complete = true;
        return;
    }
    if (load_green_genre_songs(path, box_def)) {
        loading_complete = true;
        return;
    }

    wait_for_song_files();
    int songs_added = 0;
    std::unordered_map<std::string, std::unique_ptr<SongParser>> preparsed;

    auto add_song = [&](const fs::path& song_path) {
        if (songs_added > 0 && songs_added % 10 == 0)
            enqueue_inline_box(make_back_box(path.parent_path()));
        auto box = make_song_box(song_path, box_def, take_parser(preparsed, song_path));
        box->fade_in(266);
        enqueue_inline_box(std::move(box));
        songs_added++;
    };

    if (box_def.collection == "RECOMMENDED") {
        load_collection_recommended(path, box_def);
        loading_complete = true;
        return;
    } else if (box_def.collection == "FAVORITE") {
        load_from_song_list(path, box_def, true);
        loading_complete = true;
        return;
    } else if (box_def.collection == "RECENT") {
        load_from_song_list(path, box_def, false);
        loading_complete = true;
        return;
    } else if (box_def.collection == "DIFFICULTY") {
        if (diff_sort_filter) {
            load_collection_difficulty(path, box_def, diff_sort_filter->first, diff_sort_filter->second);
            diff_sort_filter.reset();
        }
        loading_complete = true;
        return;
    } else if (box_def.collection == "NEW") {
        load_collection_new(path, box_def);
        loading_complete = true;
        return;
    } else if (box_def.collection == "SEARCH") {
        load_collection_search(path, box_def);
        loading_complete = true;
        return;
    }

    std::vector<fs::path> song_paths;
    try {
        for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
            if (abort_loading) break;
            const fs::path& curr_path = entry.path();
            if (!fs::is_directory(curr_path)) {
                if (is_song_file(curr_path)) song_paths.push_back(curr_path);
                continue;
            }
            if (is_osu_song_folder(curr_path)) continue;
            std::error_code ec;
            auto it = fs::recursive_directory_iterator(curr_path, ec);
            while (it != fs::end(it)) {
                if (abort_loading) break;
                if (fs::is_directory(it->path()) && is_osu_song_folder(it->path()))
                    it.disable_recursion_pending();
                else if (is_song_file(it->path()))
                    song_paths.push_back(it->path());
                it.increment(ec);
                if (ec) { ec.clear(); }
            }
        }
    } catch (const fs::filesystem_error&) { /* main loop reports errors */ }
    preparsed = parse_songs_parallel(song_paths, abort_loading);

    try {
        for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
            if (abort_loading) break;
            const fs::path& curr_path = entry.path();
            try {
                if (!fs::is_directory(curr_path)) {
                    if (curr_path.filename() == "song_list.txt") {
                        parse_song_list(curr_path, box_def, true);
                        continue;
                    }
                    if (is_song_file(curr_path))
                        enqueue_inline_box(make_song_box(curr_path, box_def, take_parser(preparsed, curr_path)));
                    continue;
                }
                if (is_osu_song_folder(curr_path)) {
                    BoxDef osu_box_def = box_def;
                    osu_box_def.name = curr_path.filename().string();
                    auto folder = std::make_unique<FolderBox>(curr_path, osu_box_def, song_files);
                    folder->fade_in(266);
                    enqueue_inline_box(std::move(folder));
                } else {
                    std::error_code ec;
                    auto it = fs::recursive_directory_iterator(curr_path, ec);
                    while (it != fs::end(it)) {
                        if (abort_loading) break;
                        try {
                            if (fs::is_directory(it->path()) && is_osu_song_folder(it->path())) {
                                it.disable_recursion_pending();
                                BoxDef osu_box_def = box_def;
                                osu_box_def.name = it->path().filename().string();
                                auto folder = std::make_unique<FolderBox>(it->path(), osu_box_def, song_files);
                                folder->fade_in(266);
                                enqueue_inline_box(std::move(folder));
                            } else if (is_song_file(it->path())) {
                                add_song(it->path());
                            }
                        } catch (const std::exception& inner) {
                            spdlog::warn("Skipping song: {}", inner.what());
                        }
                        it.increment(ec);
                        if (ec) { ec.clear(); }
                    }
                }
            } catch (const std::exception& e) {
                spdlog::warn("Skipping entry: {}, {}", e.what(), curr_path.string());
            }
        }
    } catch (const fs::filesystem_error& e) {
        spdlog::error("Error loading directory: {}", e.what());
    }
    loading_complete = true;
}

BoxDef Navigator::parse_box_def(const fs::path& path) {
    std::string key = path.string();
    auto it = box_def_cache.find(key);
    if (it != box_def_cache.end()) return it->second;

    std::ifstream boxDef(path / "box.def");
    std::string line;
    BoxDef result;
    result.name = path.filename().string();
    result.texture_index = TextureIndex::DEFAULT;
    result.genre_index = GenreIndex::DEFAULT;
    result.collection = "";
    bool title_localized = false;

    while (std::getline(boxDef, line)) {
        if (line.size() >= 3 && (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
            line.erase(0, 3);
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        auto get_value = [&](const std::string& prefix) -> std::string {
            return line.substr(prefix.size());
        };

        if (line.starts_with("#GENRE:")) {
            std::string genre = get_value("#GENRE:");
            auto it = TEXTURE_MAP.find(genre);
            if (it != TEXTURE_MAP.end()) result.texture_index = it->second;
            result.genre_index = get_genre_index(genre);
        } else if (line.starts_with("#TITLE:")) {
            if (!title_localized)
                result.name = get_value("#TITLE:");
        } else if (line.starts_with("#TITLE")) {
            const std::string& lang = global_data.config->general.language;
            std::string lang_upper = lang;
            std::transform(lang_upper.begin(), lang_upper.end(), lang_upper.begin(), ::toupper);
            std::string lang_prefix = "#TITLE" + lang_upper + ":";
            if (line.starts_with(lang_prefix)) {
                result.name = get_value(lang_prefix);
                title_localized = true;
            }
        } else if (line.starts_with("#COLLECTION:")) {
            result.collection = get_value("#COLLECTION:");
            auto it = TEXTURE_MAP.find(result.collection);
            if (it != TEXTURE_MAP.end()) result.texture_index = it->second;
            result.genre_index = get_genre_index(result.collection);
        } else if (line.starts_with("#BACKCOLOR:")) {
            result.back_color = parse_hex_color(get_value("#BACKCOLOR:"));
            result.texture_index = TextureIndex::NONE;
        } else if (line.starts_with("#FORECOLOR:")) {
            result.fore_color = parse_hex_color(get_value("#FORECOLOR:"));
        }
    }
    box_def_cache[key] = result;
    return result;
}

bool Navigator::has_def_file(const std::filesystem::path& path) {
    std::string key = path.string();
    auto it = def_file_cache.find(key);
    if (it != def_file_cache.end()) return it->second;

    std::error_code ec;
    if (fs::exists(path / "box.def", ec)) {
        def_file_cache[key] = true;
        return true;
    }

    for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_directory(ec)) continue;
        // Never descend into a game's data: there is no box.def inside, just
        // an enormous tree of charts and tables.
        if (is_gen4_root(entry.path()) || !green_root_at(entry.path()).empty()) continue;
        if (has_def_file(entry.path())) {
            def_file_cache[key] = true;
            return true;
        }
    }
    def_file_cache[key] = false;
    return false;
}


void Navigator::exit_inline() {
    if (!inline_state) return;
    InlineState& state = *inline_state;

    // Kick off fade_out on all inserted items
    int end = state.first_song_index + state.songs_count;
    for (int i = state.first_song_index - 1; i < end; i++)
        items[i]->fade_out();

    state.fading_out = true;
    auto& start_box = items[genre_bg_start];
    auto& end_box = items[genre_bg_end];
    float start_pos = start_box->left_bound;
    float end_pos = end_box->right_bound;
    genre_bg->exit(start_pos, end_pos, pending_inline_folder);
    is_inline = false;
    is_processing = true;
}

void Navigator::begin_inline_load() {
    int approx_items = (pending_inline_box_def.collection == "RECOMMENDED" ||
                        pending_inline_box_def.collection == "DIFFICULTY")
                       ? 10
                       : pending_inline_folder->tja_count;
    items[open_index]->enter_box();
    genre_bg_start = 0;
    genre_bg_end   = 0;
    int approx_total = (approx_items + approx_items / 10) + 1;
    int last_index = open_index + approx_total;

    float offset = last_index - open_index;
    float base_spacing = 100 * tex.screen_scale;
    float side_offset_r = 300 * tex.screen_scale;
    float center_offset = 150 * tex.screen_scale;

    float temp_end_pos = (594 - center_offset) + (offset * base_spacing) + side_offset_r;
    genre_bg_end_pos = (temp_end_pos > tex.screen_width) ? -100.0f : temp_end_pos;

    genre_bg.emplace(items[open_index]->text_name, items[open_index]->back_color,
                     items[open_index]->texture_index, approx_items * 100);
    is_processing = true;
    for (int i = 0; i < (int)items.size(); i++) {
        if (items[i]->position > items[open_index]->position)
            items[i]->move_box(tex.screen_width + 150, 600);
    }
}

void Navigator::load_current_directory(const fs::path path) {
    BoxDef box_def = parse_box_def(path);
    bool has_children = has_child_folders(path);

    // Coming back out to a root rebuilds every root together: the top of the
    // wheel is all of them, and each load cancels the one before it, so
    // reloading just this one would leave only its own songs on the wheel.
    if (has_children && root_paths.size() > 1 && !reloading_roots &&
        std::find(root_paths.begin(), root_paths.end(), path) != root_paths.end() &&
        // A song path can point straight at a game, and then its box carries
        // that same path. Opening it is going in, not coming back out, so it
        // must not be read as a return to the top of the wheel.
        gen4::find_data_root(path).empty() && green::find_data_root(path).empty()) {
        // A folder was open on a wheel that is otherwise already showing the
        // top level: fold the listing shut and leave every box standing,
        // rather than rebuilding the whole wheel just to show what is
        // already there.
        if (inline_state.has_value()) {
            collapse_inline_now();
            return;
        }

        // The wheel really was replaced (a game's genres were up), so it has
        // to be rebuilt; remember which box the cursor should come back to.
        {
            fs::path game = gen4::find_data_root(current_path);
            if (game.empty()) game = green::find_data_root(current_path);
            if (!game.empty())
                restore_cursor_path = game;
            else if (open_index >= 0 && open_index < (int)items.size())
                restore_cursor_path = items[open_index]->path;
        }
        load_all_roots();
        return;
    }

    if (!has_children && !items.empty()) {
        if (inline_state.has_value()) {
            collapse_inline_now();
            items[open_index]->close_box();
            // Now move the cursor to the folder actually being opened.
            for (int i = 0; i < (int)items.size(); i++)
                if (items[i]->path == path) { open_index = i; break; }
            set_positions(true, 0);
        }

        InlineState state;
        state.folder_index     = open_index;
        state.first_song_index = open_index + 1;
        state.songs_count      = 0;
        pending_inline_folder  = static_cast<FolderBox*>(items[open_index].get());
        inline_state           = std::move(state);
        pending_inline_path    = path;
        pending_inline_box_def = box_def;

        if (box_def.collection == "DIFFICULTY") {
            awaiting_diff_sort = true;
            return;
        }

        begin_inline_load();
        return;
    }

    if (is_inline) {
        exit_inline();
        return;
    }

    open_index = 0;
    def_file_cache.clear();
    box_def_cache.clear();
    setup_back_box(path, true);
    is_processing = true;
    loading_complete = false;

    join_loader();
#ifndef __EMSCRIPTEN__
    loader_thread = std::thread(&Navigator::load_current_directory_async, this, path);
#else
    load_current_directory_async(path);
#endif
}

bool Navigator::jump_to_song(const std::string& hash) {
    auto song_path_opt = scores_manager.get_path_by_diff_hash(hash);
    if (!song_path_opt) {
        spdlog::warn("jump_to_song: no song found for hash {}", hash);
        return false;
    }
    fs::path song_path = *song_path_opt;

    std::vector<fs::path> chain;
    fs::path cur = song_path.parent_path();
    while (true) {
        fs::path folder = find_box_def_folder(cur);
        if (folder.empty()) break;
        chain.push_back(folder);
        if (std::find(root_paths.begin(), root_paths.end(), folder.parent_path()) != root_paths.end())
            break;
        cur = folder.parent_path();
    }
    if (chain.empty()) {
        spdlog::warn("jump_to_song: could not resolve a genre folder for {}", song_path.string());
        return false;
    }
    std::reverse(chain.begin(), chain.end());

    BoxDef final_box_def = parse_box_def(chain.back());
    if (!final_box_def.collection.empty()) {
        spdlog::warn("jump_to_song: song resolves into a synthetic collection folder, not supported: {}", chain.back().string());
        return false;
    }

    join_loader();
    is_inline = false;
    inline_state.reset();
    pending_inline_path.reset();
    pending_inline_folder = nullptr;
    genre_bg.reset();
    awaiting_diff_sort = false;
    diff_sort_filter.reset();

    auto load_full = [&](const fs::path& folder) {
        open_index = 0;
        def_file_cache.clear();
        box_def_cache.clear();
        setup_back_box(folder, true);
        loading_complete = false;
        load_current_directory_async(folder);
        flush_pending_boxes();
    };

    load_full(chain.front().parent_path());
    for (size_t i = 0; i + 1 < chain.size(); i++) {
        load_full(chain[i]);
    }

    fs::path final_folder = chain.back();
    bool final_has_children = has_child_folders(final_folder);

    if (final_has_children || items.empty()) {
        load_full(final_folder);
    } else {
        int folder_index = -1;
        for (int i = 0; i < (int)items.size(); i++) {
            if (items[i]->path == final_folder) {
                folder_index = i;
                break;
            }
        }
        if (folder_index < 0) {
            spdlog::warn("jump_to_song: could not locate folder box for {}", final_folder.string());
            return false;
        }


        items[open_index]->close_box();
        open_index = folder_index;
        set_positions(true, 0);
        items[open_index]->expand_box();
        items[open_index]->enter_box();

        FolderBox* folder_box = static_cast<FolderBox*>(items[open_index].get());
        for (auto& item : items) {
            if (item && item->position > folder_box->position)
                item->move_box(tex.screen_width + 150, 600);
        }

        InlineState state;
        state.folder_index     = open_index;
        state.first_song_index = open_index + 1;
        state.songs_count      = 0;
        state.saved_folder_box = std::unique_ptr<FolderBox>(static_cast<FolderBox*>(items[open_index].release()));
        pending_inline_folder  = folder_box;
        inline_state           = std::move(state);

        setup_back_box(final_folder, false);
        genre_bg.emplace(folder_box->text_name, folder_box->back_color, folder_box->texture_index, 1000.0f);

        loading_complete = false;
        is_inline = true;
        load_songs_inline_async(final_folder, final_box_def);
        flush_pending_boxes();
    }

    if (items.empty()) {
        spdlog::warn("jump_to_song: destination folder loaded no items for {}", song_path.string());
        return false;
    }

    int target_index = -1;
    for (int i = 0; i < (int)items.size(); i++) {
        if (items[i]->path == song_path) {
            target_index = i;
            break;
        }
    }
    if (target_index < 0) {
        spdlog::warn("jump_to_song: song loaded but not found among items: {}", song_path.string());
        return false;
    }

    items[open_index]->close_box();
    open_index = target_index;
    bg_genre_index = items[open_index]->genre_index;
    last_bg_genre_index = bg_genre_index;
    set_positions(true, 0);
    items[open_index]->expand_box();
    background_fade_change->start();
    is_processing = false;
    return true;
}

void Navigator::setup_back_box(const fs::path& path, bool has_children) {
    if (has_children) {
        // While every root is being walked in turn, the wheel was already
        // emptied once and each root adds to it.
        if (!reloading_roots) items.clear();
        // A root level has no folder to close: the box would point at the
        // songs dir's parent and selecting it does nothing sensible.
        if (std::find(root_paths.begin(), root_paths.end(), path) != root_paths.end())
            return;
        items.push_back(make_back_box(path.parent_path()));
    } else {
        auto back = make_back_box(path.parent_path());
        back->fade_in(266);
        items.erase(items.begin() + open_index);
        items.insert(items.begin() + open_index, std::move(back));
    }
}

bool Navigator::has_child_folders(const fs::path& path) {
    // A data root opens into its genres, which replaces the wheel; a genre
    // opens into songs, which expands in place like any other genre folder.
    if (gen4::genre_of_path(path) >= 0) return false;
    if (!green::genre_of_path(path).empty()) return false;
    if (is_gen4_root(path) || is_green_root(path)) return true;

    for (const auto& entry : fs::directory_iterator(path)) {
        if (fs::is_directory(entry.path()) && has_def_file(entry.path()) || is_osu_song_folder(entry.path()))
            return true;
        // A folder holding a game data root is a level of its own too, or
        // coming back out of that game tries to expand this folder as a song.
        if (is_gen4_root(entry.path()) || !green_root_at(entry.path()).empty())
            return true;
    }
    return false;
}

bool Navigator::is_directory(BaseBox* item) {
    // Not simply "the path is a folder": a gen 4 song is a folder of chart
    // files, and treating one as a directory opens it instead of playing it,
    // while a gen 4 genre is a folder whose path is not on disk at all.
    if (dynamic_cast<FolderBox*>(item) != nullptr) return true;
    return !is_song(item) && fs::is_directory(item->path);
}

bool Navigator::is_song_file(const fs::path& path) {
    if (!fs::is_regular_file(path)) return false;
    auto ext = path.extension();
    return ext == ".tja" || ext == ".osu";
}

// A gen 4 genre has no box.def of its own, so it borrows the one the library
// already has for that genre: same colours, same folder art, same text
// outline, rather than a second set that only nearly matches.
const BoxDef* Navigator::box_def_for_genre(GenreIndex genre) {
    if (!genre_box_defs_built) {
        genre_box_defs_built = true;
        for (const fs::path& root : root_paths) {
            std::error_code ec;
            // A game data root holds no box.def, only tens of thousands of
            // chart files the recursive search below would crawl through.
            if (!gen4::find_data_root(root).empty() ||
                !green::find_data_root(root).empty()) continue;
            if (!fs::is_directory(root, ec)) continue;
            for (const auto& entry : fs::directory_iterator(root, ec)) {
                if (abort_loading) return nullptr;
                if (!fs::is_directory(entry.path(), ec)) continue;
                if (is_gen4_root(entry.path()) || !green_root_at(entry.path()).empty()) continue;
                if (!has_def_file(entry.path())) continue;
                BoxDef def = parse_box_def(entry.path());
                genre_box_defs.emplace(def.genre_index, def);
            }
        }
    }
    auto it = genre_box_defs.find(genre);
    return it != genre_box_defs.end() ? &it->second : nullptr;
}

// The folder holding a game's datatable, fumen and sound. It stands in the
// wheel as one box: opening it puts the wheel into that game's own genres, and
// the back box leads out again.
bool Navigator::is_gen4_root(const fs::path& path) {
    std::error_code ec;
    if (!fs::is_directory(path, ec)) return false;
    return gen4::find_data_root(path) == path && gen4::library_for(path) != nullptr;
}

// True when the song paths hold nothing but game data. There is then nothing
// to switch back to, so the genres of that game are the top of the wheel
// rather than sitting inside a folder of their own.
bool Navigator::only_gen4_songs() {
    int games = 0;
    std::error_code ec;
    for (const fs::path& root : root_paths) {
        if (!gen4::find_data_root(root).empty() ||
            !green::find_data_root(root).empty()) { games++; continue; }
        if (!fs::is_directory(root, ec)) continue;
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (is_gen4_root(entry.path()) || !green_root_at(entry.path()).empty()) {
                games++;
                continue;
            }
            // Anything else at all - a genre folder, an osu folder, a loose
            // chart - means the wheel has somewhere else to go.
            return false;
        }
    }
    // Several games have to stand as boxes; one on its own is the wheel.
    return games == 1;
}

void Navigator::load_gen4_genres(const fs::path& data_root) {
    const gen4::Library* library = gen4::library_for(data_root);
    if (!library) return;

    const std::string& lang = global_data.config->general.language;
    for (int genre_no : gen4::genres_present(*library)) {
        if (abort_loading) break;
        GenreIndex genre = (GenreIndex)gen4::genre_index_for(genre_no);

        BoxDef def;
        if (const BoxDef* like = box_def_for_genre(genre)) {
            def = *like;
        } else {
            // Nothing in the library defines this genre - a game on its own,
            // with no TJA songs beside it - so the look a box.def would give
            // it is spelled out here instead: the same colours, and the same
            // texture, which is the plain genre art for everything except
            // vocaloid, which has art of its own.
            // NONE means "tint the plain box with these colours", which is
            // what gives each genre its own look; vocaloid is the one genre
            // the skin has art for, and its box.def uses that instead.
            def.texture_index = (genre == GenreIndex::VOCALOID)
                              ? TextureIndex::VOCALOID : TextureIndex::NONE;
            if (auto colors = DEFAULT_COLORS.find(genre); colors != DEFAULT_COLORS.end()) {
                def.back_color = colors->second[0];
                def.fore_color = colors->second[1];
            }
        }
        def.name        = gen4::genre_name(genre_no, lang);
        def.genre_index = genre;
        auto folder = std::make_unique<FolderBox>(gen4::genre_path(data_root, genre_no),
                                                  def, song_files);
        folder->tja_count = 0;
        for (const gen4::OrderEntry& listing : library->order())
            if (listing.genre_no == genre_no) folder->tja_count++;
        enqueue_box(std::move(folder));
    }
}

bool Navigator::load_gen4_genre_songs(const fs::path& path, const BoxDef& box_def) {
    int genre_no = gen4::genre_of_path(path);
    if (genre_no < 0) return false;

    fs::path data_root = gen4::find_data_root(path);
    const gen4::Library* library = gen4::library_for(data_root);
    if (!library) return false;

    GenreIndex genre = (GenreIndex)gen4::genre_index_for(genre_no);

    BoxDef def = box_def;
    if (const BoxDef* like = box_def_for_genre(genre)) {
        def = *like;
    } else if (auto colors = DEFAULT_COLORS.find(genre); colors != DEFAULT_COLORS.end()) {
        // NONE means "tint the plain box art with these colours", which is
        // what a box.def genre does to its songs. Vocaloid is the exception:
        // it has no colour to tint to, only art of its own, and asking for a
        // tint that cannot happen leaves the untinted art showing through -
        // which is a green box under a genre that is not green.
        def.texture_index = (genre == GenreIndex::VOCALOID)
                          ? TextureIndex::VOCALOID : TextureIndex::NONE;
        def.back_color    = colors->second[0];
        def.fore_color    = colors->second[1];
    }
    def.name        = box_def.name;
    def.genre_index = genre;

    int songs_added = 0;
    for (const gen4::OrderEntry& listing : library->order()) {
        if (abort_loading) break;
        if (listing.genre_no != genre_no) continue;
        fs::path song_folder = data_root / "fumen" / listing.id;
        std::error_code ec;
        if (!fs::is_directory(song_folder, ec)) continue;
        if (songs_added > 0 && songs_added % 10 == 0)
            enqueue_inline_box(make_back_box(data_root));
        auto box = make_song_box(song_folder, def, SongParser(song_folder));
        // The running order is the order: leave it alone rather than sorting
        // these by title the way a folder of files is sorted.
        box->preserve_order = true;
        box->fade_in(266);
        enqueue_inline_box(std::move(box));
        songs_added++;
    }
    return true;
}

// The folder holding a PS3 game's data: fumen/, sound/ and config/ together.
bool Navigator::is_green_root(const fs::path& path) {
    std::error_code ec;
    if (!fs::is_directory(path, ec)) return false;
    return green::find_data_root(path) == path && green::library_for(path) != nullptr;
}

// The green data root standing at - or nested just under - a folder: the
// PS3 dumps keep it as <game>/USRDIR/data, and a folder of dumps should show
// one box per game without the user having to point at each data folder.
fs::path Navigator::green_root_at(const fs::path& path) {
    if (is_green_root(path)) return path;
    fs::path nested = path / "USRDIR" / "data";
    if (is_green_root(nested)) return nested;
    return {};
}

// A green song folder is fumen/<id>, holding solo/ and duet/ chart folders.
bool Navigator::is_green_song_folder(const fs::path& path) {
    if (path.parent_path().filename() != "fumen") return false;
    const green::Library* library = green::library_for(path);
    return library && library->find(path.filename().string()) != nullptr;
}

void Navigator::load_green_genres(const fs::path& data_root) {
    const green::Library* library = green::library_for(data_root);
    if (!library) return;

    for (const std::string& genre_name : green::genres_present(*library)) {
        if (abort_loading) break;
        GenreIndex genre = (GenreIndex)green::genre_index_for(genre_name);

        BoxDef def;
        if (const BoxDef* like = box_def_for_genre(genre)) {
            def = *like;
        } else {
            def.texture_index = (genre == GenreIndex::VOCALOID)
                              ? TextureIndex::VOCALOID : TextureIndex::NONE;
            if (auto colors = DEFAULT_COLORS.find(genre); colors != DEFAULT_COLORS.end()) {
                def.back_color = colors->second[0];
                def.fore_color = colors->second[1];
            }
        }
        def.name        = genre_name;
        def.genre_index = genre;
        auto folder = std::make_unique<FolderBox>(green::genre_path(data_root, genre_name),
                                                  def, song_files);
        folder->tja_count = 0;
        for (const green::SongEntry& e : library->songs())
            if (e.genre == genre_name) folder->tja_count++;
        enqueue_box(std::move(folder));
    }
}

bool Navigator::load_green_genre_songs(const fs::path& path, const BoxDef& box_def) {
    std::string genre_name = green::genre_of_path(path);
    if (genre_name.empty()) return false;

    fs::path data_root = green::find_data_root(path);
    const green::Library* library = green::library_for(data_root);
    if (!library) return false;

    GenreIndex genre = (GenreIndex)green::genre_index_for(genre_name);

    BoxDef def = box_def;
    if (const BoxDef* like = box_def_for_genre(genre)) {
        def = *like;
    } else if (auto colors = DEFAULT_COLORS.find(genre); colors != DEFAULT_COLORS.end()) {
        def.texture_index = (genre == GenreIndex::VOCALOID)
                          ? TextureIndex::VOCALOID : TextureIndex::NONE;
        def.back_color    = colors->second[0];
        def.fore_color    = colors->second[1];
    }
    def.name        = box_def.name;
    def.genre_index = genre;

    int songs_added = 0;
    for (const green::SongEntry& listing : library->songs()) {
        if (abort_loading) break;
        if (listing.genre != genre_name) continue;
        fs::path song_folder = data_root / "fumen" / listing.id;
        std::error_code ec;
        if (!fs::is_directory(song_folder, ec)) continue;
        if (songs_added > 0 && songs_added % 10 == 0)
            enqueue_inline_box(make_back_box(data_root));
        auto box = make_song_box(song_folder, def, SongParser(song_folder));
        // The file's own order is the game's order.
        box->preserve_order = true;
        box->fade_in(266);
        enqueue_inline_box(std::move(box));
        songs_added++;
    }
    return true;
}

// A gen 4 song folder is named after the song id and holds that id's chart
// files, one per difficulty. It only counts as a song when the game data it
// belongs to can be read, since the folder alone carries no title or ratings.
bool Navigator::is_gen4_song_folder(const fs::path& path) {
    std::error_code ec;
    if (!fs::is_directory(path, ec)) return false;

    std::string id = path.filename().string();
    static const char* suffixes[] = { "_e", "_n", "_h", "_m", "_x" };
    bool has_chart = false;
    for (const char* suffix : suffixes) {
        if (fs::exists(path / (id + suffix + ".bin"), ec)) { has_chart = true; break; }
    }
    if (!has_chart) return false;

    return gen4::library_for(path) != nullptr;
}

bool Navigator::is_osu_song_folder(const fs::path& path) {
    if (!fs::is_directory(path)) return false;
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (fs::is_regular_file(entry.path()) && entry.path().extension() == ".osu")
                return true;
        }
    } catch (...) {}
    return false;
}


bool Navigator::is_song(BaseBox* item) {
    // What the box is, not what its path looks like.
    return dynamic_cast<SongBox*>(item) != nullptr;
}

BaseBox* Navigator::get_current_item() {
    if (items.empty()) return nullptr;
    return items[open_index].get();
}

void Navigator::set_positions(bool init, float duration) {
    float num_boxes = items.size();
    for (int i = 0; i < num_boxes; i++) {
        float offset = i - open_index;
        if (offset > num_boxes / 2) {
            offset -= num_boxes;
        } else if (offset < -num_boxes / 2) {
            offset += num_boxes;
        }

        float position;
        if (vertical_gallery) {
            float base_spacing = 120 * tex.screen_scale;
            float center_y     = 360 * tex.screen_scale;
            float fixed_x      = 640 * tex.screen_scale;
            float expand_gap   = 90 * tex.screen_scale;
            position = center_y + offset * base_spacing;
            if (offset > 0)      position += expand_gap;
            else if (offset < 0) position -= expand_gap;
            items[i]->vertical   = true;
            float horizontal_spacing = 30 * tex.screen_scale;
            items[i]->cross_pos = fixed_x + offset * horizontal_spacing;
            if (init || std::abs(position - items[i]->position) >= tex.screen_height) {
                items[i]->set_position(position);
            } else {
                items[i]->move_box(position, duration);
            }
        } else {
            float base_spacing = 100 * tex.screen_scale;
            float center_offset = 150 * tex.screen_scale;
            float side_offset_l = 0 * tex.screen_scale;
            float side_offset_r = 300 * tex.screen_scale;

            position = ((594 * tex.screen_scale) - center_offset) + (offset * base_spacing);
            if (position == (594 * tex.screen_scale) - center_offset) {
                position += center_offset;
            } else if (position > (594 * tex.screen_scale) - center_offset) {
                position += side_offset_r;
            } else {
                position -= side_offset_l;
            }
            items[i]->vertical = false;
            if (init || std::abs(position - items[i]->position) >= tex.screen_width) {
                items[i]->set_position(position);
            } else {
                items[i]->move_box(position, duration);
            }
        }
    }
}

void Navigator::navigate(int delta, bool snap) {
    if (items.empty() || inline_streaming) return;
    items[open_index]->close_box();
    last_bg_genre_index = bg_genre_index;
    open_index = ((open_index + delta) % (int)items.size() + (int)items.size()) % (int)items.size();
    bg_genre_index = items[open_index]->genre_index;
    set_positions(snap, 166);
    items[open_index]->expand_box();
    background_fade_change->start();
}

void Navigator::move_left()  { navigate(-1,  false); }
void Navigator::move_right() { navigate(+1,  false); }
void Navigator::skip_left()  { navigate(-10, true);  }
void Navigator::skip_right() { navigate(+10, true);  }

void Navigator::enter_diff_select() {
    items[open_index]->enter_box();
    for (int i = 0; i < items.size(); i++) {
        std::unique_ptr<BaseBox>& box = items[i];
        float duration = 800;
        float distance = (150 * tex.screen_scale);
        if (vertical_gallery) {
            float center_y = 360 * tex.screen_scale;
            bool on_screen = box->position > -100 && box->position < tex.screen_height + 100;
            if (on_screen && i != open_index) {
                if (box->position < center_y) {
                    box->move_box(-distance, duration);
                } else {
                    box->move_box(tex.screen_height + distance, duration);
                }
                box->fade_out();
            }
        } else {
            bool on_screen = box->position > -100 && box->position < tex.screen_width + 100;
            if (on_screen && i != open_index) {
                if (box->position < (594 * tex.screen_scale)) {
                    box->move_box(-distance, duration);
                } else {
                    box->move_box(tex.screen_width + distance, duration);
                }
                box->fade_out();
            }
        }
    }
    if (genre_bg.has_value()) {
        genre_bg->fade_out();
    }
}

void Navigator::exit_diff_select() {
    items[open_index]->exit_box();
    set_positions(false, 500);
    for (auto& box : items) {
        box->fade_in(166);
    }
    if (genre_bg.has_value()) {
        genre_bg->fade_in();
    }
}

void Navigator::update(double current_ms) {
    if (genre_bg.has_value()) {
        if (inline_state && inline_state->fading_out)
            genre_bg->update(current_ms, pending_inline_folder);
        else
            genre_bg->update(current_ms, nullptr);
    }
    background_move->update(current_ms);
    background_fade_change->update(current_ms);

    flush_pending_boxes();

    if (pending_inline_path) {
        if (genre_bg.has_value() && genre_bg->is_finished() && !awaiting_diff_sort) {
            inline_state->saved_folder_box = std::unique_ptr<FolderBox>(
                static_cast<FolderBox*>(items[open_index].release())
            );
            setup_back_box(*pending_inline_path, false);
            int before = items.size();

            join_loader();
            loading_complete = false;
            is_inline = true;
#ifndef __EMSCRIPTEN__
            loader_thread = std::thread(&Navigator::load_songs_inline_async, this,
                                        *pending_inline_path, pending_inline_box_def);
#else
            load_songs_inline_async(*pending_inline_path, pending_inline_box_def);
#endif
            inline_state->songs_count = 0; // will be updated as boxes arrive
            pending_inline_path.reset();
            is_processing = false; // flush_pending_boxes will finalise
            inline_streaming = true;
        }
    }

    if (inline_state && inline_state->fading_out) {
        InlineState& state = *inline_state;
        int end = state.first_song_index + state.songs_count;

        bool all_done = true;
        for (int i = state.first_song_index; i < end; i++) {
            if (!items[i]->fade->is_finished || !genre_bg->is_complete()) { all_done = false; break; }
        }

        if (all_done) {
            pending_inline_folder = nullptr;
            items.erase(items.begin() + state.first_song_index,
                        items.begin() + end);
            items.erase(items.begin() + state.folder_index);
            items.insert(items.begin() + state.folder_index,
                         std::move(state.saved_folder_box));
            open_index = state.folder_index;
            inline_state.reset();
            set_positions(false, 500);
            items[open_index]->exit_box();
            genre_bg.reset();
            is_processing = false;
        }
    }

    for (auto& box : items) {
        bool on_screen = vertical_gallery
            ? (box->position > -100 && box->position < tex.screen_height + 100)
            : (box->position > -100 && box->position < tex.screen_width  + 100);
        if (on_screen && !box->text_loaded)
            box->load_text();
        box->update(current_ms);
    }
}

float Navigator::get_diff_fade_in() {
    SongBox* current_item = dynamic_cast<SongBox*>(get_current_item());
    if (!current_item || !current_item->diff_fade_in) return 0.0f;
    return current_item->diff_fade_in->attribute;
}

void Navigator::draw_background() {
    if (script && script->draw_background(this)) return;

    int width = tex.textures[BOX::BACKGROUND]->width;
    for (int i = 0; i < width * 4; i += width) {
        tex.draw_texture(BOX::BACKGROUND, {.frame=(int)last_bg_genre_index, .x=(float)(i - background_move->attribute)});
        tex.draw_texture(BOX::BACKGROUND, {.frame=(int)bg_genre_index,  .x=(float)(i - background_move->attribute), .fade=1.0f - background_fade_change->attribute});
    }
}

void Navigator::draw() {
    if (!vertical_gallery && genre_bg.has_value()) {
        float start_pos;
        float end_pos;

        if ((!items.empty() && (is_processing || !items[open_index]->fade->is_finished)) &&
            pending_inline_folder != nullptr && genre_bg_end_pos.has_value()) {
            start_pos = pending_inline_folder->left_bound;
            end_pos = genre_bg_end_pos.value();  // approximation while loading
        } else if (genre_bg_start < (int)items.size() && genre_bg_end < (int)items.size()) {
            start_pos = items[genre_bg_start]->left_bound;
            end_pos = items[genre_bg_end]->right_bound;
        } else {
            // The wheel was rebuilt under the band; nothing to anchor it to.
            genre_bg.reset();
        }

        if (genre_bg.has_value()) {
            FolderBox* folder = pending_inline_folder;
            genre_bg->draw(start_pos, end_pos, folder);
        }
    }
    for (auto& box : items) {
        bool on_screen = vertical_gallery
            ? (box->position > -100 && box->position < tex.screen_height + 100)
            : (box->position > -100 && box->position < tex.screen_width  + 100);
        if (on_screen) {
            if (!script || !script->draw_box(box.get()))
                box->draw();
        }
    }
}

void Navigator::draw_diff_select_bg() {
    if (open_index >= items.size()) return;
    BaseBox* box = items[open_index].get();
    if (!script || !script->draw_box_bg(box))
        box->draw_diff_select_bg();
}

void Navigator::draw_score_history() {
    if (open_index < items.size() && dynamic_cast<SongBox*>(items[open_index].get()) != nullptr)
        items[open_index]->draw_score_history();
}

Statistics Navigator::get_statistics(const fs::path& path) {
    Statistics stats;

    for (int course = 0; course <= 4; course++)
        for (int level = 1; level <= 10; level++)
            stats[course][level] = CourseStats{};

    for (const auto& sibling : fs::directory_iterator(path)) {
        if (!fs::is_directory(sibling) || sibling.path() == path) continue;

        for (const auto& entry : fs::recursive_directory_iterator(sibling)) {
            if (!is_song_file(entry.path())) continue;

            const auto& hashes = scores_manager.get_hashes(entry.path());

            SongParser parser(entry.path());
            parser.get_metadata();

            for (const auto& [course, data] : parser.metadata.course_data) {
                if (course < 0 || course > 4) continue;
                int level = static_cast<int>(data.level);
                if (level < 1 || level > 10) continue;

                CourseStats& cs = stats[course][level];
                cs.total++;

                std::string hash = hashes[course];
                if (hash.empty()) continue;

                auto score = scores_manager.get_score(hash, course, scores_manager.player_1);
                if (!score.has_value()) continue;

                if (score->crown >= Crown::FC)
                    cs.full_combos++;
                if (score->crown >= Crown::CLEAR)
                    cs.clears++;
            }
        }
    }

    return stats;
}

void replace_all(std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) return;

    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

std::string normalize_title(std::string s) {
    replace_all(s, "-New Audio-", "");
    replace_all(s, "-新曲-", "");
    replace_all(s, "-Old Audio-", "");
    replace_all(s, "-旧曲-", "");

    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) {
        return !std::isalnum(c);
    }), s.end());

    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    return s;
}

std::optional<fs::path> Navigator::find_song_by_title(const std::string& title, const std::string& subtitle) {
    std::string norm_title = normalize_title(title);
    std::string norm_subtitle = normalize_title(subtitle);

    for (auto& [key, path] : song_files) {
        if (normalize_title(key.first) == norm_title &&
            normalize_title(key.second) == norm_subtitle) {
            return path;
        }
    }

    // Fallback: title only
    for (auto& [key, path] : song_files) {
        if (normalize_title(key.first) == norm_title) {
            return path;
        }
    }

    for (auto& [key, path] : song_files) {
        std::string indexed_title = normalize_title(key.first);

        // Check if norm_title is inside indexed_title or vice-versa
        if (indexed_title.find(norm_title) != std::string::npos ||
            norm_title.find(indexed_title) != std::string::npos) {

            // Safety: Don't match tiny fragments like "a" or "the"
            if (norm_title.length() < 3 && indexed_title != norm_title) continue;

            return path;
        }
    }

    return std::nullopt;
}

Navigator navigator;
