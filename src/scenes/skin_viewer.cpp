#include "skin_viewer.h"
#include "../libs/texture.h"
#include "../libs/input.h"
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

namespace rj = rapidjson;

// ─── SvEntry ────────────────────────────────────────────────────────────────

int SvEntry::total_frames() const {
    if (is_framed) {
        if (!frame_order.empty()) return (int)frame_order.size();
        return (int)frame_textures.size();
    }
    if (!crops.empty()) return (int)crops.size();
    return 1;
}

void SvEntry::load() {
    if (loaded) return;
    if (is_framed) {
        std::vector<fs::path> files;
        for (const auto& e : fs::directory_iterator(tex_path))
            if (e.is_regular_file() && e.path().extension() == ".png")
                files.push_back(e.path());
        std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
            try { return std::stoi(a.stem().string()) < std::stoi(b.stem().string()); }
            catch (...) { return a < b; }
        });
        for (const auto& f : files) {
            frame_textures.push_back(ray::LoadTexture(f.string().c_str()));
            ray::GenTextureMipmaps(&frame_textures.back());
            ray::SetTextureFilter(frame_textures.back(), ray::TEXTURE_FILTER_TRILINEAR);
        }
    } else {
        single_tex = ray::LoadTexture(tex_path.string().c_str());
        ray::GenTextureMipmaps(&single_tex);
        ray::SetTextureFilter(single_tex, ray::TEXTURE_FILTER_TRILINEAR);
    }
    loaded = true;
}

void SvEntry::unload() {
    if (!loaded) return;
    if (is_framed) {
        for (auto& t : frame_textures) ray::UnloadTexture(t);
        frame_textures.clear();
    } else if (single_tex.id != 0) {
        ray::UnloadTexture(single_tex);
        single_tex = {};
    }
    loaded = false;
    // current_frame is intentionally not reset here so callers can restore it after reload
}

std::pair<ray::Texture2D*, std::optional<ray::Rectangle>> SvEntry::frame_info() {
    if (!loaded) return {nullptr, std::nullopt};

    if (is_framed) {
        if (frame_textures.empty()) return {nullptr, std::nullopt};
        int fcount = (int)frame_textures.size();
        int file_idx;
        if (!frame_order.empty()) {
            int fo = current_frame % (int)frame_order.size();
            file_idx = frame_order[fo] % fcount;
        } else {
            file_idx = current_frame % fcount;
        }
        return {&frame_textures[file_idx], std::nullopt};
    }

    // Single PNG
    if (single_tex.id == 0) return {nullptr, std::nullopt};
    if (!crops.empty()) {
        ray::Rectangle crop = crops[current_frame % (int)crops.size()];
        return {&single_tex, crop};
    }
    return {&single_tex, std::nullopt};
}

// ─── SvFolder ───────────────────────────────────────────────────────────────

void SvFolder::reload() {
    parsed = false;
    entries.clear();
    parse();
}

void SvFolder::parse() {
    if (parsed) return;
    entries.clear();

    fs::path json = folder_path / "texture.json";
    if (!fs::exists(json)) { parsed = true; return; }

    std::ifstream ifs(json);
    rj::IStreamWrapper isw(ifs);
    rj::Document doc;
    doc.ParseStream(isw);
    if (doc.HasParseError() || !doc.IsObject()) { parsed = true; return; }

    for (auto& m : doc.GetObject()) {
        SvEntry e;
        e.name          = m.name.GetString();
        e.current_frame = 0;

        fs::path dir_path = folder_path / e.name;
        fs::path png_path = folder_path / (e.name + ".png");

        e.is_framed = fs::is_directory(dir_path);
        e.tex_path  = e.is_framed ? dir_path : png_path;

        if (!e.is_framed && !fs::exists(png_path)) continue;

        const auto& val = m.value;
        if (val.IsObject()) {
            if (val.HasMember("frame_order") && val["frame_order"].IsArray()) {
                for (auto& v : val["frame_order"].GetArray())
                    e.frame_order.push_back(v.GetInt());
            }
            if (val.HasMember("crop") && val["crop"].IsArray()) {
                for (auto& c : val["crop"].GetArray()) {
                    if (c.IsArray() && c.Size() == 4)
                        e.crops.push_back({(float)c[0].GetInt(), (float)c[1].GetInt(),
                                           (float)c[2].GetInt(), (float)c[3].GetInt()});
                }
            }
        }

        entries.push_back(std::move(e));
    }
    parsed = true;
}

// ─── SkinViewerScreen ───────────────────────────────────────────────────────

SkinViewerScreen::~SkinViewerScreen() {
    unload_entry(loaded_idx);
}

void SkinViewerScreen::on_screen_start() {
    fs::path skin_gfx = fs::path("Skins") / global_data.config->paths.skin / "Graphics";
    folders.clear();
    folder_idx = folder_scroll = tex_idx = tex_scroll = 0;
    loaded_idx = -1;
    mode       = Mode::FOLDERS;

    if (!fs::exists(skin_gfx)) return;

    for (const auto& e : fs::recursive_directory_iterator(skin_gfx)) {
        if (e.is_regular_file() && e.path().filename() == "texture.json") {
            SvFolder f;
            f.folder_path   = e.path().parent_path();
            f.display_name  = fs::relative(f.folder_path, skin_gfx).string();
            folders.push_back(std::move(f));
        }
    }
    std::sort(folders.begin(), folders.end(),
              [](const SvFolder& a, const SvFolder& b) { return a.display_name < b.display_name; });
}

Screens SkinViewerScreen::on_screen_end(Screens next_screen) {
    unload_entry(loaded_idx);
    folders.clear();
    screen_init = false;
    return next_screen;
}

SvEntry* SkinViewerScreen::cur_entry() {
    if (folder_idx >= (int)folders.size()) return nullptr;
    auto& f = cur_folder();
    if (!f.parsed || f.entries.empty() || tex_idx >= (int)f.entries.size()) return nullptr;
    return &f.entries[tex_idx];
}

void SkinViewerScreen::ensure_loaded(int idx) {
    if (loaded_idx == idx) return;
    unload_entry(loaded_idx);
    auto& f = cur_folder();
    if (idx >= 0 && idx < (int)f.entries.size()) {
        f.entries[idx].load();
        loaded_idx = idx;
    }
}

void SkinViewerScreen::unload_entry(int idx) {
    if (idx < 0 || folder_idx >= (int)folders.size()) return;
    auto& f = folders[folder_idx];
    if (idx < (int)f.entries.size()) f.entries[idx].unload();
    if (loaded_idx == idx) loaded_idx = -1;
}

void SkinViewerScreen::enter_folder() {
    if (folders.empty()) return;
    unload_entry(loaded_idx);
    auto& f = cur_folder();
    f.parse();
    tex_idx = tex_scroll = 0;
    mode = Mode::TEXTURES;
    if (!f.entries.empty()) ensure_loaded(0);
}

void SkinViewerScreen::reload_current_folder() {
    // Save state before invalidating anything
    std::string prev_name;
    int prev_frame = 0;
    if (auto* e = cur_entry()) {
        prev_name  = e->name;
        prev_frame = e->current_frame;
    }

    unload_entry(loaded_idx);
    loaded_idx = -1;

    auto& f = cur_folder();
    f.reload();

    // Restore by name so a reordered JSON doesn't land on the wrong entry
    tex_idx = 0;
    if (!prev_name.empty()) {
        for (int i = 0; i < (int)f.entries.size(); i++) {
            if (f.entries[i].name == prev_name) { tex_idx = i; break; }
        }
    }
    tex_idx = std::max(0, std::min(tex_idx, (int)f.entries.size() - 1));

    // Keep the entry visible in the scroll window
    int visible = (tex.screen_height - TOP_H - BOT_H) / ITEM_H;
    if (tex_idx < tex_scroll) tex_scroll = tex_idx;
    if (tex_idx >= tex_scroll + visible) tex_scroll = tex_idx - visible + 1;
    if (tex_scroll < 0) tex_scroll = 0;

    if (!f.entries.empty()) {
        ensure_loaded(tex_idx);
        // Restore frame position, clamped to the (possibly updated) frame count
        if (auto* e = cur_entry()) {
            int n = e->total_frames();
            e->current_frame = (n > 1) ? prev_frame % n : 0;
        }
    }
}

void SkinViewerScreen::reload_all_folders() {
    std::string prev_name = folder_idx < (int)folders.size()
                            ? folders[folder_idx].display_name : "";
    folders.clear();
    folder_idx = 0;

    fs::path skin_gfx = fs::path("Skins") / global_data.config->paths.skin / "Graphics";
    if (!fs::exists(skin_gfx)) return;

    for (const auto& e : fs::recursive_directory_iterator(skin_gfx)) {
        if (e.is_regular_file() && e.path().filename() == "texture.json") {
            SvFolder f;
            f.folder_path  = e.path().parent_path();
            f.display_name = fs::relative(f.folder_path, skin_gfx).string();
            folders.push_back(std::move(f));
        }
    }
    std::sort(folders.begin(), folders.end(),
              [](const SvFolder& a, const SvFolder& b) { return a.display_name < b.display_name; });

    if (!prev_name.empty()) {
        for (int i = 0; i < (int)folders.size(); i++) {
            if (folders[i].display_name == prev_name) { folder_idx = i; break; }
        }
    }
    folder_idx = std::max(0, std::min(folder_idx, (int)folders.size() - 1));
}

void SkinViewerScreen::enter_edit_mode() {
    auto* e = cur_entry();
    if (!e || !e->is_framed || !e->loaded) return;
    edit_seq         = e->frame_order;    // start from existing sequence
    edit_file_idx    = 0;
    edit_file_scroll = 0;
    edit_status.clear();
    edit_status_until = 0.0;
    mode = Mode::EDIT_FRAME_ORDER;
}

void SkinViewerScreen::leave_edit_mode() {
    edit_seq.clear();
    mode = Mode::TEXTURES;
}

void SkinViewerScreen::save_frame_order() {
    auto& f = cur_folder();
    auto* e = cur_entry();
    if (!e) return;

    fs::path json_path = f.folder_path / "texture.json";

    rj::Document doc;
    {
        std::ifstream ifs(json_path);
        if (!ifs.is_open()) { edit_status = "Error: cannot open texture.json"; return; }
        rj::IStreamWrapper isw(ifs);
        doc.ParseStream(isw);
    }
    if (doc.HasParseError() || !doc.IsObject()) {
        edit_status = "Error: invalid JSON";
        return;
    }

    if (!doc.HasMember(e->name.c_str())) {
        edit_status = "Error: entry '" + e->name + "' not found in JSON";
        return;
    }

    auto& entry_val = doc[e->name.c_str()];
    if (!entry_val.IsObject()) entry_val.SetObject();

    auto& alloc = doc.GetAllocator();
    rj::Value arr(rj::kArrayType);
    for (int idx : edit_seq)
        arr.PushBack(idx, alloc);

    if (entry_val.HasMember("frame_order"))
        entry_val["frame_order"] = std::move(arr);
    else
        entry_val.AddMember("frame_order", std::move(arr), alloc);

    {
        std::ofstream ofs(json_path);
        if (!ofs.is_open()) { edit_status = "Error: cannot write texture.json"; return; }
        rj::OStreamWrapper osw(ofs);
        rj::PrettyWriter<rj::OStreamWrapper> writer(osw);
        writer.SetIndent(' ', 2);
        doc.Accept(writer);
    }

    e->frame_order = edit_seq;
    edit_status       = "Saved!";
    edit_status_until = get_current_ms() + 2000.0;
}

void SkinViewerScreen::leave_folder() {
    unload_entry(loaded_idx);
    mode = Mode::FOLDERS;
}

void SkinViewerScreen::navigate_textures(int delta) {
    auto& f = cur_folder();
    if (f.entries.empty()) return;
    int n = (int)f.entries.size();
    unload_entry(loaded_idx);
    tex_idx = (tex_idx + delta + n) % n;
    ensure_loaded(tex_idx);

    int visible = (tex.screen_height - TOP_H - BOT_H) / ITEM_H;
    if (tex_idx < tex_scroll) tex_scroll = tex_idx;
    if (tex_idx >= tex_scroll + visible) tex_scroll = tex_idx - visible + 1;
    if (auto* e = cur_entry()) e->current_frame = 0;
}

void SkinViewerScreen::navigate_frames(int delta) {
    auto* e = cur_entry();
    if (!e || !e->loaded) return;
    int n = e->total_frames();
    if (n <= 1) return;
    e->current_frame = (e->current_frame + delta + n) % n;
}

// ─── Drawing ────────────────────────────────────────────────────────────────

void SkinViewerScreen::draw_list(int x, int y, int w, int h,
                                  const std::vector<std::string>& labels,
                                  int current, int scroll)
{
    ray::DrawRectangle(x, y, w, h, {20, 20, 20, 255});
    int visible = h / ITEM_H;
    for (int i = 0; i < visible; i++) {
        int li = scroll + i;
        if (li >= (int)labels.size()) break;
        int iy = y + i * ITEM_H;
        bool sel = (li == current);
        if (sel) ray::DrawRectangle(x, iy, w, ITEM_H, {55, 110, 200, 255});

        // Truncate label to fit
        std::string lbl = labels[li];
        while (lbl.size() > 1 && ray::MeasureText(lbl.c_str(), 18) > w - 8)
            lbl = lbl.substr(0, lbl.size() - 1);
        ray::DrawText(lbl.c_str(), x + 5, iy + 5, 18, sel ? ray::WHITE : ray::LIGHTGRAY);
    }
}

void SkinViewerScreen::draw_folders() {
    int vw = tex.screen_width;
    int vh = tex.screen_height;
    ray::DrawRectangle(0, 0, vw, vh, ray::BLACK);

    // Top bar
    ray::DrawRectangle(0, 0, vw, TOP_H, {30, 30, 30, 255});
    ray::DrawText("SKIN VIEWER  —  Select folder", 10, 10, 22, ray::YELLOW);
    std::string cnt = std::to_string(folder_idx + 1) + " / " + std::to_string((int)folders.size());
    ray::DrawText(cnt.c_str(), vw - ray::MeasureText(cnt.c_str(), 22) - 10, 10, 22, ray::WHITE);

    // Folder list (full width)
    int list_h = vh - TOP_H - BOT_H;
    int visible = list_h / ITEM_H;
    if (folder_idx < folder_scroll) folder_scroll = folder_idx;
    if (folder_idx >= folder_scroll + visible) folder_scroll = folder_idx - visible + 1;

    std::vector<std::string> labels;
    labels.reserve(folders.size());
    for (auto& f : folders) labels.push_back(f.display_name);
    draw_list(0, TOP_H, vw, list_h, labels, folder_idx, folder_scroll);

    // Bottom hint
    ray::DrawRectangle(0, vh - BOT_H, vw, BOT_H, {30, 30, 30, 255});
    const char* hint = "Kat / ↑↓: navigate    Don / Enter: open folder    F1: rescan    ESC: exit";
    ray::DrawText(hint, (vw - ray::MeasureText(hint, 18)) / 2, vh - BOT_H + 5, 18, ray::DARKGRAY);
}

void SkinViewerScreen::draw_textures() {
    int vw = tex.screen_width;
    int vh = tex.screen_height;
    ray::DrawRectangle(0, 0, vw, vh, ray::BLACK);

    auto& folder = cur_folder();
    auto* entry  = cur_entry();

    // ── Left panel: texture name list ──
    int list_h = vh - TOP_H - BOT_H;
    std::vector<std::string> labels;
    labels.reserve(folder.entries.size());
    for (auto& e : folder.entries) labels.push_back(e.name);
    draw_list(0, TOP_H, LIST_W, list_h, labels, tex_idx, tex_scroll);

    // Separator line
    ray::DrawRectangle(LIST_W, TOP_H, 2, list_h, {50, 50, 50, 255});

    // ── Right panel: image preview ──
    int px = LIST_W + 6;
    int pw = vw - px - 4;
    int py = TOP_H + 4;
    int ph = list_h - 8;

    if (entry && entry->loaded) {
        auto [draw_tex, crop] = entry->frame_info();

        if (draw_tex && draw_tex->id != 0) {
            float tw = (float)draw_tex->width;
            float th = (float)draw_tex->height;
            float scale = std::min((float)pw / tw, (float)ph / th);
            float dw = tw * scale;
            float dh = th * scale;
            float dx = px + ((float)pw - dw) * 0.5f;
            float dy = py + ((float)ph - dh) * 0.5f;

            // Checkerboard background (2 alternating rows = cheap)
            for (int row = 0; row < (int)dh; row += 12) {
                bool dark = (row / 12) % 2 == 0;
                ray::Color bg = dark ? ray::Color{140, 140, 140, 255} : ray::Color{180, 180, 180, 255};
                ray::DrawRectangle((int)dx, (int)(dy + row), (int)dw, std::min(12, (int)dh - row), bg);
            }

            ray::Rectangle src = {0, 0, tw, th};
            ray::Rectangle dst = {dx, dy, dw, dh};
            ray::DrawTexturePro(*draw_tex, src, dst, {0, 0}, 0.0f, ray::WHITE);

            // Crop overlay
            if (crop.has_value()) {
                float hx = dx + crop->x * scale;
                float hy = dy + crop->y * scale;
                float hw = crop->width  * scale;
                float hh = crop->height * scale;
                // Dim the non-crop area
                ray::DrawRectangle((int)dx, (int)dy, (int)dw, (int)(hy - dy),      {0,0,0,100});
                ray::DrawRectangle((int)dx, (int)(hy+hh), (int)dw, (int)(dy+dh-hy-hh), {0,0,0,100});
                ray::DrawRectangle((int)dx, (int)hy, (int)(hx-dx), (int)hh,        {0,0,0,100});
                ray::DrawRectangle((int)(hx+hw), (int)hy, (int)(dx+dw-hx-hw), (int)hh, {0,0,0,100});
                ray::DrawRectangleLinesEx({hx, hy, hw, hh}, 2.0f, ray::YELLOW);

                // Crop dims
                char cdim[64];
                snprintf(cdim, sizeof(cdim), "crop %dx%d @ (%d,%d)",
                         (int)crop->width, (int)crop->height,
                         (int)crop->x, (int)crop->y);
                ray::DrawText(cdim, px + 4, py + 4, 16, ray::YELLOW);
            }

            ray::DrawRectangleLinesEx({dx, dy, dw, dh}, 1.0f, ray::DARKGRAY);

            // Image dims (bottom-right of preview)
            char dims[32];
            snprintf(dims, sizeof(dims), "%dx%d", draw_tex->width, draw_tex->height);
            int dw2 = ray::MeasureText(dims, 16);
            ray::DrawText(dims, (int)(dx + dw) - dw2 - 4, (int)(dy + dh) - 20, 16, {200,200,200,200});
        }
    } else if (entry) {
        ray::DrawText("Loading...", px + 20, py + 20, 24, ray::GRAY);
    } else {
        ray::DrawText("(empty folder)", px + 20, py + 20, 24, ray::GRAY);
    }

    // ── Top bar ──
    ray::DrawRectangle(0, 0, vw, TOP_H, {30, 30, 30, 255});
    std::string header = folder.display_name;
    if (entry) header += "  ›  " + entry->name;
    ray::DrawText(header.c_str(), 10, 10, 22, ray::YELLOW);

    if (entry) {
        int n = entry->total_frames();
        std::string finfo;
        if (n > 1) {
            finfo = "frame " + std::to_string(entry->current_frame + 1) + " / " + std::to_string(n);
            if (entry->is_framed && !entry->frame_order.empty()) {
                int file_idx = entry->frame_order[entry->current_frame % (int)entry->frame_order.size()];
                finfo += "   (file " + std::to_string(file_idx) + ")";
            }
        } else {
            finfo = "1 frame";
        }
        int fw = ray::MeasureText(finfo.c_str(), 20);
        ray::DrawText(finfo.c_str(), vw - fw - 10, 11, 20, ray::WHITE);
    }

    // ── Bottom hint ──
    ray::DrawRectangle(0, vh - BOT_H, vw, BOT_H, {30, 30, 30, 255});
    const char* hint = (entry && entry->is_framed)
        ? "Kat/↑↓: texture    Don/←→: frame    F2: edit frame_order    F1: reload    ESC: folders"
        : "Kat / ↑↓: texture    Don / ←→: frame    F1: reload    ESC: folders";
    ray::DrawText(hint, (vw - ray::MeasureText(hint, 18)) / 2, vh - BOT_H + 5, 18, ray::DARKGRAY);
}

void SkinViewerScreen::draw_edit_mode() {
    int vw = tex.screen_width;
    int vh = tex.screen_height;
    ray::DrawRectangle(0, 0, vw, vh, ray::BLACK);

    auto* e = cur_entry();
    int file_count = e ? (int)e->frame_textures.size() : 0;

    // ── Top bar ──
    ray::DrawRectangle(0, 0, vw, TOP_H, {30, 30, 30, 255});
    std::string title = "EDIT frame_order: " + cur_folder().display_name;
    if (e) title += "  ›  " + e->name;
    ray::DrawText(title.c_str(), 10, 10, 22, ray::YELLOW);

    // Status message (top-right, fades out)
    if (!edit_status.empty()) {
        bool saved = (edit_status == "Saved!");
        ray::Color sc = saved ? ray::GREEN : ray::RED;
        int sw = ray::MeasureText(edit_status.c_str(), 22);
        ray::DrawText(edit_status.c_str(), vw - sw - 10, 10, 22, sc);
    }

    int content_y = TOP_H;
    int content_h = vh - TOP_H - BOT_H;

    // ── Left panel: file list ──
    // Update scroll to keep selection visible
    int list_visible = content_h / ITEM_H;
    if (edit_file_idx < edit_file_scroll) edit_file_scroll = edit_file_idx;
    if (edit_file_idx >= edit_file_scroll + list_visible)
        edit_file_scroll = edit_file_idx - list_visible + 1;
    if (edit_file_scroll < 0) edit_file_scroll = 0;

    ray::DrawRectangle(0, content_y, LIST_W, content_h, {20, 20, 20, 255});
    for (int i = 0; i < list_visible; i++) {
        int fi = edit_file_scroll + i;
        if (fi >= file_count) break;
        int iy = content_y + i * ITEM_H;
        bool sel = (fi == edit_file_idx);
        if (sel) ray::DrawRectangle(0, iy, LIST_W, ITEM_H, {55, 110, 200, 255});
        std::string lbl = "File " + std::to_string(fi);
        ray::DrawText(lbl.c_str(), 8, iy + 5, 18, sel ? ray::WHITE : ray::LIGHTGRAY);
    }

    // Separator
    ray::DrawRectangle(LIST_W, content_y, 2, content_h, {50, 50, 50, 255});

    // ── Right panel ──
    int px = LIST_W + 6;
    int pw = vw - px - 4;

    // Preview area: top 60% of content height
    int preview_h = content_h * 6 / 10;
    int seq_y     = content_y + preview_h;
    int seq_h     = content_h - preview_h;

    // Preview of currently selected file
    if (e && e->loaded && edit_file_idx < file_count) {
        auto& draw_tex = e->frame_textures[edit_file_idx];
        if (draw_tex.id != 0) {
            float tw = (float)draw_tex.width;
            float th = (float)draw_tex.height;
            float scale = std::min((float)pw / tw, (float)preview_h / th);
            float dw = tw * scale, dh = th * scale;
            float dx = px + ((float)pw - dw) * 0.5f;
            float dy = content_y + ((float)preview_h - dh) * 0.5f;

            for (int row = 0; row < (int)dh; row += 12) {
                bool dark = (row / 12) % 2 == 0;
                ray::Color bg = dark ? ray::Color{140,140,140,255} : ray::Color{180,180,180,255};
                ray::DrawRectangle((int)dx, (int)(dy+row), (int)dw, std::min(12,(int)dh-row), bg);
            }
            ray::DrawTexturePro(draw_tex, {0,0,tw,th}, {dx,dy,dw,dh}, {0,0}, 0, ray::WHITE);
            ray::DrawRectangleLinesEx({dx,dy,dw,dh}, 1, ray::DARKGRAY);

            char dims[32];
            snprintf(dims, sizeof(dims), "File %d  (%dx%d)", edit_file_idx, draw_tex.width, draw_tex.height);
            ray::DrawText(dims, px + 4, content_y + 4, 18, ray::LIGHTGRAY);
        }
    }

    // Divider between preview and sequence area
    ray::DrawRectangle(px, seq_y, pw, 2, {50, 50, 50, 255});

    // ── Sequence display ──
    ray::DrawRectangle(px, seq_y + 2, pw, seq_h - 2, {15, 15, 15, 255});

    constexpr int BOX_W = 46, BOX_H = 34, BOX_GAP = 4;
    int boxes_per_row = std::max(1, pw / (BOX_W + BOX_GAP));
    int seq_label_y = seq_y + 6;
    char seq_hdr[64];
    snprintf(seq_hdr, sizeof(seq_hdr), "frame_order  (%d frames):", (int)edit_seq.size());
    ray::DrawText(seq_hdr, px + 4, seq_label_y, 18, ray::WHITE);

    int box_start_y = seq_label_y + 24;
    int rows_available = (seq_h - 34) / (BOX_H + BOX_GAP);
    int max_boxes = boxes_per_row * rows_available;

    // Show the last `max_boxes` entries so the end (cursor) is always visible
    int seq_start = std::max(0, (int)edit_seq.size() - max_boxes + 1);

    for (int i = 0; i < (int)edit_seq.size() && (i - seq_start) < max_boxes; i++) {
        if (i < seq_start) continue;
        int bi = i - seq_start;
        int col = bi % boxes_per_row;
        int row = bi / boxes_per_row;
        int bx = px + 4 + col * (BOX_W + BOX_GAP);
        int by = box_start_y + row * (BOX_H + BOX_GAP);

        bool is_last = (i == (int)edit_seq.size() - 1);
        ray::Color bg = is_last ? ray::Color{80, 140, 80, 255} : ray::Color{50, 50, 80, 255};
        ray::DrawRectangle(bx, by, BOX_W, BOX_H, bg);
        ray::DrawRectangleLinesEx({(float)bx,(float)by,(float)BOX_W,(float)BOX_H}, 1, ray::DARKGRAY);

        char num[8];
        snprintf(num, sizeof(num), "%d", edit_seq[i]);
        int nw = ray::MeasureText(num, 18);
        ray::DrawText(num, bx + (BOX_W - nw) / 2, by + (BOX_H - 18) / 2, 18, ray::WHITE);
    }

    // Cursor box (append position)
    {
        int bi = (int)edit_seq.size() - seq_start;
        if (bi >= 0 && bi < max_boxes) {
            int col = bi % boxes_per_row;
            int row = bi / boxes_per_row;
            int bx = px + 4 + col * (BOX_W + BOX_GAP);
            int by = box_start_y + row * (BOX_H + BOX_GAP);
            ray::DrawRectangleLinesEx({(float)bx,(float)by,(float)BOX_W,(float)BOX_H}, 2, ray::YELLOW);

            // Show what would be appended
            char num[8];
            snprintf(num, sizeof(num), "%d", edit_file_idx);
            int nw = ray::MeasureText(num, 18);
            ray::DrawText(num, bx + (BOX_W - nw) / 2, by + (BOX_H - 18) / 2, 18, {200,200,80,180});
        }
    }

    // ── Bottom hint ──
    ray::DrawRectangle(0, vh - BOT_H, vw, BOT_H, {30, 30, 30, 255});
    const char* hint = "Kat/↑↓: file    Don R/→: append    Don L/←: remove last    Enter: save    ESC: cancel";
    ray::DrawText(hint, (vw - ray::MeasureText(hint, 18)) / 2, vh - BOT_H + 5, 18, ray::DARKGRAY);
}

void SkinViewerScreen::draw() {
    if (mode == Mode::FOLDERS)
        draw_folders();
    else if (mode == Mode::TEXTURES)
        draw_textures();
    else
        draw_edit_mode();
}

// ─── Update ─────────────────────────────────────────────────────────────────

std::optional<Screens> SkinViewerScreen::update() {
    Screen::update();

    if (mode == Mode::FOLDERS) {
        int n = (int)folders.size();
        if (n == 0) return std::nullopt;

        if (is_l_kat_pressed() || check_key_pressed(ray::KEY_UP))
            folder_idx = (folder_idx - 1 + n) % n;
        else if (is_r_kat_pressed() || check_key_pressed(ray::KEY_DOWN))
            folder_idx = (folder_idx + 1) % n;
        else if (is_l_don_pressed() || is_r_don_pressed() || check_key_pressed(ray::KEY_ENTER))
            enter_folder();
        else if (check_key_pressed(ray::KEY_F1))
            reload_all_folders();
        else if (check_key_pressed(ray::KEY_ESCAPE))
            return on_screen_end(Screens::SETTINGS);

    } else if (mode == Mode::TEXTURES) {
        if (is_l_kat_pressed() || check_key_pressed(ray::KEY_UP))
            navigate_textures(-1);
        else if (is_r_kat_pressed() || check_key_pressed(ray::KEY_DOWN))
            navigate_textures(1);
        else if (is_l_don_pressed() || check_key_pressed(ray::KEY_LEFT))
            navigate_frames(-1);
        else if (is_r_don_pressed() || check_key_pressed(ray::KEY_RIGHT))
            navigate_frames(1);
        else if (check_key_pressed(ray::KEY_F2))
            enter_edit_mode();
        else if (check_key_pressed(ray::KEY_F1))
            reload_current_folder();
        else if (check_key_pressed(ray::KEY_ESCAPE))
            leave_folder();

    } else {  // EDIT_FRAME_ORDER
        auto* e = cur_entry();
        int file_count = e ? (int)e->frame_textures.size() : 0;

        if ((is_l_kat_pressed() || check_key_pressed(ray::KEY_UP)) && file_count > 0)
            edit_file_idx = (edit_file_idx - 1 + file_count) % file_count;
        else if ((is_r_kat_pressed() || check_key_pressed(ray::KEY_DOWN)) && file_count > 0)
            edit_file_idx = (edit_file_idx + 1) % file_count;
        else if (is_r_don_pressed() || check_key_pressed(ray::KEY_RIGHT))
            edit_seq.push_back(edit_file_idx);
        else if (is_l_don_pressed() || check_key_pressed(ray::KEY_LEFT)) {
            if (!edit_seq.empty()) edit_seq.pop_back();
        } else if (check_key_pressed(ray::KEY_ENTER))
            save_frame_order();
        else if (check_key_pressed(ray::KEY_ESCAPE))
            leave_edit_mode();
    }

    return std::nullopt;
}
