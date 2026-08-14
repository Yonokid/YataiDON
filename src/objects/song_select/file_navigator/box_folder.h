#pragma once

#include "box_base.h"
#include "../../../libs/global_data.h"

class FolderBox : public BaseBox {
public:
    int tja_count;
    bool is_osu_folder = false;
    std::map<int, Crown> crown;
    bool entered = false;
    // The genre voice this box started, so closing only talks to the audio
    // engine when there is actually something to stop.
    bool genre_voice_started = false;
    std::unique_ptr<FadeAnimation> enter_fade;
    std::optional<ray::Texture> box_texture;

    std::unique_ptr<OutlinedText> hori_name;
    std::unique_ptr<OutlinedText> tja_count_text;

    FolderBox(const fs::path& path, const BoxDef& box_def, std::map<std::pair<std::string, std::string>, fs::path>& song_files);
    ~FolderBox() override;

    void load_text() override;
    void update(double current_time) override;

    void enter_box() override;
    void exit_box() override;

    void refresh_scores(std::map<std::pair<std::string, std::string>, fs::path>& song_files);
    // Runs every scan that refresh_scores put off. Called on the loader
    // thread once the whole wheel is up, so the boxes appear immediately and
    // their crowns and counts fill in behind them.
    static void run_deferred_scans(std::atomic<bool>& abort_flag);
    // True while this box's subtree scan is still queued or running.
    bool scan_pending = false;
    // Drop the cached crown/tja_count folder scans. Call whenever scores or
    // song lists change (after a play, favorite toggle, recent update).
    static void invalidate_scan_cache();

    const char* lua_kind() const override { return "folder"; }

protected:
    void draw_open_bg(float fade);
    void draw_open_fg(float fade);
    void draw_closed() override;
    void draw_open() override;
};
