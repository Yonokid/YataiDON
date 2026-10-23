#pragma once

#include "scores.h"

#if defined(NETWORK_URL)
#define NETWORK_ENABLED 1
#ifdef _WIN32
#define CloseWindow CloseWindow_WinAPI
#define ShowCursor ShowCursor_WinAPI
#endif
#include <cpr/cpr.h>
#ifdef _WIN32
#undef CloseWindow
#undef ShowCursor
#endif
#endif

#include <array>
#include <atomic>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "color_utils.h"

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

// Everything GET /user returns about a player; each field is set only if the server sent it
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
    bool import_requested = false;
};

struct ReplayData {
    bool ok = false;
    std::string hash;
    int difficulty = 0;
    std::map<double, int> input_log;
    PlayerData player_data;
};

std::string modifiers_to_json(const Modifiers& modifiers);

class NetworkClient {
public:
    void update(double current_ms);

    bool is_online() const { return online; }
    bool is_outdated() const { return outdated; }

    std::string register_user(const std::string& username, const std::string& idm = "");
    std::string map_to_json(const std::map<double, InputLogType>& my_map);
    void submit_score(const std::string& hash, int difficulty, const std::string& access_code, const Score& score,
                      const std::map<double, InputLogType>& input_log, int64_t played_at,
                      const std::string& modifiers_json, bool chara_is_costume, int chara_cos_index);

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

    void shutdown();

private:
    void check_heartbeat();

    bool online = false;
    bool outdated = false;
    bool probing_ = false;
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

#if defined(__ANDROID__)
    std::optional<cpr::AsyncResponse> pending_update_checksum;
    std::optional<cpr::AsyncResponse> pending_update_apk;
    std::string pending_update_expected_sha256;
    bool android_update_checked = false;
#endif
#endif
};

extern NetworkClient network;
