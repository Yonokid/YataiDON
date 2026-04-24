#include "navigator.h"
#include "../../../libs/parsers/song_parser.h"
#include <filesystem>
#include <algorithm>
#include <random>
#include <fstream>
#include <set>
#include <spdlog/spdlog.h>

Navigator::Navigator() {
}

Navigator::~Navigator() {
    join_loader();
}

void Navigator::init(std::vector<fs::path> songs_paths) {
    if (!is_init) {
        root_paths = songs_paths;
        open_index = 0;
        for (fs::path& root_path : songs_paths) {
            try {
                for (const fs::directory_entry& entry : fs::recursive_directory_iterator(
                         root_path, fs::directory_options::skip_permission_denied)) {
                    if (is_song_file(entry)) {
                        SongParser parsed_entry = SongParser(entry.path());
                        parsed_entry.get_metadata();
                        song_files[std::make_pair(parsed_entry.metadata.title["en"], parsed_entry.metadata.subtitle["en"])] = entry.path();
                    }
                }
            } catch (const fs::filesystem_error& e) {
                spdlog::error("Error scanning song directory: {}", e.what());
            }
            for (const auto& entry : fs::directory_iterator(root_path)) {
                if (!fs::is_directory(entry) || !has_def_file(entry.path())) continue;
                BoxDef bd = parse_box_def(entry.path());
                if (bd.collection == "RECENT" && !recent_folder_path)
                    recent_folder_path = entry.path();
                if (bd.collection == "FAVORITE" && !favorite_folder_path) {
                    favorite_folder_path = entry.path();
                    fs::path fav_list = entry.path() / "song_list.txt";
                    if (fs::exists(fav_list)) {
                        std::ifstream in(fav_list);
                        std::string line;
                        while (std::getline(in, line)) {
                            std::vector<std::string> f;
                            std::stringstream ss(line);
                            std::string tok;
                            while (std::getline(ss, tok, '|')) f.push_back(tok);
                            if (f.size() >= 3) favorite_songs.insert(f[1] + "|" + f[2]);
                        }
                    }
                }
            }
            load_current_directory(root_path);
        }
        is_init = true;
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
        get_current_item()->expand_box();
    }
    background_move = (MoveAnimation*)tex.get_animation(0);
    background_fade_change = (FadeAnimation*)tex.get_animation(5);
    bg_genre_index = items[open_index]->genre_index;
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
    std::lock_guard<std::mutex> lock(pending_mutex);
    auto last_write = fs::last_write_time(box->path);
#ifdef __EMSCRIPTEN__
    auto last_write_sys = ch::system_clock::time_point(
        ch::duration_cast<ch::system_clock::duration>(last_write.time_since_epoch())
    );
#else
    auto last_write_sys = ch::clock_cast<ch::system_clock>(last_write);
#endif
    auto two_weeks_ago = ch::system_clock::now() - ch::weeks(2);
    if (last_write_sys < two_weeks_ago)
        box->is_new = true;
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
            if (items[i]->text_name == "BACK_BOX") {
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
        int insert_pos = inline_state->first_song_index + inline_state->songs_count;
        while (!pending_inline_boxes.empty()) {
            items.insert(items.begin() + insert_pos, std::move(pending_inline_boxes.front()));
            pending_inline_boxes.pop();
            inline_state->songs_count++;
            insert_pos++;
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
        items[open_index]->expand_box();
        is_processing = false;
        loading_complete = false;
    }
}

void Navigator::parse_song_list(const fs::path& path, BoxDef box_def, bool inline_mode) {
    std::ifstream file(path);
    std::string line;
    std::vector<std::string> out_lines;
    bool needs_rewrite = false;

    int songs_added = 0;
    while (std::getline(file, line)) {
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, '|')) fields.push_back(field);
        if (fields.size() < 3) { out_lines.push_back(line); continue; }

        std::string hash     = fields[0];
        std::string title    = fields[1];
        std::string subtitle = fields[2];

        fs::path song_path;
        if (auto found = scores_manager.get_path_by_hash(hash)) {
            song_path = *found;
            out_lines.push_back(line);
        } else {
            auto it = song_files.find({title, subtitle});
            if (it == song_files.end()) { out_lines.push_back(line); continue; }
            song_path = it->second;
            std::string correct_hash = scores_manager.get_single_hash(song_path);
            out_lines.push_back(correct_hash + "|" + title + "|" + subtitle);
            needs_rewrite = true;
        }

        auto box = std::make_unique<SongBox>(song_path, box_def, SongParser(song_path));
        if (songs_added > 0 && songs_added % 10 == 0) {
            BoxDef back_box_def;
            back_box_def.back_color    = BackBox::COLOR;
            back_box_def.fore_color    = BackBox::COLOR;
            back_box_def.texture_index = TextureIndex::NONE;
            back_box_def.genre_index   = GenreIndex::NAMCO;
            enqueue_inline_box(std::make_unique<BackBox>(path.parent_path().parent_path(), back_box_def));
        }
        if (inline_mode) {
            enqueue_inline_box(std::move(box));
        } else {
            box->preserve_order = true;
            enqueue_box(std::move(box));
        }
        songs_added++;
    }
    file.close();

    if (needs_rewrite) {
        std::ofstream out(path, std::ios::trunc);
        for (const auto& l : out_lines) out << l << "\n";
    }
}

void Navigator::load_current_directory_async(const fs::path path) {
    BoxDef box_def = parse_box_def(path);

    setup_back_box(path, true);

    for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
        if (abort_loading) break;
        const fs::path& curr_path = entry.path();
        if (!fs::is_directory(curr_path)) {
            if (curr_path.filename() == "song_list.txt") {
                BoxDef entry_box_def = parse_box_def(curr_path);
                parse_song_list(curr_path, entry_box_def, false);
                continue;
            }
            if (is_song_file(curr_path))
                enqueue_box(std::make_unique<SongBox>(curr_path, box_def, SongParser(curr_path)));
            continue;
        }
        if (has_def_file(curr_path)) {
            BoxDef entry_box_def = parse_box_def(curr_path);
            enqueue_box(std::make_unique<FolderBox>(curr_path, entry_box_def, get_tja_count(curr_path), song_files));
        } else {
            for (const auto& song : fs::recursive_directory_iterator(curr_path)) {
                if (abort_loading) break;
                if (is_song_file(song.path()))
                    enqueue_box(std::make_unique<SongBox>(song.path(), box_def, SongParser(song.path())));
            }
        }
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
#ifdef __EMSCRIPTEN__
            auto last_write_sys = ch::system_clock::time_point(
                ch::duration_cast<ch::system_clock::duration>(last_write.time_since_epoch())
            );
#else
            auto last_write_sys = ch::clock_cast<ch::system_clock>(last_write);
#endif
            if (last_write_sys < two_weeks_ago) continue;
            if (songs_added > 0 && songs_added % 10 == 0) {
                BoxDef back_box_def;
                back_box_def.back_color    = BackBox::COLOR;
                back_box_def.fore_color    = BackBox::COLOR;
                back_box_def.texture_index = TextureIndex::NONE;
                back_box_def.genre_index   = GenreIndex::NAMCO;
                enqueue_inline_box(std::make_unique<BackBox>(path.parent_path(), back_box_def));
            }
            auto song = std::make_unique<SongBox>(entry.path(), box_def, SongParser(entry.path()));
            if (sibling_box_def.fore_color.has_value())
                song->fore_color = sibling_box_def.fore_color;
            else if (sibling_box_def.back_color.has_value())
                song->fore_color = darken_color(sibling_box_def.back_color.value());
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
            if (songs_added > 0 && songs_added % 10 == 0) {
                BoxDef back_box_def;
                back_box_def.back_color    = BackBox::COLOR;
                back_box_def.fore_color    = BackBox::COLOR;
                back_box_def.texture_index = TextureIndex::NONE;
                back_box_def.genre_index   = GenreIndex::NAMCO;
                enqueue_inline_box(std::make_unique<BackBox>(path.parent_path(), back_box_def));
            }
            auto song = std::make_unique<SongBox>(entry.path(), box_def, parser);
            if (sibling_box_def.fore_color.has_value())
                song->fore_color = sibling_box_def.fore_color;
            else if (sibling_box_def.back_color.has_value())
                song->fore_color = darken_color(sibling_box_def.back_color.value());
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

void Navigator::load_collection_favorite(const fs::path& path, const BoxDef& box_def) {
    fs::path song_list = path / "song_list.txt";
    if (!fs::exists(song_list)) return;
    std::ifstream file(song_list);
    std::string line;
    int songs_added = 0;
    while (std::getline(file, line)) {
        if (abort_loading) break;
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, '|')) fields.push_back(field);
        if (fields.size() < 3) continue;
        fs::path song_path;
        if (auto found = scores_manager.get_path_by_hash(fields[0])) {
            song_path = *found;
        } else {
            auto it = song_files.find({fields[1], fields[2]});
            if (it == song_files.end()) continue;
            song_path = it->second;
        }
        if (songs_added > 0 && songs_added % 10 == 0) {
            BoxDef back_box_def;
            back_box_def.back_color    = BackBox::COLOR;
            back_box_def.fore_color    = BackBox::COLOR;
            back_box_def.texture_index = TextureIndex::NONE;
            back_box_def.genre_index   = GenreIndex::NAMCO;
            enqueue_inline_box(std::make_unique<BackBox>(path.parent_path(), back_box_def));
        }
        auto song = std::make_unique<SongBox>(song_path, box_def, SongParser(song_path));
        song->is_favorite = true;
        fs::path genre_folder = find_box_def_folder(song_path);
        if (!genre_folder.empty()) {
            BoxDef genre_box_def = parse_box_def(genre_folder);
            if (genre_box_def.fore_color.has_value())
                song->fore_color = genre_box_def.fore_color;
            else if (genre_box_def.back_color.has_value())
                song->fore_color = darken_color(genre_box_def.back_color.value());
        }
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
    std::string entry    = scores_manager.get_single_hash(song->parser.file_path) + "|" + title + "|" + subtitle;

    fs::path song_list = *favorite_folder_path / "song_list.txt";
    std::vector<std::string> lines;
    if (fs::exists(song_list)) {
        std::ifstream in(song_list);
        std::string line;
        while (std::getline(in, line))
            if (!line.empty()) lines.push_back(line);
    }

    bool was_favorite = favorite_songs.count(key) > 0;
    if (was_favorite) {
        favorite_songs.erase(key);
        lines.erase(std::remove(lines.begin(), lines.end(), entry), lines.end());
        song->is_favorite = false;
    } else {
        favorite_songs.insert(key);
        lines.insert(lines.begin(), entry);
        song->is_favorite = true;
    }

    std::ofstream out(song_list, std::ios::trunc);
    for (const auto& l : lines) out << l << "\n";
}

fs::path Navigator::find_box_def_folder(const fs::path& song_path) {
    fs::path current = song_path.parent_path();
    while (!current.empty() && current != current.parent_path()) {
        if (fs::exists(current / "box.def")) return current;
        current = current.parent_path();
    }
    return fs::path{};
}

void Navigator::load_collection_recent(const fs::path& path, const BoxDef& box_def) {
    fs::path song_list = path / "song_list.txt";
    if (!fs::exists(song_list)) return;
    std::ifstream file(song_list);
    std::string line;
    int songs_added = 0;
    while (std::getline(file, line)) {
        if (abort_loading) break;
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, '|')) fields.push_back(field);
        if (fields.size() < 3) continue;
        fs::path song_path;
        if (auto found = scores_manager.get_path_by_hash(fields[0])) {
            song_path = *found;
        } else {
            auto it = song_files.find({fields[1], fields[2]});
            if (it == song_files.end()) continue;
            song_path = it->second;
        }
        if (songs_added > 0 && songs_added % 10 == 0) {
            BoxDef back_box_def;
            back_box_def.back_color    = BackBox::COLOR;
            back_box_def.fore_color    = BackBox::COLOR;
            back_box_def.texture_index = TextureIndex::NONE;
            back_box_def.genre_index   = GenreIndex::NAMCO;
            enqueue_inline_box(std::make_unique<BackBox>(path.parent_path(), back_box_def));
        }
        auto song = std::make_unique<SongBox>(song_path, box_def, SongParser(song_path));
        fs::path genre_folder = find_box_def_folder(song_path);
        if (!genre_folder.empty()) {
            BoxDef genre_box_def = parse_box_def(genre_folder);
            if (genre_box_def.fore_color.has_value())
                song->fore_color = genre_box_def.fore_color;
            else if (genre_box_def.back_color.has_value())
                song->fore_color = darken_color(genre_box_def.back_color.value());
        }
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
        songs_added++;
    }
}

void Navigator::add_to_recent(const SongBox* song) {
    if (!recent_folder_path) return;
    fs::path song_list = *recent_folder_path / "song_list.txt";

    const std::string& lang = global_data.config->general.language;
    auto& titles    = song->parser.metadata.title;
    auto& subtitles = song->parser.metadata.subtitle;
    std::string title    = titles.count("en")    ? titles.at("en")    : titles.begin()->second;
    std::string subtitle = subtitles.count("en") ? subtitles.at("en") : subtitles.begin()->second;
    std::string new_entry = scores_manager.get_single_hash(song->parser.file_path) + "|" + title + "|" + subtitle;

    std::vector<std::string> lines;
    if (fs::exists(song_list)) {
        std::ifstream in(song_list);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            if (line == new_entry) continue; // deduplicate
            lines.push_back(line);
        }
    }
    lines.insert(lines.begin(), new_entry);
    if ((int)lines.size() > 25) lines.resize(25);

    std::ofstream out(song_list, std::ios::trunc);
    for (const auto& l : lines) out << l << "\n";
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
        auto song = std::make_unique<SongBox>(song_path, box_def, SongParser(song_path));
        if (song_box_def.fore_color.has_value())
            song->fore_color = song_box_def.fore_color;
        else if (song_box_def.back_color.has_value())
            song->fore_color = darken_color(song_box_def.back_color.value());
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
        if (songs_added > 0 && songs_added % 10 == 0) {
            BoxDef back_box_def;
            back_box_def.back_color    = BackBox::COLOR;
            back_box_def.fore_color    = BackBox::COLOR;
            back_box_def.texture_index = TextureIndex::NONE;
            back_box_def.genre_index   = GenreIndex::NAMCO;
            enqueue_inline_box(std::make_unique<BackBox>(path.parent_path(), back_box_def));
        }
        auto song = std::make_unique<SongBox>(song_path, box_def, SongParser(song_path));
        fs::path genre_folder = find_box_def_folder(song_path);
        if (!genre_folder.empty()) {
            BoxDef genre_box_def = parse_box_def(genre_folder);
            if (genre_box_def.fore_color.has_value())
                song->fore_color = genre_box_def.fore_color;
            else if (genre_box_def.back_color.has_value())
                song->fore_color = darken_color(genre_box_def.back_color.value());
        }
        song->fade_in(266);
        enqueue_inline_box(std::move(song));
        songs_added++;
    }
}

void Navigator::load_songs_inline_async(const fs::path path, BoxDef box_def) {
    int songs_added = 0;

    auto add_song = [&](const fs::path& song_path) {
        if (songs_added > 0 && songs_added % 10 == 0) {
            BoxDef back_box_def;
            back_box_def.back_color    = BackBox::COLOR;
            back_box_def.fore_color    = BackBox::COLOR;
            back_box_def.texture_index = TextureIndex::NONE;
            back_box_def.genre_index   = GenreIndex::NAMCO;
            enqueue_inline_box(std::make_unique<BackBox>(path.parent_path(), back_box_def));
        }
        auto box = std::make_unique<SongBox>(song_path, box_def, SongParser(song_path));
        box->fade_in(266);
        enqueue_inline_box(std::move(box));
        songs_added++;
    };

    if (box_def.collection == "RECOMMENDED") {
        load_collection_recommended(path, box_def);
        loading_complete = true;
        return;
    } else if (box_def.collection == "FAVORITE") {
        load_collection_favorite(path, box_def);
        loading_complete = true;
        return;
    } else if (box_def.collection == "RECENT") {
        load_collection_recent(path, box_def);
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

    for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
        if (abort_loading) break;
        const fs::path& curr_path = entry.path();
        if (!fs::is_directory(curr_path)) {
            if (curr_path.filename() == "song_list.txt") {
                parse_song_list(curr_path, box_def, true);
                continue;
            }
            if (is_song_file(curr_path))
                enqueue_inline_box(std::make_unique<SongBox>(curr_path, box_def, SongParser(curr_path)));
            continue;
        }
        for (const auto& song : fs::recursive_directory_iterator(curr_path)) {
            if (abort_loading) break;
            if (is_song_file(song.path()))
                add_song(song.path());
        }
    }
    loading_complete = true;
}

BoxDef Navigator::parse_box_def(const fs::path& path) {
    std::ifstream boxDef(path / "box.def");
    std::string line;
    BoxDef result;
    result.name = path.filename().string();
    result.texture_index = TextureIndex::DEFAULT;
    result.genre_index = GenreIndex::DEFAULT;
    result.collection = "";
    while (std::getline(boxDef, line)) {
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
            result.name = get_value("#TITLE:");
        } else if (line.starts_with("#TITLE")) {
            const std::string& lang = global_data.config->general.language;
            std::string lang_upper = lang;
            std::transform(lang_upper.begin(), lang_upper.end(), lang_upper.begin(), ::toupper);
            std::string lang_prefix = "#TITLE" + lang_upper + ":";
            if (line.starts_with(lang_prefix))
                result.name = get_value(lang_prefix);
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
    return result;
}

bool Navigator::has_def_file(const std::filesystem::path& path) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.path().extension() == ".def") return true;
    }
    return false;
}

int Navigator::get_tja_count(const std::filesystem::path& path) {
    int count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.path().extension() == ".tja" || entry.path().extension() == ".osu") count++;
        if (entry.path().filename() == "song_list.txt") {
            std::ifstream file(entry.path());
            std::string line;
            while (std::getline(file, line)) {
                count++;
            }
        }
    }
    return count;
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
                       : get_tja_count(pending_inline_path.value());
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

    if (!has_children) {
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

void Navigator::setup_back_box(const fs::path& path, bool has_children) {
    BoxDef back_box_def;
    back_box_def.back_color = BackBox::COLOR;
    back_box_def.fore_color = BackBox::COLOR;
    back_box_def.texture_index = TextureIndex::NONE;
    back_box_def.genre_index = GenreIndex::NAMCO;
    auto back = std::make_unique<BackBox>(path.parent_path(), back_box_def);
    if (has_children) {
        items.clear();
        items.push_back(std::move(back));
    } else {
        back->fade_in(666);
        items.erase(items.begin() + open_index);
        items.insert(items.begin() + open_index, std::move(back));
    }
}

bool Navigator::has_child_folders(const fs::path& path) {
    for (const auto& entry : fs::directory_iterator(path))
        if (fs::is_directory(entry.path()) && has_def_file(entry.path()))
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

bool Navigator::is_song(BaseBox* item) {
    return is_song_file(item->path);
}

BaseBox* Navigator::get_current_item() {
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

        float base_spacing = 100 * tex.screen_scale;
        float center_offset = 150 * tex.screen_scale;
        float side_offset_l = 0 * tex.screen_scale;
        float side_offset_r = 300 * tex.screen_scale;

        float position = ((594 * tex.screen_scale) - center_offset) + (offset * base_spacing);
        if (position == (594 * tex.screen_scale) - center_offset) {
            position += center_offset;
        } else if (position > (594 * tex.screen_scale) - center_offset) {
            position += side_offset_r;
        } else {
            position -= side_offset_l;
        }
        if (init || std::abs(position - items[i]->position) >= tex.screen_width) {
            items[i]->set_position(position);
        } else {
            items[i]->move_box(position, duration);
        }
    }
}

void Navigator::move_left() {
    items[open_index]->close_box();
    last_bg_genre_index = bg_genre_index;
    open_index = (open_index - 1 + (int)items.size()) % (int)items.size();
    bg_genre_index = items[open_index]->genre_index;
    set_positions(false, 166);
    items[open_index]->expand_box();
    background_fade_change->start();
}

void Navigator::move_right() {
    items[open_index]->close_box();
    last_bg_genre_index = bg_genre_index;
    open_index = (open_index + 1 + (int)items.size()) % (int)items.size();
    bg_genre_index = items[open_index]->genre_index;
    set_positions(false, 166);
    items[open_index]->expand_box();
    background_fade_change->start();
}

void Navigator::skip_left() {
    items[open_index]->close_box();
    last_bg_genre_index = bg_genre_index;
    open_index = (open_index - 10 + (int)items.size()) % (int)items.size();
    bg_genre_index = items[open_index]->genre_index;
    set_positions(true, 166);
    items[open_index]->expand_box();
    background_fade_change->start();
}

void Navigator::skip_right() {
    items[open_index]->close_box();
    last_bg_genre_index = bg_genre_index;
    open_index = (open_index + 10 + (int)items.size()) % (int)items.size();
    bg_genre_index = items[open_index]->genre_index;
    set_positions(true, 166);
    items[open_index]->expand_box();
    background_fade_change->start();
}

void Navigator::enter_diff_select() {
    items[open_index]->enter_box();
    for (int i = 0; i < items.size(); i++) {
        std::unique_ptr<BaseBox>& box = items[i];
        bool on_screen = box->position > -100 && box->position < tex.screen_width + 100;
        if (on_screen and i != open_index) {
            float duration = 800;
            float distance = (150 * tex.screen_scale);
            if (box->position < (594 * tex.screen_scale)) {
                box->move_box(-distance, duration);
            } else {
                box->move_box(tex.screen_width + distance, duration);
            }
            box->fade_out();
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
        bool on_screen = box->position > -100 && box->position < tex.screen_width + 100;
        if (on_screen && !box->text_loaded)
            box->load_text();
        box->update(current_ms);
    }
}

void Navigator::draw(bool is_ura) {
    int width = tex.textures[BOX::BACKGROUND]->width;

    for (int i = 0; i < width * 4; i += width) {
        tex.draw_texture(BOX::BACKGROUND, {.frame=(int)last_bg_genre_index, .x=(float)(i - background_move->attribute)});
        tex.draw_texture(BOX::BACKGROUND, {.frame=(int)bg_genre_index,  .x=(float)(i - background_move->attribute), .fade=1.0f - background_fade_change->attribute});
    }

    if (genre_bg.has_value()) {
        float start_pos;
        float end_pos;

        if ((is_processing || !items[open_index]->fade->is_finished) &&
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
        if (box->position > -100 && box->position < tex.screen_width + 100) {
            box->draw(is_ura);
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

Navigator navigator;
