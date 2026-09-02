#include "navigator.h"
#include "box_song_osu.h"
#include "box_back.h"
#include "color_utils.h"
#include "../song_select_script.h"
#include "../../../libs/filesystem.h"
#ifdef SUPPORT_FUMEN
#include "../../../libs/optional/gen4.h"
#include "../../../libs/optional/gen3.h"
#endif
#include <random>
#include <algorithm>
#include <cmath>
#include <cstdlib>   // r56: std::getenv (YATAIDON_R56_DISABLE gate)

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

static void apply_song_genre(SongBox* song, const BoxDef& box_def) {
    song->song_genre_index = box_def.genre_index;
    if (!box_def.box_color.has_value() && !box_def.back_color.has_value() && !box_def.fore_color.has_value())
        return;
    BoxColors colors = resolve_box_colors(box_def.box_color, box_def.back_color, box_def.fore_color);
    song->fore_color = colors.outline;
    song->text_color = colors.text;
}

namespace ch = std::chrono;

Navigator::Navigator() {
}

Navigator::~Navigator() {
    join_loader();
    if (song_files_thread.joinable())
        song_files_thread.join();
}

void Navigator::emit_wheel_event(int id) {
    wheel_event = id;
    ++wheel_event_seq;
    if (id == WHEEL_EVENT_OPEN_BEGIN || id == WHEEL_EVENT_CLOSE_BEGIN)
        wheel_leg_ms = get_current_ms();
}

bool Navigator::swap_is_early(int begin_id) const {
    if (wheel_event != begin_id) return false;
    return (get_current_ms() - wheel_leg_ms) < kSwapDelayMs;
}

void Navigator::wait_for_song_files() {
    if (song_files_thread.joinable())
        song_files_thread.join();
}

void Navigator::load_all_roots() {
    join_loader();
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
    song_files_ready = false;
    song_files_thread = std::thread([this, songs_paths]() {
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
        song_files_ready = true;
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
    emit_wheel_event(WHEEL_EVENT_SCENE_ENTRY);
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
            if (inline_state.has_value()) {
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
    bg_genre_pending = true;
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

    if (loading_complete.load() && inline_state.has_value() &&
        swap_is_early(WHEEL_EVENT_OPEN_BEGIN))
        return;

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
        if (inline_streaming && inline_state.has_value())
            emit_wheel_event(WHEEL_EVENT_OPEN_SWAP);
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
    BoxDef box_def = parse_box_def(path);

    setup_back_box(path, true);

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
    if (!own_root.empty() && is_a_root &&
        (own_root_is_gen3 || gen4::library_for(own_root))) {
        if (reloading_roots && !only_gen4_songs()) {
            BoxDef gen4_def = box_def;
            gen4_def.name        = own_root.filename().string();
            gen4_def.genre_index = GenreIndex::DEFAULT;
            enqueue_box(std::make_unique<FolderBox>(own_root, gen4_def, song_files));
            loading_complete = true;
            current_path = path;
            return;
        }
        for (const fs::path& root : root_paths) {
            if (!gen4::find_data_root(root).empty()) continue;
            items.push_back(make_back_box(root));
            break;
        }
        try {
            if (own_root_is_gen3) load_gen3_genres(own_root);
            else                   load_gen4_genres(own_root);
        } catch (const std::exception& e) {
            spdlog::error("Error listing arcade genres of {}: {}", own_root.string(), e.what());
        }
        loading_complete = true;
        current_path = path;
        return;
    }
#endif

    if (is_gen3_root(path) && path.filename() == "data" &&
        path.parent_path().filename() == "USRDIR" && !items.empty()) {
        if (auto* back = dynamic_cast<BackBox*>(items.front().get()))
            back->path = path.parent_path().parent_path().parent_path();
    }

    if (is_gen4_root(path) || is_gen3_root(path)) {
        try {
            if (is_gen3_root(path)) load_gen3_genres(path);
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
                fs::path groot = gen3_root_at(curr_path);
                if (is_gen4_root(curr_path) || !groot.empty()) {
                    if (only_gen4_songs()) {
                        if (!groot.empty()) load_gen3_genres(groot);
                        else                load_gen4_genres(curr_path);
                        continue;
                    }
                    BoxDef gen4_def = box_def;
                    gen4_def.name = curr_path.filename().string();
#ifdef SUPPORT_FUMEN
                    if (!groot.empty())
                        if (const gen3::Library* lib = gen3::library_for(groot))
                            gen4_def.name = lib->game_name();
#endif
                    gen4_def.genre_index = GenreIndex::DEFAULT;
                    enqueue_box(std::make_unique<FolderBox>(groot.empty() ? curr_path : groot,
                                                            gen4_def, song_files));
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
                if (has_def_file(curr_path)) {
                    BoxDef entry_box_def = parse_box_def(curr_path);
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

void Navigator::load_collection_difficulty(const fs::path& path, const BoxDef& box_def, int course, int level, int order) {
    struct Hit { fs::path path; BoxDef sibling; bool unmet; };
    std::vector<Hit> hits;

    const int crown_needed = (order == 2) ? (int)Crown::CLEAR
                           : (order == 3) ? (int)Crown::FC
                           : (order == 4) ? (int)Crown::DFC : 0;

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

            bool unmet = false;
            if (crown_needed > 0) {
                unmet = true;
                const auto& hashes = scores_manager.get_hashes(entry.path());
                std::string hash = hashes[std::min(course, 4)];
                if (!hash.empty()) {
                    auto score = scores_manager.get_score(hash, course, scores_manager.player_1);
                    if (score.has_value() && (int)score->crown >= crown_needed) unmet = false;
                }
            }
            hits.push_back(Hit{entry.path(), sibling_box_def, unmet});
        }
    }

    if (crown_needed > 0)
        std::stable_partition(hits.begin(), hits.end(), [](const Hit& h) { return h.unmet; });

    int songs_added = 0;
    for (const auto& h : hits) {
        if (abort_loading) break;
        if (songs_added > 0 && songs_added % 10 == 0)
            enqueue_inline_box(make_back_box(path.parent_path()));
        SongParser parser(h.path);
        parser.get_metadata();
        auto song = make_song_box(h.path, box_def, parser);
        apply_song_genre(song.get(), h.sibling);
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
        songs_added++;
    }
}

void Navigator::apply_diff_sort(int course, int level, int order) {
    last_diff_sort_result = {course, level};
    diff_sort_filter      = std::array<int,3>{course, level, order};
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
    if (load_gen3_genre_songs(path, box_def)) {
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
            load_collection_difficulty(path, box_def, (*diff_sort_filter)[0], (*diff_sort_filter)[1],
                                       (*diff_sort_filter)[2]);
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
    BoxDef result = parse_box_def_uncached(path);
    box_def_cache[key] = result;
    return result;
}

BoxDef Navigator::parse_box_def_uncached(const fs::path& path) {
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
        } else if (line.starts_with("#BOXCOLOR:")) {
            result.box_color = parse_hex_color(get_value("#BOXCOLOR:"));
            result.texture_index = TextureIndex::NONE;
        } else if (line.starts_with("#BACKCOLOR:")) {
            result.back_color = parse_hex_color(get_value("#BACKCOLOR:"));
            result.texture_index = TextureIndex::NONE;
        } else if (line.starts_with("#FORECOLOR:")) {
            result.fore_color = parse_hex_color(get_value("#FORECOLOR:"));
        }
    }
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
        if (is_gen4_root(entry.path()) || !gen3_root_at(entry.path()).empty()) continue;
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
    emit_wheel_event(WHEEL_EVENT_CLOSE_BEGIN);
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
    emit_wheel_event(WHEEL_EVENT_OPEN_BEGIN);
    bg_genre_pending = false;
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

#ifdef SUPPORT_FUMEN
    bool own_path_is_arcade_root = !gen4::find_data_root(path).empty() || !gen3::find_data_root(path).empty();
#else
    bool own_path_is_arcade_root = false;
#endif
    if (has_children && root_paths.size() > 1 && !reloading_roots &&
        std::find(root_paths.begin(), root_paths.end(), path) != root_paths.end() &&
        !own_path_is_arcade_root) {
        if (inline_state.has_value()) {
            collapse_inline_now();
            return;
        }

        {
            fs::path game;
#ifdef SUPPORT_FUMEN
            game = gen4::find_data_root(current_path);
            if (game.empty()) game = gen3::find_data_root(current_path);
#endif
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
    return jump_to_song_path(*song_path_opt);
}

bool Navigator::jump_to_song_path(const fs::path& song_path) {
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
    bg_genre_pending = false;
    set_positions(true, 0);
    items[open_index]->expand_box();
    background_fade_change->start();
    is_processing = false;
    return true;
}

void Navigator::setup_back_box(const fs::path& path, bool has_children) {
    if (has_children) {
        if (!reloading_roots) items.clear();
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
#ifdef SUPPORT_FUMEN
    if (gen4::genre_of_path(path) >= 0) return false;
    if (!gen3::genre_of_path(path).empty()) return false;
#endif
    if (is_gen4_root(path) || is_gen3_root(path)) return true;

    for (const auto& entry : fs::directory_iterator(path)) {
        if (fs::is_directory(entry.path()) && has_def_file(entry.path()) || is_osu_song_folder(entry.path()))
            return true;
        if (is_gen4_root(entry.path()) || !gen3_root_at(entry.path()).empty())
            return true;
    }
    return false;
}

bool Navigator::is_directory(BaseBox* item) {
    if (dynamic_cast<FolderBox*>(item) != nullptr) return true;
    return !is_song(item) && fs::is_directory(item->path);
}

bool Navigator::is_song_file(const fs::path& path) {
    if (!fs::is_regular_file(path)) return false;
    auto ext = path.extension();
    return ext == ".tja" || ext == ".osu";
}

const BoxDef* Navigator::box_def_for_genre(GenreIndex genre) {
    if (!genre_box_defs_built) {
        genre_box_defs_built = true;
        for (const fs::path& root : root_paths) {
            std::error_code ec;
#ifdef SUPPORT_FUMEN
            if (!gen4::find_data_root(root).empty() ||
                !gen3::find_data_root(root).empty()) continue;
#endif
            if (!fs::is_directory(root, ec)) continue;
            for (const auto& entry : fs::directory_iterator(root, ec)) {
                if (abort_loading) return nullptr;
                if (!fs::is_directory(entry.path(), ec)) continue;
                if (is_gen4_root(entry.path()) || !gen3_root_at(entry.path()).empty()) continue;
                if (!has_def_file(entry.path())) continue;
                BoxDef def = parse_box_def(entry.path());
                genre_box_defs.emplace(def.genre_index, def);
            }
        }
    }
    auto it = genre_box_defs.find(genre);
    return it != genre_box_defs.end() ? &it->second : nullptr;
}

bool Navigator::is_gen4_root(const fs::path& path) {
#ifdef SUPPORT_FUMEN
    std::error_code ec;
    if (!fs::is_directory(path, ec)) return false;
    return gen4::find_data_root(path) == path && gen4::library_for(path) != nullptr;
#else
    (void)path;
    return false;
#endif
}

bool Navigator::only_gen4_songs() {
#ifdef SUPPORT_FUMEN
    int games = 0;
    std::error_code ec;
    for (const fs::path& root : root_paths) {
        if (!gen4::find_data_root(root).empty() ||
            !gen3::find_data_root(root).empty()) { games++; continue; }
        if (!fs::is_directory(root, ec)) continue;
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            if (is_gen4_root(entry.path()) || !gen3_root_at(entry.path()).empty()) {
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

void Navigator::load_gen4_genres(const fs::path& data_root) {
#ifdef SUPPORT_FUMEN
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
#else
    (void)data_root;
#endif
}

bool Navigator::load_gen4_genre_songs(const fs::path& path, const BoxDef& box_def) {
#ifdef SUPPORT_FUMEN
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
        box->preserve_order = true;
        box->fade_in(266);
        enqueue_inline_box(std::move(box));
        songs_added++;
    }
    return true;
#else
    (void)path; (void)box_def;
    return false;
#endif
}

bool Navigator::is_gen3_root(const fs::path& path) {
#ifdef SUPPORT_FUMEN
    std::error_code ec;
    if (!fs::is_directory(path, ec)) return false;
    return gen3::find_data_root(path) == path && gen3::library_for(path) != nullptr;
#else
    (void)path;
    return false;
#endif
}

fs::path Navigator::gen3_root_at(const fs::path& path) {
#ifdef SUPPORT_FUMEN
    if (is_gen3_root(path)) return path;
    fs::path nested = path / "USRDIR" / "data";
    if (is_gen3_root(nested)) return nested;
    return {};
#else
    (void)path;
    return {};
#endif
}

bool Navigator::is_gen3_song_folder(const fs::path& path) {
#ifdef SUPPORT_FUMEN
    if (path.parent_path().filename() != "fumen") return false;
    const gen3::Library* library = gen3::library_for(path);
    return library && library->find(path.filename().string()) != nullptr;
#else
    (void)path;
    return false;
#endif
}

void Navigator::load_gen3_genres(const fs::path& data_root) {
#ifdef SUPPORT_FUMEN
    const gen3::Library* library = gen3::library_for(data_root);
    if (!library) return;

    for (const std::string& genre_name : gen3::genres_present(*library)) {
        if (abort_loading) break;
        GenreIndex genre = (GenreIndex)gen3::genre_index_for(genre_name);

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
        auto folder = std::make_unique<FolderBox>(gen3::genre_path(data_root, genre_name),
                                                  def, song_files);
        folder->tja_count = 0;
        for (const gen3::SongEntry& e : library->songs())
            if (e.genre == genre_name) folder->tja_count++;
        enqueue_box(std::move(folder));
    }
#else
    (void)data_root;
#endif
}

bool Navigator::load_gen3_genre_songs(const fs::path& path, const BoxDef& box_def) {
#ifdef SUPPORT_FUMEN
    std::string genre_name = gen3::genre_of_path(path);
    if (genre_name.empty()) return false;

    fs::path data_root = gen3::find_data_root(path);
    const gen3::Library* library = gen3::library_for(data_root);
    if (!library) return false;

    GenreIndex genre = (GenreIndex)gen3::genre_index_for(genre_name);

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
    for (const gen3::SongEntry& listing : library->songs()) {
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
#else
    (void)path; (void)box_def;
    return false;
#endif
}

bool Navigator::is_gen4_song_folder(const fs::path& path) {
#ifdef SUPPORT_FUMEN
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
#else
    (void)path;
    return false;
#endif
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
            float horizontal_spacing = 30 * tex.screen_scale;
            if (const SkinInfo* pitch = tex.skin_entry("song_select_row_pitch")) {
                if (pitch->x > 0) base_spacing = pitch->x;
                if (pitch->y > 0) expand_gap   = pitch->y;
            }
            if (const SkinInfo* curve = tex.skin_entry("song_select_row_curve")) {
                if (curve->x > 0) horizontal_spacing = curve->x;
            }
            position = center_y + offset * base_spacing;
            if (offset > 0)      position += expand_gap;
            else if (offset < 0) position -= expand_gap;
            items[i]->vertical   = true;
            float new_cross = fixed_x + offset * horizontal_spacing;
            if (init || std::abs(position - items[i]->position) >= tex.screen_height) {
                items[i]->set_position(position);
                items[i]->snap_cross(new_cross);
            } else {
                items[i]->glide_cross(new_cross);
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
    emit_wheel_event(WHEEL_EVENT_CURSOR_MOVE);
    items[open_index]->close_box();
    last_bg_genre_index = bg_genre_index;
    bg_genre_pending = false;
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
    emit_wheel_event(WHEEL_EVENT_COURSE_BACK);
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

    if (bg_genre_pending && !items.empty()) {
        GenreIndex cursor_genre = items[open_index]->genre_index;
        if (cursor_genre != bg_genre_index) {
            bg_genre_index      = cursor_genre;
            last_bg_genre_index = cursor_genre;
        }
        if (loading_complete) bg_genre_pending = false;
    }

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

        if (all_done && swap_is_early(WHEEL_EVENT_CLOSE_BEGIN)) all_done = false;

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
            emit_wheel_event(WHEEL_EVENT_CLOSE_SWAP);
        }
    }

    SongBox::service_bgm_resume(current_ms);

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
