#include "title.h"

#include <random>

#include "../libs/file/config.h"
#include "../libs/file/filesystem.h"
#include "../libs/global_data.h"
#include "../libs/input/input.h"
#include "../libs/network/network.h"
#include "../libs/network/skin_updater.h"

#ifdef CARD_READER_ENABLED
#include "../libs/optional/card_reader.h"
static std::unique_ptr<card_reader::Reader> g_card_reader;
static double g_next_card_start_ms = 0;
#endif

void TitleScreen::on_screen_start() {
    Screen::on_screen_start();
    kinotake_indicator = KinotakeIndicator("attract");
    announce_mode.clear();
    reset_attract_objects();
    load_videos();
    state = TitleState::OP_VIDEO;
    hit_taiko_text = std::make_unique<OutlinedText>(font_manager.get_text_label("hit_taiko_to_start"),
                                                    font_manager.get_text_info("hit_taiko_to_start")->font_size,
                                                    Color::WHITE, Color::BLACK, false, 4);
    fade_out = dynamic_cast<FadeAnimation*>(tex.get_animation(13));
    text_overlay_fade = dynamic_cast<FadeAnimation*>(tex.get_animation(14));
    if (!fade_out || !text_overlay_fade) {
        throw std::runtime_error("Title skin animations 13 and 14 must be FadeAnimation objects");
    }
    text_overlay_fade->start();
    t_caution = tex.has_texture("tournament/caution") ? tex.get_texture("tournament/caution") : nullptr;

    // Session over: undo card-login access code overrides
    for (int i = 0; i < 2; i++) {
        if (!global_data.card_override[i]) {
            continue;
        }
        (i == 0 ? global_data.config->network.access_code_1 : global_data.config->network.access_code_2) =
            global_data.card_prev_code[i];
        global_data.card_override[i] = false;
    }
    scores_manager.clear_guests();
    global_data.guest_credit = false;
    scores_manager.player_1 = global_data.config->network.access_code_1;
    scores_manager.player_2 = global_data.config->network.access_code_2;
    global_data.card_reader_card_valid = false;
    global_data.card_reader_card_id_hex.clear();

#ifdef CARD_READER_ENABLED
    g_next_card_start_ms = 0;
    // Card reader only matters when online (case 4: offline scan does nothing)
    // Opened on a worker thread; update() starts polling once it is ready
    if (global_data.config && global_data.config->network.online_play && global_data.config->card_reader.enabled) {
        card_reader::reader_opener().request(
            global_data.config->card_reader.port, global_data.config->card_reader.baudrate,
            global_data.config->card_reader.backend, global_data.config->card_reader.device);
    }
#endif

    // Re-check for skin updates every time the title screen opens.
    if (global_data.config && global_data.config->general.skin_updater) {
        skin_updater.start();
    }
}

void TitleScreen::load_videos() {
    op_video_list.clear();
    attract_video_list.clear();
    try {
        fs::path op_path = resolve_skin_path("Videos/op_videos");
        if (fs::exists(op_path)) {
            for (const auto& entry : fs::recursive_directory_iterator(op_path)) {
                if (entry.path().extension() == ".mp4") {
                    op_video_list.push_back(entry.path());
                }
            }
        } else {
            spdlog::warn("op_videos folder not found");
        }
    } catch (const fs::filesystem_error& e) {
        spdlog::warn("Failed to scan op_videos folder: {}", e.what());
    }

    try {
        fs::path attract_path = resolve_skin_path("Videos/attract_videos");
        if (fs::exists(attract_path)) {
            for (const auto& entry : fs::recursive_directory_iterator(attract_path)) {
                if (entry.path().extension() == ".mp4") {
                    attract_video_list.push_back(entry.path());
                }
            }
        } else {
            spdlog::warn("attract_videos folder not found");
        }
    } catch (const fs::filesystem_error& e) {
        spdlog::warn("Failed to scan attract_videos folder: {}", e.what());
    }
}

void TitleScreen::reset_attract_objects() {
    op_video.reset();
    attract_video.reset();
    warning_board.reset();
    attract_scene.reset();
    tournament.reset();
    network_msg.hide();
    gacha_busy = false;
    gacha_waiting = false;
    gacha_puchichara.reset();
    msg_until_ms = 0.0;
}

Screens TitleScreen::on_screen_end(Screens next_screen) {
    reset_attract_objects();
    global_data.title_state = "";
    global_data.title_state_start_ms = 0.0;

#ifdef CARD_READER_ENABLED
    // Clean up card reader
    if (g_card_reader) {
        g_card_reader->stop_polling();
        g_card_reader->led_reset();
        g_card_reader.reset();
    }
#endif

    return Screen::on_screen_end(next_screen);
}

void TitleScreen::scene_manager(double current_ms) {
    if (state == TitleState::OP_VIDEO) {
        if (!op_video.has_value()) {
            if (op_video_list.empty()) {
                state = TitleState::WARNING;
                return;
            }
            std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<size_t> dist(0, op_video_list.size() - 1);
            fs::path chosen = op_video_list[dist(rng)];
            op_video.emplace(chosen);
            op_video->set_volume(global_data.config->volume.attract_mode);
            op_video->start(current_ms);
        }
        op_video->update(current_ms);
        if (op_video->is_finished()) {
            op_video->stop();
            op_video.reset();
            state = TitleState::WARNING;
        }
    } else if (state == TitleState::WARNING) {
        if (!warning_board.has_value()) {
            warning_board.emplace(current_ms);
        }
        warning_board->update(current_ms);
        if (warning_board->is_finished()) {
            warning_board.reset();
            state = TitleState::ATTRACT_VIDEO;
        }
    } else if (state == TitleState::ATTRACT_VIDEO) {
        if (!attract_video.has_value()) {
            if (attract_video_list.empty()) {
                state = TitleState::ATTRACT_CAMERA;
                return;
            }
            std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<size_t> dist(0, attract_video_list.size() - 1);
            fs::path chosen = attract_video_list[dist(rng)];
            attract_video.emplace(chosen);
            attract_video->set_volume(global_data.config->volume.attract_mode);
            attract_video->start(current_ms);
        }
        attract_video->update(current_ms);
        if (attract_video->is_finished()) {
            attract_video->stop();
            attract_video.reset();
            state = TitleState::ATTRACT_CAMERA;
        }
    } else if (state == TitleState::ATTRACT_CAMERA) {
        if (!attract_scene.has_value()) {
            attract_scene.emplace();
        }
        attract_scene->update(current_ms);
        if (attract_scene->is_finished()) {
            attract_scene.reset();
            state = network.is_tournament() ? TitleState::TOURNAMENT : TitleState::OP_VIDEO;
        }
    } else if (state == TitleState::TOURNAMENT) {
        if (!tournament.has_value()) {
            tournament.emplace(current_ms);
        }
        tournament->update(current_ms);
        if (tournament->is_finished() && !gacha_busy) {
            tournament.reset();
            state = TitleState::OP_VIDEO;
        }
    }
}

void TitleScreen::update_announce(double current_ms) {
    auto campaigns = network.active_campaigns();
    std::erase(campaigns, "kinotake");
    std::string mode;
    if (!campaigns.empty()) {
        mode = network.is_tournament() ? "tournament" : kinotake_on ? "" : "campaign";
    }
    if (mode != announce_mode) {
        announce_mode = mode;
        if (!mode.empty()) {
            announce = AttractAnnounce(mode);
        }
    }
    if (!announce_mode.empty()) {
        announce.update(current_ms);
    }
}

std::optional<Screens> TitleScreen::update() {
    if (auto next = Screen::update()) {
        return next;
    }
    double current_ms = get_current_ms();

    text_overlay_fade->update(current_ms);
    fade_out->update(current_ms);
    allnet_indicator.update(current_ms);
    kinotake_on = kinotake_indicator.refresh();
    kinotake_indicator.update(current_ms);
    update_announce(current_ms);
    entry_overlay.update(current_ms);

    if (fade_out->is_finished) {
        return on_screen_end(Screens::ENTRY);
    }

#ifdef CARD_READER_ENABLED
    if (!g_card_reader) {
        if (auto opened = card_reader::reader_opener().take()) {
            g_card_reader = std::move(*opened);
            if (!g_card_reader) {
                spdlog::warn("Failed to initialize card reader");
            }
        }
    }
    if (g_card_reader && !g_card_reader->is_polling() && !fade_out->is_started && current_ms >= g_next_card_start_ms) {
        if (g_card_reader->start_polling()) {
            g_card_reader->set_led_color(255, 255, 255); // White for polling
            spdlog::info("Card reader polling started");
        }

        g_next_card_start_ms = get_current_ms() + 1000;
    }

    if (g_card_reader && g_card_reader->is_polling()) {
        const bool accepts_card = !fade_out->is_started;
        g_card_reader->set_accepting_cards(accepts_card);
        if (auto event = g_card_reader->take_event(); accepts_card && event) {
            global_data.card_reader_card_id_hex = event->card.card_id;
            global_data.card_reader_card_valid = true;

            spdlog::info("Card detected, transitioning to entry screen");
            g_card_reader->set_led_color(0, 255, 0);
            g_card_reader->stop_polling();

            if (!fade_out->is_started) {
                fade_out->start();
                fade_out->unlock_input();
                audio.play_sound("don", VolumePreset::SOUND);
            }
        }
    }
#endif

    scene_manager(current_ms);
    if (!update_gacha(current_ms) && menu_confirm_pressed()) {
        start_entry();
    }
    return std::nullopt;
}

void TitleScreen::start_entry() {
    if (!fade_out->is_started) {
        fade_out->start();
    }
    fade_out->unlock_input();
    audio.play_sound("don", VolumePreset::SOUND);
}

bool TitleScreen::update_gacha(double current_ms) {
    network_msg.update(current_ms);
    float msg_wait_ms = 2000;
    if (state != TitleState::TOURNAMENT || !tournament.has_value()) {
        return false;
    }

    if (auto result = network.take_gacha_result()) {
        gacha_waiting = false;
        if (result->puchichara >= 0) {
            gacha_puchichara = result->puchichara;
            network_msg.show("clear_reading_ok");
        } else {
            network_msg.show("error_" + result->error);
            audio.play_sound("error", VolumePreset::ATTRACT_MODE);
        }
        msg_until_ms = current_ms + msg_wait_ms;
    }
    if (msg_until_ms > 0.0 && current_ms >= msg_until_ms) {
        msg_until_ms = 0.0;
        network_msg.hide();
        if (gacha_puchichara.has_value()) {
            tournament->spin(*gacha_puchichara);
            gacha_puchichara.reset();
        }
    }
    if (gacha_busy && !gacha_waiting && msg_until_ms == 0.0 && tournament->can_spin()) {
        gacha_busy = false;
    }
    if (gacha_busy) {
        return true;
    }

    if (!network.is_tournament() || !tournament->can_spin() || !key_pressed(SDL_SCANCODE_RETURN)) {
        return false;
    }
    gacha_busy = true;
    audio.play_sound("don", VolumePreset::SOUND);
    const std::string& code = global_data.config->network.access_code_1;
    if (code.empty() || code == "0" || code == "1") {
        network_msg.show("error_not_play");
        audio.play_sound("error", VolumePreset::ATTRACT_MODE);
        msg_until_ms = current_ms + msg_wait_ms;
    } else {
        gacha_waiting = true;
        network.spin_gacha(code);
        network_msg.show("clear_reading");
    }
    return true;
}

void TitleScreen::draw() {
    if (state == TitleState::OP_VIDEO && op_video) {
        op_video->draw();
    } else if (state == TitleState::WARNING && warning_board) {
        warning_board->draw();
    } else if (state == TitleState::ATTRACT_VIDEO && attract_video) {
        attract_video->draw();
    } else if (state == TitleState::ATTRACT_CAMERA && attract_scene) {
        attract_scene->draw();
    } else if (state == TitleState::TOURNAMENT && tournament) {
        tournament->draw();
        network_msg.draw();
    }
    if (t_caution && state != TitleState::TOURNAMENT && (network.is_tournament() || network.tournament_offline())) {
        tex.draw_texture(t_caution, {.frame = network.is_tournament() ? 0 : 1});
    }

    if (state != TitleState::OP_VIDEO && state != TitleState::TOURNAMENT) {
        if (kinotake_on) {
            kinotake_indicator.draw();
        }
        if (!announce_mode.empty()) {
            announce.draw();
        }
    }
    draw_rectangle(0, 0, tex.screen_width, tex.screen_height, fade(Color::WHITE, fade_out->attribute));
    coin_overlay.draw();
    allnet_indicator.draw();
    entry_overlay.draw(true);

    hit_taiko_text->draw({.x = static_cast<float>(tex.screen_width * 0.25 - hit_taiko_text->width / 2),
                          .y = font_manager.get_text_info("hit_taiko_to_start")->y,
                          .fade = text_overlay_fade->attribute});
    hit_taiko_text->draw({.x = static_cast<float>(tex.screen_width * 0.75 - hit_taiko_text->width / 2),
                          .y = font_manager.get_text_info("hit_taiko_to_start")->y,
                          .fade = text_overlay_fade->attribute});
}
