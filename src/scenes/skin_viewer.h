#pragma once
#include "../libs/screen.h"
#include <vector>
#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

struct SvEntry {
    std::string name;
    bool is_framed;                    // true if backed by a numbered-frames dir
    fs::path tex_path;                 // .png file OR directory of numbered PNGs
    std::vector<int> frame_order;      // logical frame → file index mapping
    std::vector<ray::Rectangle> crops; // sub-regions within a single PNG

    ray::Texture2D single_tex{};
    std::vector<ray::Texture2D> frame_textures;
    bool loaded = false;
    int current_frame = 0;

    int total_frames() const;
    void load();
    void unload();
    std::pair<ray::Texture2D*, std::optional<ray::Rectangle>> frame_info();
};

struct SvFolder {
    std::string display_name;  // path relative to Graphics/
    fs::path folder_path;      // absolute dir containing texture.json
    std::vector<SvEntry> entries;
    bool parsed = false;

    void parse();
    void reload();
};

class SkinViewerScreen : public Screen {
    enum class Mode { FOLDERS, TEXTURES, EDIT_FRAME_ORDER };

    Mode mode = Mode::FOLDERS;
    std::vector<SvFolder> folders;

    int folder_idx    = 0;
    int folder_scroll = 0;
    int tex_idx       = 0;
    int tex_scroll    = 0;
    int loaded_idx    = -1;

    // Edit-frame-order state
    std::vector<int> edit_seq;       // sequence being built
    int  edit_file_idx    = 0;       // file index currently selected for appending
    int  edit_file_scroll = 0;
    std::string edit_status;
    double edit_status_until = 0.0;

    static constexpr int ITEM_H = 28;
    static constexpr int TOP_H  = 40;
    static constexpr int BOT_H  = 28;
    static constexpr int LIST_W = 300;

    SvFolder& cur_folder() { return folders[folder_idx]; }
    SvEntry*  cur_entry();

    void enter_folder();
    void leave_folder();
    void reload_current_folder();
    void reload_all_folders();
    void navigate_textures(int delta);
    void navigate_frames(int delta);
    void ensure_loaded(int idx);
    void unload_entry(int idx);

    void enter_edit_mode();
    void leave_edit_mode();
    void save_frame_order();

    void draw_list(int x, int y, int w, int h,
                   const std::vector<std::string>& labels,
                   int current, int scroll);
    void draw_folders();
    void draw_textures();
    void draw_edit_mode();

public:
    SkinViewerScreen() : Screen("skin_viewer") {}
    ~SkinViewerScreen();

    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
