#include "navigator.h"
#include <filesystem>

Navigator::Navigator() {
}

void Navigator::init(std::vector<fs::path> songs_paths) {
    if (!is_init) {
        for (fs::path& root_path : songs_paths) {
            load_current_directory(root_path, false);
        }
        is_init = true;
    } else {
        get_current_item()->reset_yellow_box();
    }
}

BoxDef Navigator::parse_box_def(const fs::path& path) {
    std::ifstream boxDef(path / "box.def");
    std::string line;
    BoxDef result;
    result.name = path.filename();
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

bool Navigator::has_subdirectories(const std::filesystem::path& path) {
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

void Navigator::load_current_directory(const fs::path path, bool clear_items) {
    if (clear_items) {
        items.clear();
    }
    BoxDef box_def = parse_box_def(path);
    for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
        const fs::path& curr_path = entry.path();
        if (!fs::is_directory(curr_path)) continue;
        if (has_subdirectories(curr_path)) {
            BoxDef entry_box_def = parse_box_def(entry.path());
            int tja_count = get_tja_count(entry.path());
            items.push_back(std::make_unique<FolderBox>(entry.path(), entry_box_def.back_color, entry_box_def.fore_color, entry_box_def.texture_index, entry_box_def.genre_index, entry_box_def.name, tja_count));
        } else {
            for (const fs::directory_entry& song : fs::directory_iterator(curr_path)) {
                if (is_song_file(song.path())) {
                    TJAParser parser = TJAParser(song.path());
                    items.push_back(std::make_unique<SongBox>(song.path(), box_def.back_color, box_def.fore_color, box_def.texture_index, parser));
                }
            }
        }
    }
    items.push_back(std::make_unique<BackBox>(path.parent_path(), BackBox::COLOR, BackBox::COLOR, TextureIndex::NONE));
    open_index = items.size() / 2;
    set_positions(true, 0);
    items[open_index]->expand_box();
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
    open_index = (open_index - 1 + (int)items.size()) % (int)items.size();
    set_positions(false, 166);
    items[open_index]->expand_box();
}

void Navigator::move_right() {
    items[open_index]->close_box();
    open_index = (open_index + 1 + (int)items.size()) % (int)items.size();
    set_positions(false, 166);
    items[open_index]->expand_box();
}

void Navigator::enter_diff_select() {
    items[open_index]->enter_diff_select();
    for (int i = 0; i < items.size(); i++) {
        std::unique_ptr<BaseBox>& box = items[i];
        bool on_screen = box->position > -100 && box->position < tex.screen_width + 100;
        if (on_screen and i != open_index) {
            float duration = 500;
            float distance = 150;
            if (box->position < 594) {
                box->move_box(-distance, duration);
            } else {
                box->move_box(tex.screen_width + distance, duration);
            }
        }
    }
}

void Navigator::exit_diff_select() {
    items[open_index]->exit_diff_select();
    set_positions(false, 500);
}

void Navigator::update(double current_ms) {
    for (auto& box : items) {
        bool on_screen = box->position > -100 && box->position < tex.screen_width + 100;
        if (on_screen && !box->text_loaded) {
            box->load_text();
        }
        box->update(current_ms);
    }
}

void Navigator::draw() {
    for (auto& box : items) {
        if (box->position > -100 && box->position < tex.screen_width + 100) {
            box->draw();
        }
    }
}

Navigator navigator;
