#include "entry.h"

#include <algorithm>
#include <thread>

#include "../libs/file/config.h"
#include "../libs/file/scores.h"
#include "../libs/global_data.h"
#include "../libs/input/input.h"
#include "../libs/network/network.h"

void EntryScreen::on_screen_start() {
    Screen::on_screen_start();
    side = 1;
    is_2p = false;
    box_manager = std::make_unique<BoxManager>(global_data.entry_join_pending);
    if (global_data.config->network.auto_login) {
        state = EntryState::SELECT_SIDE;
    } else {
        state = EntryState::WAITING;
    }

    const bool online = global_data.config->network.online_play;
    pending_card_hex_.clear();
    used_cards_.clear();
    local_login_ = false;
    login_ready_ = false;
    name_pending_ = -1;
    name_entry_.reset();
    // Non-blocking: an unreachable server held the screen for the 1.5 s timeout. Logins wait for it.
    if (online) {
        network.start_probe();
    }

    // Card scanned on title (case 5) or auto login (case 3): join on first update
    if (global_data.card_reader_card_valid && !global_data.card_reader_card_id_hex.empty()) {
        pending_card_hex_ = global_data.card_reader_card_id_hex;
        used_cards_.push_back(pending_card_hex_);
    }
    global_data.card_reader_card_valid = false;
    global_data.card_reader_card_id_hex.clear();

#ifdef CARD_READER_ENABLED
    // Opened on a worker thread; update() starts polling once it is ready
    if (online && global_data.config->card_reader.enabled) {
        card_reader::reader_opener().request(global_data.config->card_reader.port,
                                             global_data.config->card_reader.baudrate);
    }
#endif

    // Preview only: local DB, no network until someone actually logs in
    {
        auto pd = scores_manager.get_player_data(global_data.config->network.access_code_1);
        nameplate = Nameplate(pd ? pd->username : "", pd ? pd->title : "", PlayerNum::ALL, pd ? pd->dan : -1,
                              pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
    }

    timer = std::make_unique<Timer>(60, get_current_ms(), [this] {
        if (box_manager->is_costume_box()) {
            box_manager->open_costume_menu(players[0] ? players[0]->player_num : PlayerNum::P1);
        } else {
            if (!box_manager->selection_allowed()) {
                box_manager->move_left();
            }
            box_manager->select_box();
        }
    });

    lua_entry = std::make_unique<EntryScript>();
    lua_entry->start_side_select();
    reload_preview_chara(PlayerNum::P1);
    wait_chara[0].reset();
    wait_chara[1].reset();
    announce_played = false;
    players.clear();
    players.resize(2);
    audio.play_sound("bgm", VolumePreset::MUSIC);

    // Online: auto_login shows side select right away, otherwise wait for Enter/Don
    if (online && !global_data.config->network.auto_login && !global_data.entry_join_pending) {
        state = EntryState::WAITING;
    }

    if (global_data.entry_join_pending) {
        global_data.entry_join_pending = false;
        start_second_player_join();
    }
}

// Resolve + load the account for the next free login slot (1st login -> access_code_1, 2nd -> access_code_2).
// card_hex empty: key/auto login. Returns false if login impossible. Offline the login finishes and
// then() runs before this returns; online the network part runs on a worker thread, update() applies
// it and then runs then(), and a card that cannot be registered never runs it.
bool EntryScreen::login_slot(const std::string& card_hex, std::function<void()> then) {
    if (login_pending()) {
        return false;
    }
    const int slot = players[0] ? 1 : 0;
    auto& cfg = global_data.config->network;
    std::string& code = slot == 0 ? cfg.access_code_1 : cfg.access_code_2;
    const PlayerData& sm_data = slot == 0 ? scores_manager.player_1_data : scores_manager.player_2_data;
    const bool net = cfg.online_play && network.is_online() && !local_login_;

    // Don after this slot was migrated to a server code: use a fresh local "0"/"1" account instead.
    // In-memory only (restored on title), so the migrated code stays in the config for Enter logins.
    if (cfg.online_play && local_login_ && code != "0" && code != "1") {
        if (!global_data.card_override[slot]) {
            global_data.card_prev_code[slot] = code;
        }
        global_data.card_override[slot] = true;
        code = slot == 0 ? "0" : "1";
    }

    if (!net) {
        // Case 1 / offline: scores.db keyed by config code. Cards can't resolve without server (case 4).
        if (!card_hex.empty()) {
            return false;
        }
        PlayerData pd = scores_manager.get_player_data(code).value_or(PlayerData{});
        pd.player_id = code;
        finish_slot(slot, pd);
        then();
        return true;
    }

    // Cases 2/3, never registered: get a code from server, replace the placeholder
    const bool placeholder = card_hex.empty() && (code.empty() || code == "0" || code == "1");
    const std::string name = sm_data.username.empty() ? "Player" : sm_data.username;
    pending_login_slot_ = slot;
    after_login_ = std::move(then);
    pending_login_ = std::async(std::launch::async, [card_hex, code = code, placeholder, name, sync = cfg.sync_scores] {
        RemoteLogin r;
        r.code = code;
        if (!card_hex.empty()) {
            std::string new_code = network.register_user("Player " + card_hex.substr(0, 8), card_hex);
            if (new_code.empty()) {
                return r;
            }
            r.code = new_code;
            r.card = true;
        } else if (placeholder) {
            std::string new_code = network.register_user(name);
            if (!new_code.empty()) {
                r.replaced_code = code;
                r.code = new_code;
            }
        }
        r.user = network.fetch_user(r.code);
        if (sync) {
            r.scores = network.fetch_scores(r.code);
        }
        r.ok = true;
        return r;
    });
    return true;
}

// Main-thread half of an online login: the config, scores.db and the slot's player data.
void EntryScreen::finish_login(RemoteLogin remote) {
    std::function<void()> then = std::move(after_login_);
    after_login_ = nullptr;
    if (!remote.ok) {
        spdlog::warn("Card login failed: the server did not register the card");
        return;
    }

    const int slot = pending_login_slot_;
    auto& cfg = global_data.config->network;
    std::string& code = slot == 0 ? cfg.access_code_1 : cfg.access_code_2;
    if (remote.card) {
        if (!global_data.card_override[slot]) {
            global_data.card_prev_code[slot] = code;
        }
        global_data.card_override[slot] = true;
        code = remote.code;
    } else if (!remote.replaced_code.empty()) {
        scores_manager.migrate_player_id(remote.replaced_code, remote.code);
        code = remote.code;
        save_config(*global_data.config);
    }

    // Local row (modifiers, dan, ...) overlaid with whatever the server knows.
    PlayerData pd = scores_manager.get_player_data(code).value_or(PlayerData{});
    pd.player_id = code;
    if (remote.user) {
        const RemoteUser& u = *remote.user;
        if (u.chara_colors) {
            pd.chara_color_1 = (*u.chara_colors)[0];
            pd.chara_color_2 = (*u.chara_colors)[1];
            pd.chara_color_3 = (*u.chara_colors)[2];
        }
        if (u.username && !u.username->empty()) {
            pd.username = *u.username;
        }
        if (u.title && !u.title->empty()) {
            pd.title = *u.title;
        }
        if (u.title_bg) {
            pd.title_bg = *u.title_bg;
        }
        if (u.costume) {
            pd.chara_head_index = u.costume->head_index;
            pd.chara_body_index = u.costume->body_index;
            pd.chara_cos_index = u.costume->cos_index;
            pd.chara_is_costume = u.costume->is_costume;
        }
        if (u.import_requested) {
            scores_manager.export_to_hiroba(code, code);
            network.clear_import_flag(code);
        }
    }
    if (pd.username.empty()) {
        pd.username = "Player";
    }
    if (cfg.sync_scores) {
        scores_manager.apply_remote_scores(remote.scores, code);
    }

    const bool fresh = remote.card || !remote.replaced_code.empty();
    const bool needs_name = fresh && pd.username.starts_with("Player") || pd.username.starts_with("Don-chan");

    finish_slot(slot, pd);
    if (needs_name) {
        if (players[slot]) {
            begin_name_entry(slot, std::move(then));
            return;
        }
        // Side not chosen yet (card scan): side select shows a blank nameplate, join_player asks
        name_pending_ = slot;
        if (slot == 0) {
            nameplate = Nameplate("", pd.title, PlayerNum::ALL, pd.dan, pd.gold, pd.rainbow, pd.title_bg);
        }
    }
    if (then) {
        then();
    }
}

void EntryScreen::begin_name_entry(int slot, std::function<void()> then) {
    const bool is_2p_seat = players[slot] && players[slot]->player_num == PlayerNum::P2;
    name_entry_ = std::make_unique<NameEntry>(is_2p_seat);
    if (name_entry_->unavailable()) { // skin without a name picker keeps the default name
        name_entry_.reset();
        if (then) {
            then();
        }
        return;
    }
    name_slot_ = slot;
    name_then_ = std::move(then);
    name_prev_state_ = state;
    state = EntryState::NAME_ENTRY;
    audio.stop_sound("bgm");
    audio.play_sound("name_select_bgm", VolumePreset::MUSIC);
    audio.play_sound("name_entry_vo", VolumePreset::VOICE);
}

void EntryScreen::end_name_entry() {
    const std::string name = name_entry_->get_name();
    name_entry_.reset();
    if (!name.empty()) {
        PlayerData pd = name_slot_ == 0 ? scores_manager.player_1_data : scores_manager.player_2_data;
        pd.username = name;
        finish_slot(name_slot_, pd);
        const std::string& code =
            name_slot_ == 0 ? global_data.config->network.access_code_1 : global_data.config->network.access_code_2;
        std::thread([code, name] { network.update_username(code, name); }).detach();
    }
    audio.stop_sound("name_select_bgm");
    audio.stop_sound("name_entry_vo");
    audio.play_sound("bgm", VolumePreset::MUSIC);
    if (players[name_slot_]) {
        players[name_slot_]->refresh_from_player_data();
    }
    state = name_prev_state_;
    auto then = std::move(name_then_);
    name_then_ = nullptr;
    if (then) {
        then();
    }
}

void EntryScreen::finish_slot(int slot, const PlayerData& pd) {
    (slot == 0 ? scores_manager.player_1 : scores_manager.player_2) = pd.player_id;
    (slot == 0 ? scores_manager.player_1_data : scores_manager.player_2_data) = pd;
    scores_manager.save_player_data(pd);
    if (slot == 0) {
        nameplate = Nameplate(pd.username, pd.title, PlayerNum::ALL, pd.dan, pd.gold, pd.rainbow, pd.title_bg);
    }
}

// Card scan: log in, then show side select. The player still picks the side.
void EntryScreen::join_with_card(const std::string& card_hex) {
    if (login_ready_) {
        return;
    }
    login_slot(card_hex, [this] { on_card_login(); });
}

void EntryScreen::on_card_login() {
    login_ready_ = true;
    audio.play_sound("don", VolumePreset::SOUND);
    if (players[0]) {
        // Second player: same screen as pressing Don for the free seat
        const PlayerNum free_seat = (players[0]->player_num == PlayerNum::P1) ? PlayerNum::P2 : PlayerNum::P1;
        const PlayerData& pd = scores_manager.player_2_data;
        nameplate = Nameplate(name_pending_ == 1 ? "" : pd.username, pd.title, PlayerNum::ALL, pd.dan, pd.gold,
                              pd.rainbow, pd.title_bg);
        reload_preview_chara(free_seat);
    }
    state = EntryState::SELECT_SIDE;
    side = 1;
    lua_entry->restart_side_select();
}

void EntryScreen::start_second_player_join() {
    audio.set_two_player_mode(true);
    const PlayerNum first = global_data.entry_joined_seat;
    const PlayerNum second = (first == PlayerNum::P1) ? PlayerNum::P2 : PlayerNum::P1;
    PlayerSoundScope audio_owner(second);

    side = (first == PlayerNum::P1) ? 0 : 2;
    global_data.player_num = first;
    players[0] = std::make_unique<EntryPlayer>(first, side, box_manager.get());
    players[0]->start_animations();

    const int second_side = (second == PlayerNum::P1) ? 0 : 2;
    global_data.player_num = second;
    players[1] = std::make_unique<EntryPlayer>(second, second_side, box_manager.get());
    players[1]->start_animations();
    audio.play_player_sound("cloud", PlayerNum::ALL, VolumePreset::SOUND);
    audio.play_sound("entry_start_" + std::to_string(static_cast<int>(second)) + "p", VolumePreset::VOICE);

    global_data.player_num = PlayerNum::P1;
    is_2p = true;
    side = 1;
    state = EntryState::SELECT_MODE;
    spdlog::info("[2P join] ENTRY resumed: {}P kept (first_login {}P), {}P entered, mode select",
                 static_cast<int>(first), static_cast<int>(global_data.first_login_player), static_cast<int>(second));
}

std::unique_ptr<Chara3D> EntryScreen::make_preview_chara(PlayerNum player_num) {
    std::string player_id = get_player_id(player_num);
    auto pd = scores_manager.get_player_data(player_id);
    auto c = make_chara_from_player_data(pd ? &*pd : nullptr);
    if (pd) {
        c->set_don_colors(pd->chara_color_1, pd->chara_color_2, pd->chara_color_3);
        c->apply_face(pd->chara_face_index);
    } else {
        c->set_don_colors(chara_default_color_1(player_num), chara_default_color_2(player_num), {249, 240, 225, 255});
    }
    return c;
}

void EntryScreen::reload_preview_chara(PlayerNum player_num) {
    chara = make_preview_chara(player_num);
}

Screens EntryScreen::on_screen_end(Screens next_screen) {
    audio.stop_sound("bgm");
    audio.stop_sound("name_select_bgm");
    // Leaving with a login in flight (input and the timer wait for it, so this is an edge case):
    // drop it instead of waiting on the network; the worker finishes on its own
    if (login_pending()) {
        static std::vector<std::future<RemoteLogin>> abandoned;
        abandoned.push_back(std::move(pending_login_));
        after_login_ = nullptr;
    }

    // Clean up card reader
#ifdef CARD_READER_ENABLED
    if (card_reader_) {
        card_reader_->stop_polling();
        card_reader_->led_reset();
        card_reader_.reset();
        card_reader_initialized_ = false;
    }
#endif

    return Screen::on_screen_end(next_screen);
}

bool EntryScreen::arcade_credit() const {
    return tex.options[SCO::ENTRY_CREDIT_ARCADE];
}

bool EntryScreen::seat_joined(PlayerNum player_num) const {
    for (const auto& player : players) {
        if (player && player->player_num == player_num) {
            return true;
        }
    }
    return false;
}

void EntryScreen::join_player(PlayerNum player_num, bool do_login) {
    PlayerSoundScope audio_owner(player_num);
    bool loading = false;
    if (do_login && !login_ready_) {
        if (login_pending()) {
            return;
        }
        login_slot("", [this, player_num] {
            for (auto& player : players) {
                if (player && player->player_num == player_num) {
                    player->refresh_from_player_data();
                    player->finish_loading();
                }
            }
        });
        loading = login_pending();
    }
    login_ready_ = false;
    side = (player_num == PlayerNum::P1) ? 0 : 2;
    global_data.player_num = player_num;

    if (players[0]) {
        audio.set_two_player_mode(true);
        players[1] = std::make_unique<EntryPlayer>(global_data.player_num, side, box_manager.get());
        players[1]->start_animations(loading && !network_msg.available);
        global_data.player_num = PlayerNum::P1;
        is_2p = true;
    } else {
        global_data.first_login_player = global_data.player_num;
        players[0] = std::make_unique<EntryPlayer>(global_data.player_num, side, box_manager.get());
        players[0]->start_animations(loading && !network_msg.available);
        is_2p = false;
        audio.set_two_player_mode(false);
    }
    audio.play_player_sound("cloud", PlayerNum::ALL, VolumePreset::SOUND);
    audio.play_sound("entry_start_" + std::to_string(static_cast<int>(player_num)) + "p", VolumePreset::VOICE);
    if (state == EntryState::SELECT_SIDE) {
        lua_entry->decide_side_select(side);
    }
    state = EntryState::SELECT_MODE;
    audio.play_sound("don", VolumePreset::SOUND);
    if (name_pending_ >= 0 && players[name_pending_]) {
        const int slot = name_pending_;
        name_pending_ = -1;
        begin_name_entry(slot, nullptr);
    }
}

std::optional<Screens> EntryScreen::handle_input() {
    if (login_pending() || network.probing()) {
        return std::nullopt;
    }
    if (state == EntryState::NAME_ENTRY) {
        if (menu_left_pressed()) {
            audio.play_sound("kat", VolumePreset::SOUND);
            name_entry_->input(NameEntry::LEFT);
        } else if (menu_right_pressed()) {
            audio.play_sound("kat", VolumePreset::SOUND);
            name_entry_->input(NameEntry::RIGHT);
        } else if (menu_confirm_pressed()) {
            audio.play_sound("don", VolumePreset::SOUND);
            name_entry_->input(NameEntry::DECIDE);
        }
        if (name_entry_->is_done()) {
            end_name_entry();
        }
        return std::nullopt;
    }
    if (state == EntryState::WAITING) {
        const bool enter = key_pressed(SDL_SCANCODE_RETURN);
        const bool don = is_l_don_pressed() || is_r_don_pressed();
        if (!enter && !don) {
            return std::nullopt;
        }
        local_login_ = !enter; // Enter: register/fetch online; Don: local db only
        login_slot("", [this] {
            login_ready_ = true;
            audio.play_sound("don", VolumePreset::SOUND);
            state = EntryState::SELECT_SIDE;
            lua_entry->restart_side_select();
        });
        return std::nullopt;
    }
    // Enter again with one player in: online login (registers placeholder "1") for the free seat
    if (global_data.config->network.online_play && key_pressed(SDL_SCANCODE_RETURN) &&
        (state == EntryState::SELECT_SIDE || state == EntryState::SELECT_MODE) &&
        seat_joined(PlayerNum::P1) != seat_joined(PlayerNum::P2)) {
        local_login_ = false;
        join_player(seat_joined(PlayerNum::P1) ? PlayerNum::P2 : PlayerNum::P1);
        return std::nullopt;
    }
    if (arcade_credit() && state == EntryState::SELECT_SIDE) {
        if (!seat_joined(PlayerNum::P1) && (is_l_don_pressed(PlayerNum::P1) || is_r_don_pressed(PlayerNum::P1))) {
            join_player(PlayerNum::P1);
        } else if (!seat_joined(PlayerNum::P2) &&
                   (is_l_don_pressed(PlayerNum::P2) || is_r_don_pressed(PlayerNum::P2))) {
            join_player(PlayerNum::P2);
        }
        return std::nullopt;
    }
    if (state == EntryState::SELECT_SIDE) {
        if (menu_confirm_pressed()) {
            if (side == 1) {
                if (players[0]) {
                    audio.play_sound("don", VolumePreset::SOUND);
                    state = EntryState::SELECT_MODE;
                    return std::nullopt;
                }
                return on_screen_end(Screens::TITLE);
            }
            join_player((side == 0) ? PlayerNum::P1 : PlayerNum::P2);
        }
        if (menu_left_pressed()) {
            audio.play_sound("kat", VolumePreset::SOUND);
            if (players[0] && players[0]->player_num == PlayerNum::P1) {
                side = 1;
            } else if (players[0] && players[0]->player_num == PlayerNum::P2) {
                side = 0;
            } else {
                side = std::max(0, side - 1);
            }
        }
        if (menu_right_pressed()) {
            audio.play_sound("kat", VolumePreset::SOUND);
            if (players[0] && players[0]->player_num == PlayerNum::P1) {
                side = 2;
            } else if (players[0] && players[0]->player_num == PlayerNum::P2) {
                side = 1;
            } else {
                side = std::min(2, side + 1);
            }
        }
    } else if (state == EntryState::SELECT_COSTUME) {
        for (auto& player : players) {
            if (player) {
                player->handle_input();
            }
        }
    } else if (state == EntryState::SELECT_MODE) {
        if (arcade_credit()) {
            if (!seat_joined(PlayerNum::P1) && (is_l_don_pressed(PlayerNum::P1) || is_r_don_pressed(PlayerNum::P1))) {
                join_player(PlayerNum::P1);
                return std::nullopt;
            }
            if (!seat_joined(PlayerNum::P2) && (is_l_don_pressed(PlayerNum::P2) || is_r_don_pressed(PlayerNum::P2))) {
                join_player(PlayerNum::P2);
                return std::nullopt;
            }
            if (!mode_select_ready()) {
                return std::nullopt;
            }
            for (auto& player : players) {
                if (player) {
                    player->handle_input();
                }
            }
            return std::nullopt;
        }
        if (!mode_select_ready()) {
            return std::nullopt;
        }
        for (auto& player : players) {
            if (player) {
                player->handle_input();
            }
        }
        if (players[0] && players[0]->player_num == PlayerNum::P1 &&
            (is_l_don_pressed(PlayerNum::P2) || is_r_don_pressed(PlayerNum::P2))) {
            audio.play_sound("don", VolumePreset::SOUND);
            state = EntryState::SELECT_SIDE;
            {
                auto pd = scores_manager.get_player_data(global_data.config->network.access_code_2);
                nameplate = Nameplate(pd ? pd->username : "", pd ? pd->title : "", PlayerNum::ALL, pd ? pd->dan : -1,
                                      pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
            }
            lua_entry->restart_side_select();
            side = 1;
            reload_preview_chara(PlayerNum::P2);
        } else if (players[0] && players[0]->player_num == PlayerNum::P2 &&
                   (is_l_don_pressed(PlayerNum::P1) || is_r_don_pressed(PlayerNum::P1))) {
            audio.play_sound("don", VolumePreset::SOUND);
            state = EntryState::SELECT_SIDE;
            {
                auto pd = scores_manager.get_player_data(global_data.config->network.access_code_1);
                nameplate = Nameplate(pd ? pd->username : "", pd ? pd->title : "", PlayerNum::ALL, pd ? pd->dan : -1,
                                      pd ? pd->gold : false, pd ? pd->rainbow : false, pd ? pd->title_bg : 0);
            }
            lua_entry->restart_side_select();
            side = 1;
            reload_preview_chara(PlayerNum::P1);
        }
    }
    return std::nullopt;
}

std::optional<Screens> EntryScreen::update() {
    Screen::update();
    double current_time = get_current_ms();

    if (login_pending() && pending_login_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        finish_login(pending_login_.get());
    }

    if (!players.empty() && !pending_card_hex_.empty() && !login_pending() && !network.probing()) {
        std::string hex = std::move(pending_card_hex_);
        pending_card_hex_.clear();
        join_with_card(hex);
    }

#ifdef CARD_READER_ENABLED
    if (!card_reader_) {
        if (auto opened = card_reader::reader_opener().take()) {
            card_reader_ = std::move(*opened);
            if (card_reader_) {
                card_reader_->start_polling();
                card_reader_->set_led_color(255, 255, 255);
                last_card_poll_ms_ = current_time;
                card_reader_initialized_ = true;
            }
        }
    }
    // Case 6: any card while a seat is free (P1 first, then P2)
    if (card_reader_ && card_reader_->is_polling() && !login_ready_ && !login_pending() && !network.probing() &&
        state != EntryState::SELECT_COSTUME && state != EntryState::NAME_ENTRY &&
        !(seat_joined(PlayerNum::P1) && seat_joined(PlayerNum::P2))) {
        if (current_time - last_card_poll_ms_ >= global_data.config->card_reader.poll_interval_ms) {
            last_card_poll_ms_ = current_time;
            if (card_reader_->poll_once()) {
                const auto& ci = card_reader_->get_card_info();
                if (ci.valid && !ci.card_id_hex.empty() && !std::ranges::contains(used_cards_, ci.card_id_hex)) {
                    used_cards_.push_back(ci.card_id_hex);
                    card_reader_->set_led_color(0, 255, 0);
                    join_with_card(ci.card_id_hex);
                }
            }
        }
    }
#endif

    allnet_indicator.update(current_time);
    network_msg.update(current_time);
    if (login_pending() || network.probing()) {
        network_msg.show("clear_reading");
    } else {
        network_msg.hide();
    }
    entry_overlay.update(current_time);
    lua_entry->update(current_time);
    if (name_entry_) {
        name_entry_->update(current_time);
    }
    box_manager->update(current_time, is_2p);
    if (state != EntryState::WAITING && state != EntryState::NAME_ENTRY &&
        !(arcade_credit() && state == EntryState::SELECT_SIDE) && !login_pending()) {
        timer->update(current_time);
    }
    nameplate.update(current_time);
    chara->update(current_time);
    if (state == EntryState::WAITING) {
        for (auto& c : wait_chara) {
            if (c) {
                c->update(current_time);
            }
        }
    }
    for (auto& player : players) {
        if (player) {
            player->update(current_time);
        }
    }
    if (box_manager->costume_menu_open) {
        box_manager->costume_menu_open = false;
        state = EntryState::SELECT_COSTUME;
        for (auto& player : players) {
            if (player && player->player_num == box_manager->opening_player) {
                player->open_costume_menu();
            }
        }
    }
    if (state == EntryState::SELECT_COSTUME) {
        bool any_open = false;
        for (auto& player : players) {
            if (player && player->costume_menu.has_value()) {
                any_open = true;
                break;
            }
        }
        if (!any_open) {
            state = EntryState::SELECT_MODE;
        }
    }
    if (box_manager->is_finished()) {
        return on_screen_end(box_manager->selected_box());
    }
    for (auto& player : players) {
        if (player && player->is_cloud_animation_finished() &&
            !audio.is_sound_playing("entry_start_" + std::to_string(static_cast<int>(global_data.player_num)) + "p") &&
            !announce_played && state != EntryState::NAME_ENTRY) {
            audio.play_sound("select_mode", VolumePreset::VOICE);
            announce_played = true;
        }
    }
    return handle_input();
}

void EntryScreen::draw_background() {
    lua_entry->draw_background();
}

void EntryScreen::draw_side_select(float fade) {
    lua_entry->draw_side_select();

    const bool show_preview = !arcade_credit() || seat_joined(PlayerNum::P1) || seat_joined(PlayerNum::P2);
    if (show_preview) {
        chara->draw(tex.chara_xy("entry").first, tex.chara_xy("entry").second);
    }
    lua_entry->draw_side_select_buttons(side);
    if (show_preview) {
        auto [nx, ny] = tex.nameplate_xy("entry");
        nameplate.draw(nx, ny, fade);
    }
}

void EntryScreen::draw_player_drum() {
    for (auto& player : players) {
        if (player) {
            player->draw_drum();
        }
    }
}

bool EntryScreen::mode_select_ready() {
    if (network_msg.available && login_pending()) {
        return false;
    }
    for (auto& player : players) {
        if (player && !player->is_cloud_animation_finished()) {
            return false;
        }
    }
    return true;
}

void EntryScreen::draw_mode_select() {
    if (!mode_select_ready()) {
        return;
    }
    box_manager->draw();
}

void EntryScreen::draw() {
    draw_background();
    draw_player_drum();

    if (state == EntryState::WAITING) {
        lua_entry->draw_waiting();
        for (int i = 0; i < 2; i++) {
            auto pos = lua_entry->wait_chara_pos(i == 1);
            if (!pos) {
                continue;
            }
            if (!wait_chara[i]) {
                wait_chara[i] = make_preview_chara(i == 0 ? PlayerNum::P1 : PlayerNum::P2);
            }
            wait_chara[i]->draw(pos->first, pos->second);
        }
    } else if (state == EntryState::SELECT_SIDE) {
        draw_side_select(lua_entry->get_side_select_fade());
    } else if (state == EntryState::SELECT_MODE) {
        draw_mode_select();
    } else if (state == EntryState::SELECT_COSTUME) {
        for (auto& player : players) {
            if (player) {
                player->draw_costume_menu();
            }
        }
    }

    bool p1_joined = (players[0] && players[0]->player_num == PlayerNum::P1) ||
                     (players[1] && players[1]->player_num == PlayerNum::P1);
    bool p2_joined = (players[0] && players[0]->player_num == PlayerNum::P2) ||
                     (players[1] && players[1]->player_num == PlayerNum::P2);
    lua_entry->draw_footer(p1_joined, p2_joined);

    for (auto& player : players) {
        if (player) {
            player->draw_nameplate_and_indicator(player->get_nameplate_fadein());
        }
    }

    if (!name_entry_) {
        lua_entry->draw_player_entry(); // the name picker has its own title in its place
    }
    if (name_entry_) {
        name_entry_->draw();
    }

    if (box_manager->is_finished()) {
        draw_rectangle(0, 0, tex.screen_width, tex.screen_height, Color::BLACK);
    }

    network_msg.draw();
    timer->draw();
    entry_overlay.draw(false);
    coin_overlay.draw();
    allnet_indicator.draw();
}
