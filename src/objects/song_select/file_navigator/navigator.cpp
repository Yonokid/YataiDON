#include "navigator.h"
#include <filesystem>

Navigator::Navigator() {
}

void Navigator::init(std::vector<fs::path> songs_paths) {
    if (!is_init) {
        open_index = 0;
        for (fs::path& root_path : songs_paths) {
            load_current_directory(root_path);
        }
        is_init = true;
    } else {
        for (auto& item : items) {
            item->reset();
            item->fade_in(0);
        }
        if (pending_inline_folder != nullptr) pending_inline_folder->reset();
        set_positions(false, 0);
        get_current_item()->expand_box();
    }
    background_move = (MoveAnimation*)tex.get_animation(0);
    background_fade_change = (FadeAnimation*)tex.get_animation(5);
    bg_genre_index = items[open_index]->genre_index;
    last_bg_genre_index = bg_genre_index;
    if (genre_bg.has_value()) genre_bg->fade_in();
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
        } else if (line.starts_with("#TITLEJA:")) {
            if (global_data.config->general.language == "ja")
                result.name = get_value("#TITLEJA:");
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
        if (entry.path().extension() == ".tja") count++;
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

void Navigator::load_songs_inline(const fs::path& path, const BoxDef& box_def) {
    int insert_pos = open_index + 1;
    int songs_added = 0;

    auto add_song = [&](const fs::path& song_path) {
        auto box = std::make_unique<SongBox>(song_path, box_def, TJAParser(song_path));

        if (songs_added > 0 && songs_added % 10 == 0) {
            BoxDef back_box_def;
            back_box_def.back_color = BackBox::COLOR;
            back_box_def.fore_color = BackBox::COLOR;
            back_box_def.texture_index = TextureIndex::NONE;
            back_box_def.genre_index = GenreIndex::NAMCO;
            auto back = std::make_unique<BackBox>(path.parent_path(), back_box_def);
            items.insert(items.begin() + insert_pos++, std::move(back));
        }

        box->fade_in(466);
        items.insert(items.begin() + insert_pos++, std::move(box));
        songs_added++;
    };

    for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
        const fs::path& curr_path = entry.path();
        if (!fs::is_directory(curr_path)) {
            if (is_song_file(curr_path))
                items.insert(items.begin() + insert_pos++,
                    std::make_unique<SongBox>(curr_path, box_def, TJAParser(curr_path)));
            continue;
        }
        for (const auto& song : fs::recursive_directory_iterator(curr_path))
            if (is_song_file(song.path()))
                add_song(song.path());
    }
}

void Navigator::load_current_directory(const fs::path path) {
    BoxDef box_def = parse_box_def(path);
    bool has_children = has_child_folders(path);

    if (!has_children) {
        items[open_index]->enter_box();
        InlineState state;
        state.folder_index   = open_index;
        state.first_song_index = open_index + 1;
        state.songs_count    = 0;
        pending_inline_folder  = static_cast<FolderBox*>(items[open_index].get());
        inline_state           = std::move(state);
        pending_inline_path    = path;
        pending_inline_box_def = box_def;
        float approx_distance = get_tja_count(pending_inline_path.value()) * 100;
        genre_bg_start = 0;
        genre_bg_end = 0;
        genre_bg.emplace(items[open_index]->text_name, items[open_index]->back_color, items[open_index]->texture_index, approx_distance);
        is_processing = true;
        for (int i = 0; i < items.size(); i++) {
            if (items[i]->position > items[open_index]->position) {
                items[i]->move_box(tex.screen_width + 150, 600);
            }
        }
        return;
    }

    if (is_inline) {
        exit_inline();
        return;
    }

    setup_back_box(path, true);

    for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
        const fs::path& curr_path = entry.path();
        if (!fs::is_directory(curr_path)) {
            if (is_song_file(curr_path))
                items.push_back(std::make_unique<SongBox>(curr_path, box_def, TJAParser(curr_path)));
            continue;
        }
        if (has_def_file(curr_path)) {
            BoxDef entry_box_def = parse_box_def(curr_path);
            items.push_back(std::make_unique<FolderBox>(curr_path, entry_box_def, get_tja_count(curr_path)));
        } else {
            for (const auto& song : fs::recursive_directory_iterator(curr_path))
                if (is_song_file(song.path()))
                    items.push_back(std::make_unique<SongBox>(song.path(), box_def, TJAParser(song.path())));
        }
    }

    set_positions(false, 800);
    items[open_index]->expand_box();
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
    return fs::is_regular_file(path) && path.extension() == ".tja";
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

        float position = (594 - center_offset) + (offset * base_spacing);
        if (position == 594 - center_offset) {
            position += center_offset;
        } else if (position > 594 - center_offset) {
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
    set_positions(false, 166);
    items[open_index]->expand_box();
    background_fade_change->start();
}

void Navigator::skip_right() {
    items[open_index]->close_box();
    last_bg_genre_index = bg_genre_index;
    open_index = (open_index + 10 + (int)items.size()) % (int)items.size();
    bg_genre_index = items[open_index]->genre_index;
    set_positions(false, 166);
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
            float distance = 150;
            if (box->position < 594) {
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
    if (genre_bg.has_value()) genre_bg->update(current_ms, pending_inline_folder);
    background_move->update(current_ms);
    background_fade_change->update(current_ms);
    // --- Pending inline load: wait for enter_fade to finish ---
    if (pending_inline_path) {
        if (genre_bg->is_finished()) {
            inline_state->saved_folder_box = std::unique_ptr<FolderBox>(
                static_cast<FolderBox*>(items[open_index].release())
            );
            setup_back_box(*pending_inline_path, false);
            int before = items.size();
            genre_bg_start = inline_state->first_song_index - 1;
            load_songs_inline(*pending_inline_path, pending_inline_box_def);
            is_inline = true;
            inline_state->songs_count = items.size() - before;
            genre_bg_end = inline_state->songs_count + inline_state->first_song_index - 1;
            pending_inline_path.reset();
            set_positions(false, 800);
            items[open_index]->expand_box();
            is_processing = false;
        }
    }

    // --- Inline fade-out: wait for completion then restore ---
    if (inline_state && inline_state->fading_out) {
        InlineState& state = *inline_state;
        int end = state.first_song_index + state.songs_count;

        bool all_done = true;
        for (int i = state.first_song_index; i < end; i++) {
            if (!items[i]->fade->is_finished || !genre_bg->is_complete()) { all_done = false; break; }
        }

        if (all_done) {
            pending_inline_folder = nullptr;
            // Remove all inserted songs/back-boxes
            items.erase(items.begin() + state.first_song_index,
                        items.begin() + end);

            // Restore FolderBox in place of the BackBox
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
    int width = tex.textures["box"]["background"]->width;

    for (int i = 0; i < width * 4; i += width) {
        tex.draw_texture("box", "background", {.frame=(int)last_bg_genre_index, .x=(float)(i - background_move->attribute)});
        tex.draw_texture("box", "background", {.frame=(int)bg_genre_index,  .x=(float)(i - background_move->attribute), .fade=1.0f - background_fade_change->attribute});
    }

    if (genre_bg.has_value()) {
        auto& start_box = items[genre_bg_start];
        auto& end_box = items[genre_bg_end];
        float start_pos = start_box->left_bound;
        float end_pos = end_box->right_bound;
        FolderBox* folder = pending_inline_folder;
        genre_bg->draw(start_pos, end_pos, folder);
    }
    for (auto& box : items) {
        if (box->position > -100 && box->position < tex.screen_width + 100) {
            box->draw(is_ura);
        }
    }
}

Navigator navigator;
