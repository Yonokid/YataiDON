#include "box_folder.h"

#include "../../../libs/audio/audio.h"

FolderBox::FolderBox(const fs::path& path, const BoxDef& box_def) : BaseBox(path, box_def) {
    this->text_name = box_def.name;
    enter_fade = std::make_unique<FadeAnimation>(166);
}

FolderBox::~FolderBox() {
    if (box_texture.has_value()) {
        unload_texture(box_texture.value());
    }
}

void FolderBox::load_text() {
    BaseBox::load_text();
    if (is_osu_folder) {
        std::error_code dir_ec;
        auto it = fs::directory_iterator(path, dir_ec);
        auto end = fs::directory_iterator();
        while (!dir_ec && it != end && it->path().extension() != ".jpg" && it->path().extension() != ".png") {
            it++;
        }
        if (!dir_ec && it != end) {
            box_texture = load_texture_mipmapped((it->path()).string().c_str());
        }
    } else if (fs::exists(fs::path(path / "box.png")) && !box_texture.has_value()) {
        box_texture = load_texture_mipmapped((path / "box.png").string().c_str());
    }
    text_loaded = true;
}

void FolderBox::update(double current_time) {
    bool is_open_prev = yellow_box_opened;
    enter_fade->update(current_time);
    BaseBox::update(current_time);

    std::string genre_voice_str = "genre_voice_" + std::to_string(static_cast<int>(genre_index));
    if (!is_open_prev && yellow_box_opened) {
        if (!audio.is_sound_playing("voice_enter")) {
            audio.play_sound(genre_voice_str, VolumePreset::VOICE);
            genre_voice_started = true;
        }
    } else if (!yellow_box_opened && genre_voice_started) {
        audio.stop_sound(genre_voice_str);
        genre_voice_started = false;
    }
}

void FolderBox::enter_box() {
    entered = true;
    enter_fade->start();
    if (genre_voice_started) {
        audio.stop_sound("genre_voice_" + std::to_string(static_cast<int>(genre_index)));
    }
}

void FolderBox::exit_box() {
    entered = false;
    enter_fade->reset();
}
