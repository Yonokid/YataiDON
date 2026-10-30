#include "modifier.h"

#include <algorithm>
#include <iterator>
#include <string_view>

#include "../../libs/audio/audio.h"

const std::array<std::string, 5> ModifierSelector::BASE_MOD_NAMES = {"auto", "speed", "display", "inverse", "random"};

static bool PlayerData::* find_bool_modifier(std::string_view name) {
    struct BoolModifier {
        std::string_view name;
        bool PlayerData::* member;
    };

    static constexpr BoolModifier modifiers[] = {
        {"auto", &PlayerData::modifier_auto},
        {"display", &PlayerData::modifier_display},
        {"inverse", &PlayerData::modifier_inverse},
        {"skip", &PlayerData::modifier_skip},
    };

    const auto* const modifier = std::ranges::find(modifiers, name, &BoolModifier::name);
    return modifier != std::end(modifiers) ? modifier->member : nullptr;
}

bool ModifierSelector::get_bool(int mod_index) {
    const auto member = find_bool_modifier(mod_names[mod_index]);
    return member ? player->*member : false;
}

void ModifierSelector::set_bool(int mod_index, bool value) {
    if (const auto member = find_bool_modifier(mod_names[mod_index])) {
        player->*member = value;
    }
}

ModifierSelector::ModifierSelector(PlayerNum player_num, PlayerData* player) : player_num(player_num), player(player) {
    PlayerSoundScope audio_owner(player_num);
    static int next_instance_id = 0;
    instance_id = ++next_instance_id;
    current_mod_index = 0;
    is_confirmed = false;
    is_finished = false;
    direction = -1;
    language = global_data.config->general.language;

    mod_names.assign(BASE_MOD_NAMES.begin(), BASE_MOD_NAMES.end());
    if (tex.options[SCO::OPTION_SKIP_ROW]) {
        mod_names.emplace_back("skip");
    }
    if (tex.options[SCO::OPTION_NEIRO_ROW]) {
        load_neiro_names();
        mod_names.emplace_back("neiro");
    }

    blue_arrow_fade = dynamic_cast<FadeAnimation*>(tex.get_animation(29, true));
    blue_arrow_move = dynamic_cast<MoveAnimation*>(tex.get_animation(30, true));
    move = dynamic_cast<MoveAnimation*>(tex.get_animation(28, true));
    move_out = tex.has_animation(43) ? dynamic_cast<MoveAnimation*>(tex.get_animation(43, true)) : nullptr;
    move->start();
    move_sideways = dynamic_cast<MoveAnimation*>(tex.get_animation(31, true));
    fade_sideways = dynamic_cast<FadeAnimation*>(tex.get_animation(32, true));

    audio.play_player_sound("voice_options_" + std::to_string(static_cast<int>(player_num)) + "p", player_num,
                            VolumePreset::VOICE);
}

ModifierSelector::~ModifierSelector() {
    if (!neiro_preview.empty()) {
        audio.unload_sound(neiro_preview);
    }
}

void ModifierSelector::load_neiro_names() {
    std::filesystem::path neiro_list_path =
        std::filesystem::path("Skins") / global_data.config->paths.skin / "Sounds" / "hit_sounds" / "neiro_list.txt";

    std::ifstream neiro_list(neiro_list_path);
    if (!neiro_list.is_open()) {
        spdlog::error("Failed to open neiro_list.txt");
    } else {
        std::string line;
        while (std::getline(neiro_list, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                neiro_names.push_back(line);
            }
        }
    }
    neiro_names.push_back(tex.skin_text("modifier_text_muon", language, "無音"));

    neiro_index = player->neiro_index;
    if (neiro_index == -1) {
        neiro_index = static_cast<int>(neiro_names.size()) - 1;
    }
    neiro_index = std::clamp(neiro_index, 0, static_cast<int>(neiro_names.size()) - 1);
    player->neiro_index = (neiro_index == std::ssize(neiro_names) - 1) ? -1 : neiro_index;
}

void ModifierSelector::step_neiro(int dir) {
    int count = static_cast<int>(neiro_names.size());
    neiro_index = ((neiro_index + dir) % count + count) % count;
    player->neiro_index = (neiro_index == count - 1) ? -1 : neiro_index;

    if (!neiro_preview.empty()) {
        audio.unload_sound(neiro_preview);
    }
    neiro_preview.clear();
    if (neiro_index == count - 1) {
        return;
    }

    std::filesystem::path base = std::filesystem::path("Skins") / global_data.config->paths.skin / "Sounds" /
                                 "hit_sounds" / std::to_string(neiro_index);
    neiro_preview = audio.load_sound(base / (neiro_index == 0 ? "don.wav" : "don.ogg"), "hit_sound");
    if (!neiro_preview.empty()) {
        audio.play_player_sound(neiro_preview, player_num, VolumePreset::HITSOUND);
    }
}

void ModifierSelector::update(double current_ms) {
    PlayerSoundScope audio_owner(player_num);
    move->update(current_ms);
    blue_arrow_fade->update(current_ms);
    blue_arrow_move->update(current_ms);
    move_sideways->update(current_ms);
    fade_sideways->update(current_ms);
    is_finished = is_confirmed && move->is_finished;
}

void ModifierSelector::confirm() {
    if (is_confirmed) {
        return;
    }
    current_mod_index++;
    if (current_mod_index == std::ssize(mod_names)) {
        is_confirmed = true;
        if (move_out) {
            move = move_out;
            move->start();
        } else {
            move->restart();
        }
    }
}

void ModifierSelector::start_text_animation(int dir) {
    move_sideways->start();
    fade_sideways->start();
    direction = dir;
    change_seq++;
}

static int speed_step(int value, int dir) {
    if (dir > 0) {
        if (value >= 40) {
            return 1;
        }
        if (value >= 30) {
            return 40;
        }
        if (value >= 20) {
            return 30;
        }
        return value + 1;
    }
    if (value <= 1) {
        return 40;
    }
    if (value <= 20) {
        return value - 1;
    }
    if (value <= 30) {
        return 20;
    }
    return 30;
}

void ModifierSelector::left() {
    if (is_confirmed) {
        return;
    }
    const std::string& mod_name = mod_names[current_mod_index];
    if (row_greyed(mod_name)) {
        return;
    }

    if (mod_name == "speed") {
        player->modifier_speed = speed_step(player->modifier_speed, -1);
        start_text_animation(1);
    } else if (mod_name == "neiro") {
        step_neiro(-1);
        start_text_animation(1);
    } else if (mod_name == "random") {
        player->modifier_random = (player->modifier_random + 2) % 3;
        start_text_animation(1);
    } else {
        set_bool(current_mod_index, !get_bool(current_mod_index));
        start_text_animation(1);
    }
}

void ModifierSelector::right() {
    if (is_confirmed) {
        return;
    }
    const std::string& mod_name = mod_names[current_mod_index];
    if (row_greyed(mod_name)) {
        return;
    }

    if (mod_name == "speed") {
        player->modifier_speed = speed_step(player->modifier_speed, +1);
        start_text_animation(-1);
    } else if (mod_name == "neiro") {
        step_neiro(1);
        start_text_animation(-1);
    } else if (mod_name == "random") {
        player->modifier_random = (player->modifier_random + 1) % 3;
        start_text_animation(-1);
    } else {
        set_bool(current_mod_index, !get_bool(current_mod_index));
        start_text_animation(-1);
    }
}

std::string ModifierSelector::row_label(const std::string& n) const {
    if (std::ranges::contains(BASE_MOD_NAMES, n) || n == "skip" || n == "neiro") {
        return font_manager.get_text_label("modifier_name_" + n);
    }
    return std::string();
}

bool ModifierSelector::row_greyed(const std::string& name) const {
    return name == "skip" && global_data.player_num == PlayerNum::TWO_PLAYER;
}

std::vector<ModifierSelector::ModRow> ModifierSelector::lua_rows() {
    std::vector<ModRow> rows;
    rows.reserve(mod_names.size());
    for (int i = 0; i < std::ssize(mod_names); i++) {
        const std::string& n = mod_names[i];
        ModRow r;
        r.name = n;
        r.label = row_label(n);
        r.greyed = row_greyed(n);
        if (n == "speed") {
            r.value = std::format("{:.1f}", player->modifier_speed / 10.0f);
            r.state = player->modifier_speed;
            r.changed = player->modifier_speed != 10;
        } else if (n == "neiro") {
            r.value = neiro_names.empty() ? std::string() : neiro_names[neiro_index];
            r.state = neiro_index;
            r.changed = player->neiro_index != 0;
        } else if (n == "random") {
            if (player->modifier_random == 1) {
                r.value = font_manager.get_text_label("modifier_text_kimagure");
            } else if (player->modifier_random == 2) {
                r.value = font_manager.get_text_label("modifier_text_detarame");
            } else {
                r.value = font_manager.get_text_label("modifier_text_false");
            }
            r.state = player->modifier_random;
            r.changed = player->modifier_random != 0;
        } else {
            bool v = get_bool(i);
            r.state = v ? 1 : 0;
            r.value = v ? font_manager.get_text_label("modifier_text_true")
                        : font_manager.get_text_label("modifier_text_false");
            r.changed = v;
        }
        r.enabled = !is_confirmed && i >= current_mod_index;
        rows.push_back(std::move(r));
    }
    return rows;
}
