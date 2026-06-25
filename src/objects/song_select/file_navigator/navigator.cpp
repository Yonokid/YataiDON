#include "navigator.h"
#include "box_song_osu.h"
#include "box_back.h"
#include "color_utils.h"
#include "../../../libs/filesystem.h"
#include <random>

static std::unique_ptr<SongBox> make_song_box(const fs::path& path, const BoxDef& box_def, SongParser parser) {
    if (path.extension() == ".osu")
        return std::make_unique<SongBoxOsu>(path, box_def, std::move(parser));
    return std::make_unique<SongBox>(path, box_def, std::move(parser));
}

static std::unique_ptr<BackBox> make_back_box(const fs::path& parent_path) {
    BoxDef d;
    d.back_color    = BackBox::COLOR;
    d.fore_color    = BackBox::COLOR;
    d.texture_index = TextureIndex::NONE;
    d.genre_index   = GenreIndex::NAMCO;
    return std::make_unique<BackBox>(parent_path, d);
}

static void apply_song_color(SongBox* song, const BoxDef& box_def) {
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

void Navigator::preload(std::vector<fs::path> songs_paths) {
    if (is_preloaded) return;
    root_paths = songs_paths;
    open_index = 0;

    song_files_thread = std::thread([this, songs_paths]() {
        for (const fs::path& root_path : songs_paths) {
            try {
                std::error_code ec;
                auto it = fs::recursive_directory_iterator(root_path, fs::directory_options::skip_permission_denied, ec);
                while (it != fs::end(it)) {
                    try {
                        if (is_song_file(it->path())) {
                            SongParser parsed_entry = SongParser(it->path());
                            parsed_entry.get_metadata();
                            song_files[{parsed_entry.metadata.title["en"], parsed_entry.metadata.subtitle["en"]}] = it->path();
                        }
                    } catch (const std::exception& inner) {
                        spdlog::warn("Skipping song during scan: {}", inner.what());
                    }
                    it.increment(ec);
                    if (ec) { spdlog::warn("Skipping entry: {}", ec.message()); ec.clear(); }
                }
            } catch (const fs::filesystem_error& e) {
                spdlog::error("Error scanning song directory: {}", e.what());
            }
        }
    });

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
    if (!is_init) {
        if (!is_preloaded) {
            preload(songs_paths);
        }

        for (const fs::path& root_path : root_paths) {
            load_current_directory(root_path);
        }
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
            for (const fs::path& root_path : root_paths) {
                load_current_directory(root_path);
            }
        } else {
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

void Navigator::join_loader() {
    abort_loading = true;
    if (loader_thread.joinable())
        loader_thread.join();
    abort_loading = false;
}

void Navigator::enqueue_box(std::unique_ptr<BaseBox> box) {
    auto last_write = fs::last_write_time(box->path);
    auto last_write_sys = ch::system_clock::now() +
        std::chrono::duration_cast<ch::system_clock::duration>(
            last_write - std::filesystem::file_time_type::clock::now());
    auto two_weeks_ago = ch::system_clock::now() - ch::weeks(2);
    if (last_write_sys < two_weeks_ago)
        box->is_new = true;

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
                return a->text_name < b->text_name;
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
            for (int i = 1; i < (int)items.size(); i++) {
                if (!items[i]->preserve_order) {
                    sortable_indices.push_back(i);
                    sortable.push_back(std::move(items[i]));
                }
            }
            std::sort(sortable.begin(), sortable.end(),
                [](const std::unique_ptr<BaseBox>& a, const std::unique_ptr<BaseBox>& b) {
                    return a->path.filename().string() < b->path.filename().string();
                });
            for (int j = 0; j < (int)sortable_indices.size(); j++)
                items[sortable_indices[j]] = std::move(sortable[j]);
        }
        set_positions(false, 800);
        if (!items.empty()) items[open_index]->expand_box();
        is_processing = false;
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
    wait_for_song_files();
    BoxDef box_def = parse_box_def(path);

    setup_back_box(path, true);

    try {
        for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
            if (abort_loading) break;
            const fs::path& curr_path = entry.path();
            try {
                if (!fs::is_directory(curr_path)) {
                    if (curr_path.filename() == "song_list.txt") {
                        BoxDef entry_box_def = parse_box_def(curr_path);
                        parse_song_list(curr_path, entry_box_def, false);
                        continue;
                    }
                    if (is_song_file(curr_path))
                        enqueue_box(make_song_box(curr_path, box_def, SongParser(curr_path)));
                    continue;
                }
                if (has_def_file(curr_path)) {
                    BoxDef entry_box_def = parse_box_def(curr_path);
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
    }
    loading_complete = true;
    current_path = path;
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
            apply_song_color(song.get(), sibling_box_def);
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
            apply_song_color(song.get(), sibling_box_def);
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
        if (mark_favorite) song->is_favorite = true;
        fs::path genre_folder = find_box_def_folder(song_path);
        if (!genre_folder.empty())
            apply_song_color(song.get(), parse_box_def(genre_folder));
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
        songs_added++;
    }
}

void Navigator::toggle_favorite(SongBox* song) {
    if (!favorite_folder_path) return;
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
}

void Navigator::load_collection_recommended(const fs::path& path, const BoxDef& box_def) {
    std::vector<std::pair<fs::path, BoxDef>> all_songs;
    for (const auto& sibling : fs::directory_iterator(path.parent_path())) {
        if (abort_loading) break;
        if (!fs::is_directory(sibling) || sibling.path() == path) continue;
        BoxDef sibling_box_def = parse_box_def(sibling.path());
        for (const auto& entry : fs::recursive_directory_iterator(sibling)) {
            if (abort_loading) break;
            if (is_song_file(entry.path()))
                all_songs.push_back({entry.path(), sibling_box_def});
        }
    }
    std::mt19937 rng(std::random_device{}());
    std::shuffle(all_songs.begin(), all_songs.end(), rng);
    int count = std::min((int)all_songs.size(), 10);
    for (int i = 0; i < count; i++) {
        if (abort_loading) break;
        const auto& [song_path, song_box_def] = all_songs[i];
        auto song = make_song_box(song_path, box_def, SongParser(song_path));
        apply_song_color(song.get(), song_box_def);
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
            apply_song_color(song.get(), parse_box_def(genre_folder));
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
        songs_added++;
    }
}

void Navigator::load_songs_inline_async(const fs::path path, BoxDef box_def) {
    wait_for_song_files();
    int songs_added = 0;

    auto add_song = [&](const fs::path& song_path) {
        if (songs_added > 0 && songs_added % 10 == 0)
            enqueue_inline_box(make_back_box(path.parent_path()));
        auto box = make_song_box(song_path, box_def, SongParser(song_path));
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
                        enqueue_inline_box(make_song_box(curr_path, box_def, SongParser(curr_path)));
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
        if (entry.is_directory(ec) && has_def_file(entry.path())) {
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

    if (!has_children && !items.empty()) {
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
    loader_thread = std::thread(&Navigator::load_current_directory_async, this, path);
}

void Navigator::setup_back_box(const fs::path& path, bool has_children) {
    auto back = make_back_box(path.parent_path());
    if (has_children) {
        items.clear();
        items.push_back(std::move(back));
    } else {
        back->fade_in(266);
        items.erase(items.begin() + open_index);
        items.insert(items.begin() + open_index, std::move(back));
    }
}

bool Navigator::has_child_folders(const fs::path& path) {
    for (const auto& entry : fs::directory_iterator(path))
        if (fs::is_directory(entry.path()) && has_def_file(entry.path()) || is_osu_song_folder(entry.path()))
            return true;
    return false;
}

bool Navigator::is_directory(BaseBox* item) {
    return fs::is_directory(item->path);
}

bool Navigator::is_song_file(const fs::path& path) {
    if (!fs::is_regular_file(path)) return false;
    auto ext = path.extension();
    return ext == ".tja" || ext == ".osu";
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
    return is_song_file(item->path);
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
            float center_y     = 300 * tex.screen_scale;
            float fixed_x      = 594 * tex.screen_scale;
            position = center_y + offset * base_spacing;
            items[i]->vertical   = true;
            items[i]->cross_pos  = fixed_x;
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
    if (items.empty()) return;
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
            float center_y = 300 * tex.screen_scale;
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
            loader_thread = std::thread(&Navigator::load_songs_inline_async, this,
                                        *pending_inline_path, pending_inline_box_def);
            inline_state->songs_count = 0; // will be updated as boxes arrive
            pending_inline_path.reset();
            is_processing = false; // flush_pending_boxes will finalise
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
        } else {
            start_pos = items[genre_bg_start]->left_bound;
            end_pos = items[genre_bg_end]->right_bound;  // safe, loading done
        }

        FolderBox* folder = pending_inline_folder;
        genre_bg->draw(start_pos, end_pos, folder);
    }
    for (auto& box : items) {
        bool on_screen = vertical_gallery
            ? (box->position > -100 && box->position < tex.screen_height + 100)
            : (box->position > -100 && box->position < tex.screen_width  + 100);
        if (on_screen) {
            box->draw();
        }
    }
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
