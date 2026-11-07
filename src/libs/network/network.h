#pragma once

#include "../file/scores.h"

#if defined(NETWORK_URL) && defined(NETWORK_AUTH_KEY)
#define NETWORK_ENABLED 1
#ifdef _WIN32
#define CloseWindow CloseWindow_WinAPI
#define ShowCursor ShowCursor_WinAPI
#endif
#include <cpr/cpr.h>
#ifdef _WIN32
#undef CloseWindow
#undef ShowCursor
#undef GetObject // renames rapidjson's GetObject() to GetObjectA in every file including this
#endif
#endif

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "../gfx/color_utils.h"

enum class InputLogType {
    KAT_L = 0,
    DON_L = 1,
    DON_R = 2,
    KAT_R = 3
};

struct RemoteScore {
    std::string hash;
    int difficulty;
    Score score;
};

struct RankingEntry {
    std::string username;
    int score = 0;
};

struct RemoteUser {
    std::optional<std::string> username;
    std::optional<std::string> title;
    std::optional<int> title_bg;
    std::optional<std::array<Color, 3>> chara_colors;
    struct Costume {
        int head_index, body_index, cos_index;
        bool is_costume;
    };
    std::optional<Costume> costume;
    std::optional<int> acce_index;
    bool import_requested = false;
};

struct ReplayData {
    bool ok = false;
    std::string hash;
    int difficulty = 0;
    std::map<double, int> input_log;
    PlayerData player_data;
};

struct GachaResult {
    int puchichara = -1;
    std::string error;
};

struct KinotakeSeat {
    int team = -1;
    int plays = 0;
    int64_t base = 0;  // scores
    int64_t total = 0; // with bonuses
    int64_t mine = 0;  // the player's all-time points (after the last play)
    int assist = 1;    // highest costume rate seen
    bool duet = false;
};
// Kinotake costumes (cos index): wearing one doubles the play's points
constexpr int KINOTAKE_COSTUME_KINOKO = 105;
constexpr int KINOTAKE_COSTUME_TAKENOKO = 106;

struct KinotakeTally {
    KinotakeSeat seat[2];
    int64_t kinoko = 0, takenoko = 0; // today's team totals after the last answered play
    bool any() const { return seat[0].plays > 0 || seat[1].plays > 0; }
};

struct DonPresent {
    int type = -1; // 0 music, 1 tone, 2 dress, 3 head, 4 body
    int id = -1;
    int next_aim = -1; // -1 = the track has ended
};
struct DonTitle {
    std::string name;
    int points = 0;
};
struct DonSeat {
    bool answered = false;
    std::vector<int> songs; // points per song, in play order
    std::vector<DonTitle> titles;
    int current = 0;
    int aim = -1;
    std::vector<DonPresent> presents;
};

struct MedalSeat {
    bool answered = false;
    int now = 0; // balance before the first claim since the last reset
    int add = 0; // every claim since then
    int max = 0;
    bool daily = false, weekly = false, monthly = false;
};
struct AIBattleBest {
    int good = 0;
    int ok = 0;
    int bad = 0;
    int drumroll = 0;
};
struct AIBattleProgress {
    bool answered = false;
    int rank_before = 0;
    int rank_after = 0;
    int tokens_before = 0;
    int tokens_after = 0;
    int next_rank_tokens = -1;       // -1 once the rank can't rise
    int next_rank_after_tokens = -1; // the same for rank_after
    int wins_before = 0;
    int wins_after = 0;
    bool fulfilled = false;
    bool first = false;
    bool challenge = false;
    int next_reward_wins = -1; // -1 once every reward is earned
    std::string next_reward_name;
    std::string reward_kind; // "song", "head", "body", "title"; empty = this win gave no reward
    std::string reward_name;
    bool all_rewards = false;
    std::vector<AIBattleBest> bests;
};
struct AIBattleStatus {
    bool answered = false;
    int rank = 0;
    int tokens = 0;
    int next_rank_tokens = -1;
    int wins = 0;
    int next_reward_wins = -1;
};
struct ShopGood {
    int type = -1; // 2 dress, 3 head, 4 body, 5 puchichara, 6 title
    int id = -1;
    int cost = 0;
    bool owned = false;
    std::string name; // titles only: the skin has no names for them
};
struct ShopSeat {
    bool answered = false;
    int medals = 0;
    int max = 0;
    std::vector<ShopGood> goods;
};
struct ShopBuyResult {
    int seat = 0;
    std::string status; // ok, already, shortage, lineup or error
    int medals = 0;
};

struct OwnedCosmetics {
    bool all = true;
    std::set<int> costume;
    std::set<int> head;
    std::set<int> body;
};

std::string modifiers_to_json(const Modifiers& modifiers);

class NetworkClient {
public:
    void update(double current_ms);

    bool is_online() const { return online; }
    bool is_outdated() const { return outdated; }
    bool is_tournament() const { return online && tournament; }
    bool tournament_offline() const { return !online && tournament; }

    std::vector<std::string> active_campaigns() const;

    std::string register_user(const std::string& username, const std::string& idm = "");
    std::string map_to_json(const std::map<double, InputLogType>& my_map);
    void submit_score(const std::string& hash, int difficulty, const std::string& access_code, const Score& score,
                      const std::map<double, InputLogType>& input_log, int64_t played_at,
                      const std::string& modifiers_json, bool chara_is_costume, int chara_cos_index,
                      const std::string& source_folder = "", int gauge = -1);

    bool check_import_requested(const std::string& access_code);
    void clear_import_flag(const std::string& access_code);

    bool fetch_chara_colors(const std::string& access_code, Color& color_1, Color& color_2, Color& color_3);

    bool fetch_username(const std::string& access_code, std::string& username);
    void update_username(const std::string& access_code, const std::string& username);

    bool fetch_title(const std::string& access_code, std::string& title);
    bool fetch_title_bg(const std::string& access_code, int& title_bg);

    bool fetch_costume(const std::string& access_code, int& head_index, int& body_index, int& cos_index,
                       bool& is_costume);

    // One GET /user for the whole profile. Blocking; safe to call from a worker thread
    std::optional<RemoteUser> fetch_user(const std::string& access_code);

    void check_and_install_android_update();

    bool probe_online();
    // Non-blocking probe_online: sends a heartbeat now; is_online() updates when it lands
    void start_probe();
    bool probing() const { return probing_; }
    void update_costume(const std::string& access_code, int head_index, int body_index, int cos_index, bool is_costume);

    void poll_song_jump(const std::string& access_code);
    std::optional<std::string> take_song_jump_result();

    void poll_replay_jump(const std::string& access_code);
    std::optional<int> take_replay_jump_result();

    std::vector<RemoteScore> fetch_scores(const std::string& access_code);
    void request_replay(int score_id);
    std::optional<ReplayData> take_replay_result();

    std::optional<std::vector<RankingEntry>> song_leaderboard(const std::string& hash);

    void spin_gacha(const std::string& access_code);
    std::optional<GachaResult> take_gacha_result();

    void submit_kinotake(int seat, const std::string& access_code, int team, int score, bool duet, bool costume);
    bool kinotake_pending() const { return kinotake_pending_count_ > 0; }
    const KinotakeTally& kinotake_tally() const { return kinotake_tally_; }
    void reset_kinotake_tally() { kinotake_tally_ = {}; }
    void submit_don_points(int seat, const std::string& access_code, const std::vector<int>& gauges);
    bool don_points_pending() const {
#if defined(NETWORK_ENABLED)
        return !pending_don_points.empty();
#else
        return false;
#endif
    }
    const DonSeat& don_seat(int seat) const { return don_seats[seat]; }
    void reset_don_points() { don_seats[0] = {}, don_seats[1] = {}; }
    void submit_medals(int seat, const std::string& access_code);
    void submit_revive_result(const std::string& access_code, bool success);
    const MedalSeat& medal_seat(int seat) const { return medal_seats[seat]; }
    void reset_medals() { medal_seats[0] = {}, medal_seats[1] = {}; }
    void request_reward_shop(int seat, const std::string& access_code);
    const ShopSeat& shop_seat(int seat) const { return shop_seats[seat]; }
    void buy_shop_good(int seat, const std::string& access_code, int type, int id);
    std::optional<ShopBuyResult> take_shop_buy_result();
    // Medal claims and lineup fetches still in flight
    bool reward_shop_pending() const { return reward_shop_pending_count_ > 0; }
    OwnedCosmetics fetch_owned_cosmetics(const std::string& access_code);
    void unlock_campaign_costume(const std::string& access_code, int cos_index);
    int kinotake_leader() const { return kinotake_leader_; }

    void submit_ai_battle(const std::string& access_code, const std::string& song_hash, int difficulty, int stars,
                          bool won, bool assigned, const std::string& sections);
    AIBattleStatus fetch_ai_battle_status(const std::string& access_code);
    bool ai_battle_pending() const {
#if defined(NETWORK_ENABLED)
        return !pending_ai_battle.empty();
#else
        return false;
#endif
    }
    const AIBattleProgress& ai_battle_progress() const { return ai_battle_progress_; }
    void reset_ai_battle() { ai_battle_progress_ = {}; }

    void shutdown();

private:
    void check_heartbeat();

    bool online = false;
    bool outdated = false;
    bool tournament = false;
    std::vector<std::string> campaigns;
    std::optional<GachaResult> gacha_result;
    bool probing_ = false;
    KinotakeTally kinotake_tally_;
    DonSeat don_seats[2];
    MedalSeat medal_seats[2];
    AIBattleProgress ai_battle_progress_;
    ShopSeat shop_seats[2];
    std::optional<ShopBuyResult> shop_buy_result;
    int reward_shop_pending_count_ = 0;
    int kinotake_pending_count_ = 0;
    int kinotake_leader_ = -1;
#if defined(NETWORK_ENABLED)
    std::optional<cpr::AsyncResponse> pending_heartbeat;
    static constexpr double HEARTBEAT_INTERVAL_MS = 30000.0;
    double last_heartbeat_ms = -HEARTBEAT_INTERVAL_MS;

    std::optional<cpr::AsyncResponse> pending_song_jump;
    std::optional<std::string> song_jump_result;

    std::optional<cpr::AsyncResponse> pending_replay_jump;
    std::optional<int> replay_jump_result;

    std::optional<cpr::AsyncResponse> pending_replay_fetch;
    std::optional<ReplayData> replay_fetch_result;

    std::optional<cpr::AsyncResponse> pending_score_submit;

    std::optional<cpr::AsyncResponse> pending_gacha;

    std::vector<std::pair<int, cpr::AsyncResponse>> pending_kinotake;
    std::vector<std::pair<int, cpr::AsyncResponse>> pending_don_points;
    std::vector<std::pair<int, cpr::AsyncResponse>> pending_medals;
    std::vector<cpr::AsyncResponse> pending_ai_battle;
    std::vector<cpr::AsyncResponse> pending_revive;
    std::vector<std::pair<int, cpr::AsyncResponse>> pending_shop;
    std::vector<std::pair<int, cpr::AsyncResponse>> pending_shop_buy;

    std::optional<cpr::AsyncResponse> pending_leaderboard;
    std::string pending_leaderboard_hash;
    std::string next_leaderboard_hash;
    std::map<std::string, std::vector<RankingEntry>> leaderboard_cache;
    void start_leaderboard_fetch(const std::string& hash);

#if defined(__ANDROID__)
    std::optional<cpr::AsyncResponse> pending_update_checksum;
    std::optional<cpr::AsyncResponse> pending_update_apk;
    std::string pending_update_expected_sha256;
    bool android_update_checked = false;
#endif
#endif
};

extern NetworkClient network;
