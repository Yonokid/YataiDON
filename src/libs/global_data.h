#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "file/config.h"
#include "gfx.h" // IWYU pragma: keep
#include "gfx/color_utils.h"
#include "session_types.h" // IWYU pragma: export

namespace fs = std::filesystem;

namespace ScoreMethod {
    const std::string GEN3 = "gen3";
    const std::string SHINUCHI = "shinuchi";
} // namespace ScoreMethod

enum class PlayerNum {
    ALL = 0,
    P1 = 1,
    P2 = 2,
    TWO_PLAYER = 3,
    DAN = 4,
    AI = 5
};

enum class Difficulty {
    BACK = -3,
    MODIFIER = -2,
    NEIRO = -1,
    EASY = 0,
    NORMAL = 1,
    HARD = 2,
    ONI = 3,
    URA = 4,
    TOWER = 5,
    DAN = 6
};

enum class Crown {
    NONE = 0,
    CLEAR = 1,
    FC = 2,
    DFC = 3
};

// underscore to avoid conflict with raylib colors
enum class Rank {
    _NONE = 0,
    _WHITE = 1,
    _BRONZE = 2,
    _SILVER = 3,
    _GOLD = 4,
    _PINK = 5,
    _PURPLE = 6,
    _RAINBOW = 7
};

struct ReplayData;

struct SessionData {
    fs::path selected_song;
    std::string selected_collection; // the song select folder it was picked from, sent with the score
    std::string song_hash;
    int selected_difficulty = 0;
    int subdiff = 0;
    std::string song_title = "default_title";
    std::string song_subtitle = "default_subtitle";
    bool song_subtitle_full_display = false;
    int genre_index = 0;
    std::string genre_label;
    fs::path selected_dan_folder;
    fs::path selected_tutorial;
    DanCourse dan;
    bool ai_assigned = false;
    std::shared_ptr<const ReplayData> replay; // set when song select starts a downloaded replay
    ResultData result_data;
    DanResultData dan_result_data;
};

struct CameraConfig {
    Vec2 offset = {0.0f, 0.0f};
    float zoom = 1.0f;
    float h_scale = 1.0f;
    float v_scale = 1.0f;
    float rotation = 0.0f;
    Color border_color = Color::BLACK;
};

class PracticeTimer;

struct CardSession {
    std::string reader_hex; // 16-char hex card ID
    bool reader_valid = false;
    std::string prev_code[2];
    bool overridden[2] = {false, false};
};

struct EntryResume {
    bool join_pending = false;
    PlayerNum joined_seat = PlayerNum::P1;
    bool campaign_resume = false;
    PlayerNum resume_seats[2] = {PlayerNum::ALL, PlayerNum::ALL};
};

struct CampaignSession {
    std::string name;
    int next = 0;
    int kinotake_team[2] = {-1, -1};
    int kinotake_next = 0;
};

struct CreditSession {
    int songs_played = 0;
    int songs[2] = {0, 0};               // scored songs this credit, per seat
    bool seats[2] = {false, false};      // seats that played the credit that just ended
    bool guest = false;                  // every seat is a guest (set leaving entry): game over shows the ad
    bool reward_shop_from_entry = false; // mode select opened the shop: no reward time, back to entry
    int bncoin = 99999;
};

struct GlobalData {
    Config* config = nullptr;
    PlayerNum player_num = PlayerNum::P1;
    PlayerNum first_login_player = PlayerNum::P1;
    std::array<SessionData, 3> session_data;
    std::array<int, 3> last_difficulty = {-1, -1, -1};
    std::array<fs::path, 3> dan_folder;
    CameraConfig camera;

    std::string current_screen = "LOADING";
    std::string previous_screen;
    bool in_transition = false;
    bool skinless = false;
    bool skin_reload_pending = false;
    bool needs_onboarding = false;
    bool returned_from_result = false;
    bool force_auto_play = false;
    bool hold_genre_voice = false; // a song select story/intro is running: folder voices wait for it
    int live_skip_count = -1;

    std::shared_ptr<PracticeTimer> practice_timer; // session countdown, spans practice select + game
    std::vector<int> battle_skills;                // the skill picker's slots; empty until the first battle select

    CardSession card;
    EntryResume entry;
    CampaignSession campaign;
    CreditSession credit;
};

void reset_session();
std::string get_player_id(PlayerNum player_num);
// Guest seats (network.guest_mode, no card): a temporary id per login slot that scores.db never stores
std::string guest_id(int slot);
bool is_guest_id(const std::string& player_id);
// every seated player is a guest: slot 1, and slot 2 too when two players are in
bool guests_only(bool two_player);
void load_skin();
void unload_skin();

void set_user_appearance(const std::string& skin, const std::string& language);
void clear_user_appearance();
void apply_user_appearance();
void revert_user_appearance();
std::string instance_skin();
std::string instance_language();

void queue_window_resize(int width, int height);
bool apply_queued_window_resize();

void drop_other_screens_for_skin_reload();
void reload_skin_screens();
// Rebuilds the current screen against a fresh Lua state, so script edits on disk take effect.
void reload_current_screen_scripts();

extern GlobalData global_data;
