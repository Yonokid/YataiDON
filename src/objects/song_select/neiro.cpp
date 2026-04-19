#include "neiro.h"

NeiroSelector::NeiroSelector(PlayerNum player_num) : player_num(player_num) {
    selected_sound = global_data.hit_sound[(int)player_num];
    is_finished = false;
    is_confirmed = false;
    direction = -1;

    std::filesystem::path neiro_list_path = std::filesystem::path("Skins")
        / global_data.config->paths.skin
        / "Sounds" / "hit_sounds" / "neiro_list.txt";

    std::ifstream neiro_list(neiro_list_path);
    std::string line;
    while (std::getline(neiro_list, line)) {
        if (!line.empty() && line.back() == '\n') line.pop_back();
        if (!line.empty() && line.back() == '\r') line.pop_back();
        sounds.push_back(line);
    }
    sounds.push_back("無音");

    load_sound();
    audio->play_sound("voice_hitsound_select_" + std::to_string((int)player_num) + "p", "voice");

    move = (MoveAnimation*)tex.get_animation(28, true);
    move->start();
    blue_arrow_fade = (FadeAnimation*)tex.get_animation(29, true);
    blue_arrow_move = (MoveAnimation*)tex.get_animation(30, true);
    move_sideways = (MoveAnimation*)tex.get_animation(31, true);
    fade_sideways = (FadeAnimation*)tex.get_animation(32, true);

    text = std::make_unique<OutlinedText>(sounds[selected_sound], tex.skin_config[SC::NEIRO_TEXT].font_size, ray::WHITE, ray::BLACK, false);
    text_2 = std::make_unique<OutlinedText>(sounds[selected_sound], tex.skin_config[SC::NEIRO_TEXT].font_size, ray::WHITE, ray::BLACK, false);
}

void NeiroSelector::load_sound() {
    if (selected_sound == (int)sounds.size()) return;
    std::filesystem::path base = std::filesystem::path("Skins")
        / global_data.config->paths.skin
        / "Sounds" / "hit_sounds" / std::to_string(selected_sound);
    if (selected_sound == 0) {
        curr_sound = audio->load_sound(base / "don.wav", "hit_sound");
    } else {
        curr_sound = audio->load_sound(base / "don.ogg", "hit_sound");
    }
}

void NeiroSelector::left() {
    if (move->is_started && !move->is_finished) return;
    selected_sound = ((selected_sound - 1) % (int)sounds.size() + (int)sounds.size()) % (int)sounds.size();
    audio->unload_sound(curr_sound);
    load_sound();
    move_sideways->start();
    fade_sideways->start();

    text = std::move(text_2);
    text_2 = std::make_unique<OutlinedText>(sounds[selected_sound], tex.skin_config[SC::NEIRO_TEXT].font_size, ray::WHITE, ray::BLACK, false);

    direction = -1;
    if (selected_sound == (int)sounds.size()) return;
    audio->play_sound(curr_sound, "hitsound");
}

void NeiroSelector::right() {
    if (move->is_started && !move->is_finished) return;
    selected_sound = (selected_sound + 1) % (int)sounds.size();
    audio->unload_sound(curr_sound);
    load_sound();
    move_sideways->start();
    fade_sideways->start();

    text = std::move(text_2);
    text_2 = std::make_unique<OutlinedText>(sounds[selected_sound], tex.skin_config[SC::NEIRO_TEXT].font_size, ray::WHITE, ray::BLACK, false);

    direction = 1;
    if (selected_sound == (int)sounds.size()) return;
    audio->play_sound(curr_sound, "hitsound");
}

void NeiroSelector::confirm() {
    if (move->is_started && !move->is_finished) return;
    if (selected_sound == (int)sounds.size()) {
        global_data.hit_sound[(int)player_num] = -1;
    } else {
        global_data.hit_sound[(int)player_num] = selected_sound;
    }
    is_confirmed = true;
    move->restart();
}

void NeiroSelector::update(double current_ms) {
    move->update(current_ms);
    blue_arrow_fade->update(current_ms);
    blue_arrow_move->update(current_ms);
    move_sideways->update(current_ms);
    fade_sideways->update(current_ms);
    is_finished = move->is_finished && is_confirmed;
}

void NeiroSelector::draw() {
    float y = is_confirmed
        ? tex.skin_config[SC::SONG_SELECT_OFFSET].x + move->attribute
        : -move->attribute;
    float x = ((int)player_num - 1) * tex.skin_config[SC::OPTION_P2].x;

    tex.draw_texture(NEIRO::BACKGROUND, {.x=x, .y=y});
    tex.draw_texture(tex.get_enum("neiro/" + (std::to_string((int)player_num) + "p")), {.x=x, .y=y});
    tex.draw_texture(NEIRO::DIVISOR, {.x=x, .y=y});
    tex.draw_texture(NEIRO::MUSIC_NOTE, {.x=x + ((float)move_sideways->attribute * direction), .y=y, .fade=fade_sideways->attribute});
    tex.draw_texture(NEIRO::MUSIC_NOTE, {.x=x + (direction * -tex.skin_config[SC::OPTION_TEXT_IN].x) + ((float)move_sideways->attribute * direction), .y=y, .fade=1.0f - fade_sideways->attribute});
    tex.draw_texture(NEIRO::BLUE_ARROW, {.x=x - (float)blue_arrow_move->attribute, .y=y, .fade=blue_arrow_fade->attribute});
    tex.draw_texture(NEIRO::BLUE_ARROW, {.mirror="horizontal", .x=x + (tex.skin_config[SC::OPTION_TEXT_IN].x * 2) + (float)blue_arrow_move->attribute, .y=y, .fade=blue_arrow_fade->attribute});

    std::string counter = std::to_string(selected_sound + 1);
    float margin = tex.skin_config[SC::NEIRO_COUNTER_MARGIN].x;
    float total_width = counter.size() * margin;
    for (int i = 0; i < (int)counter.size(); i++) {
        int digit = counter[i] - '0';
        tex.draw_texture(NEIRO::COUNTER, {.frame=digit, .x=x - (total_width / 2) + (i * margin), .y=y});
    }

    counter = std::to_string(sounds.size());
    total_width = counter.size() * margin;
    for (int i = 0; i < (int)counter.size(); i++) {
        int digit = counter[i] - '0';
        tex.draw_texture(NEIRO::COUNTER, {.frame=digit, .x=x - (total_width / 2) + (i * margin) + (60 * tex.screen_scale), .y=y});
    }

    text->draw({
        .x = static_cast<float>(x + tex.skin_config[SC::NEIRO_TEXT].x - (text->width / 2.0f) + (move_sideways->attribute * direction)),
        .y = y + tex.skin_config[SC::NEIRO_TEXT].y,
        .fade = fade_sideways->attribute});
    text_2->draw({
        .x = static_cast<float>(x + (direction * -tex.skin_config[SC::OPTION_TEXT_IN].x) + tex.skin_config[SC::NEIRO_TEXT].x - (text_2->width / 2.0f) + (move_sideways->attribute * direction)),
        .y = y + tex.skin_config[SC::NEIRO_TEXT].y,
        .fade = 1.0f - fade_sideways->attribute});
}
