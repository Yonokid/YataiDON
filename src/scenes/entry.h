#pragma once

#include "../libs/file/scores.h"
#include "../libs/network/network.h"
#include "../libs/screen.h"
#include "../objects/entry/access_code_popup.h"
#include "../objects/entry/bncoin_popup.h"
#include "../objects/entry/box_manager.h"
#include "../objects/entry/campaign_menu.h"
#include "../objects/entry/entry_info.h"
#include "../objects/entry/entry_script.h"
#include "../objects/entry/name_entry.h"
#include "../objects/entry/player.h"
#include "../objects/entry/practice_help.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/chara_3d.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/entry_overlay.h"
#include "../objects/global/kinotake_indicator.h"
#include "../objects/global/nameplate.h"
#include "../objects/global/network_msg.h"
#include "../objects/global/timer.h"

#ifdef CARD_READER_ENABLED
#include "../libs/optional/card_reader.h"
#endif

#include <functional>
#include <future>

enum class EntryState {
    SELECT_SIDE = 0,
    SELECT_MODE = 1,
    SELECT_COSTUME = 2,
    WAITING = 3,
    NAME_ENTRY = 4,
    CAMPAIGN = 5,
    BNCOIN = 6,
    PRACTICE_HELP = 7,
    ENTRY_INFO = 8,
    ACCESS_CODE = 9
};

struct RemoteLogin {
    bool ok = false;           // false: a card could not be registered
    std::string code;          // access code to log in with
    std::string replaced_code; // set when a placeholder code was registered and replaced by `code`
    bool card = false;         // `code` was registered for a scanned card
    std::optional<RemoteUser> user;
    std::vector<RemoteScore> scores;
};

class EntryScreen : public Screen {
private:
    int side = 1;
    bool is_2p = false;
    std::unique_ptr<BoxManager> box_manager;
    EntryState state = EntryState::SELECT_SIDE;

    std::unique_ptr<EntryScript> lua_entry;
    Nameplate nameplate;
    CoinOverlay coin_overlay;
    AllNetIcon allnet_indicator;
    KinotakeIndicator kinotake_indicator;
    bool kinotake_on = false;
    NetworkMsg network_msg; // login / probe wait banner
    EntryOverlay entry_overlay;
    std::unique_ptr<Timer> timer;
    static constexpr int WAIT_SECONDS = 60; // nobody joined: back to title
    static constexpr int SIDE_SECONDS = 15;
    static constexpr int MODE_SECONDS = 60;
    bool timer_fired_ = false;
    void start_timer(int seconds);
    std::optional<Screens> on_timeout();

    std::unique_ptr<Chara3D> chara;
    std::unique_ptr<Chara3D> wait_chara[2]; // wait panels, P1 / P2
    bool announce_played = false;
    double wait_idle_since_ = 0; // WAITING: time of the last input, for the AFK popup
    std::vector<std::unique_ptr<EntryPlayer>> players;

#ifdef CARD_READER_ENABLED
    std::unique_ptr<card_reader::Reader> card_reader_;
    double next_card_start_ms_ = 0;
#endif
    std::vector<std::string> used_cards_; // held card must not log in twice
    std::string title_card_hex_;          // card that came from the title screen
    std::string pending_card_hex_;        // scanned on title, consumed on first update
    bool local_login_ = false;            // Don pressed: scores.db only, never register
    bool login_ready_ = false;            // next join_player already logged in (Enter / auto login)

    std::unique_ptr<Chara3D> make_preview_chara(PlayerNum player_num);
    void reload_preview_chara(PlayerNum player_num);
    bool join_by_drum();
    bool seat_joined(PlayerNum player_num) const;
    void open_reward_shop();
    void join_player(PlayerNum player_num, bool do_login = true);
    void start_second_player_join();
    void draw_background();
    void draw_side_select(float fade);
    void draw_player_drum();
    void draw_mode_select();
    bool mode_select_ready();
    std::optional<Screens> handle_input();

    std::future<RemoteLogin> pending_login_;
    int pending_login_slot_ = 0;
    std::function<void()> after_login_;

    bool login_slot(const std::string& card_hex, std::function<void()> then);
    void finish_login(RemoteLogin remote);
    void finish_slot(int slot, const PlayerData& pd);
    bool login_pending() const { return pending_login_.valid(); }
    void join_with_card(const std::string& card_hex);

    std::unique_ptr<NameEntry> name_entry_;
    EntryState name_prev_state_ = EntryState::SELECT_SIDE;
    int name_slot_ = 0;
    int name_pending_ = -1; // slot of a new account whose name is asked once its side is chosen
    std::function<void()> name_then_;
    void begin_name_entry(int slot, std::function<void()> then);
    void end_name_entry();
    void on_card_login();

    std::unique_ptr<CampaignMenu> campaign_menu_;
    std::unique_ptr<BanaCoinPopup> bncoin_popup_;
    static constexpr int BNCOIN_PRACTICE_COST = 100;
    void open_bncoin_popup();
    std::unique_ptr<AccessCodePopup> access_code_popup_;
    void open_access_code_popup();
    std::unique_ptr<PracticeHelp> practice_help_;
    void start_practice();
    std::unique_ptr<EntryInfo> entry_info_;
    void enter_reward_shop();
    bool campaign_shown_ = false;
    bool update_campaign(double current_ms);
    void resume_from_campaign();
    void leave_for_campaign();

public:
    EntryScreen() : Screen("entry") {}
    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
