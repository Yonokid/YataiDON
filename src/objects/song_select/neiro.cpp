#include "neiro.h"

#include <fstream>
#include <iterator>

#include "../../libs/audio/audio.h"
#include "../../libs/file/filesystem.h"

std::vector<std::string> read_neiro_names() {
    const std::string& language = global_data.config->general.language;
    const std::vector<fs::path> candidates{
        fs::path("Sounds/hit_sounds") / ("neiro_list_" + language + ".txt"),
        fs::path("Sounds/hit_sounds/neiro_list_ja.txt"),
        fs::path("Sounds/hit_sounds/neiro_list_en.txt"),
        fs::path("Sounds/hit_sounds/neiro_list.txt"),
    };
    for (const fs::path& candidate : candidates) {
        std::ifstream neiro_list(resolve_skin_path(candidate));
        if (!neiro_list.is_open()) {
            continue;
        }
        std::vector<std::string> names;
        std::string line;
        while (std::getline(neiro_list, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                names.push_back(line);
            }
        }
        return names;
    }
    spdlog::error("Failed to open neiro_list.txt");
    return {};
}

NeiroSelector::NeiroSelector(PlayerNum player_num, PlayerData* player) : player_num(player_num), player(player) {
    PlayerSoundScope audio_owner(player_num);
    static int next_instance_id = 0;
    instance_id = ++next_instance_id;
    selected_sound = player->neiro_index;
    is_finished = false;
    is_confirmed = false;
    direction = -1;

    sounds = read_neiro_names();
    sounds.emplace_back(tex.skin_text("modifier_text_muon", global_data.config->general.language));
    if (selected_sound == -1) {
        selected_sound = static_cast<int>(sounds.size()) - 1;
    }
    selected_sound = std::clamp(selected_sound, 0, static_cast<int>(sounds.size()) - 1);
    load_sound();
    audio.play_player_sound("voice_hitsound_select_" + std::to_string(static_cast<int>(player_num)) + "p", player_num,
                            VolumePreset::VOICE);

    move = dynamic_cast<MoveAnimation*>(tex.get_animation(28, true));
    move_out = tex.has_animation(43) ? dynamic_cast<MoveAnimation*>(tex.get_animation(43, true)) : nullptr;
    move->start();
    blue_arrow_fade = dynamic_cast<FadeAnimation*>(tex.get_animation(29, true));
    blue_arrow_move = dynamic_cast<MoveAnimation*>(tex.get_animation(30, true));
    move_sideways = dynamic_cast<MoveAnimation*>(tex.get_animation(31, true));
    fade_sideways = dynamic_cast<FadeAnimation*>(tex.get_animation(32, true));
}

void NeiroSelector::load_sound() {
    if (selected_sound == std::ssize(sounds) - 1) {
        curr_sound.clear();
        return;
    }
    fs::path base = fs::path("Sounds/hit_sounds") / std::to_string(selected_sound);
    fs::path rel = base / (selected_sound == 0 ? "don.wav" : "don.ogg");
    curr_sound = audio.load_sound(resolve_skin_path(rel), "hit_sound");
    if (curr_sound.empty()) {
        spdlog::error("NeiroSelector: failed to load hit sound {}", rel.string());
    }
}

void NeiroSelector::left() {
    if (move->is_started && !move->is_finished) {
        return;
    }
    const auto sound_count = static_cast<int>(sounds.size());
    selected_sound = ((selected_sound - 1) % sound_count + sound_count) % sound_count;
    if (!curr_sound.empty()) {
        audio.unload_sound(curr_sound);
    }
    load_sound();
    move_sideways->start();
    fade_sideways->start();
    change_seq++;

    direction = 1;
    if (selected_sound == std::ssize(sounds) - 1) {
        return;
    }
    audio.play_player_sound(curr_sound, player_num, VolumePreset::HITSOUND);
}

void NeiroSelector::right() {
    if (move->is_started && !move->is_finished) {
        return;
    }
    selected_sound = (selected_sound + 1) % static_cast<int>(sounds.size());
    if (!curr_sound.empty()) {
        audio.unload_sound(curr_sound);
    }
    load_sound();
    move_sideways->start();
    fade_sideways->start();
    change_seq++;

    direction = -1;
    if (selected_sound == std::ssize(sounds) - 1) {
        return;
    }
    audio.play_player_sound(curr_sound, player_num, VolumePreset::HITSOUND);
}

void NeiroSelector::confirm() {
    if (move->is_started && !move->is_finished) {
        return;
    }
    player->neiro_index = selected_sound == std::ssize(sounds) - 1 ? -1 : selected_sound;
    is_confirmed = true;
    if (move_out) {
        move = move_out;
        move->start();
    } else {
        move->restart();
    }
}

void NeiroSelector::update(double current_ms) {
    PlayerSoundScope audio_owner(player_num);
    move->update(current_ms);
    blue_arrow_fade->update(current_ms);
    blue_arrow_move->update(current_ms);
    move_sideways->update(current_ms);
    fade_sideways->update(current_ms);
    is_finished = move->is_finished && is_confirmed;
}
