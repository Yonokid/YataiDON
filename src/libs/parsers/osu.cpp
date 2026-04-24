#include "osu.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <cmath>
#include <digestpp/algorithm/md5.hpp>
#include <spdlog/spdlog.h>

std::vector<std::string> OsuParser::read_file_lines(const fs::path& path) {
    std::vector<std::string> lines;
    std::ifstream f(path, std::ios::binary);
    if (!f) return lines;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

std::map<std::string, std::string> OsuParser::read_section_dict(
    const std::vector<std::string>& lines, const std::string& section) const
{
    std::map<std::string, std::string> result;
    bool in_section = false;
    for (const auto& line : lines) {
        if (line == "[" + section + "]") { in_section = true; continue; }
        if (!line.empty() && line[0] == '[') { in_section = false; continue; }
        if (!in_section) continue;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        // trim whitespace
        auto ltrim = [](std::string& s) { s.erase(0, s.find_first_not_of(" \t")); };
        auto rtrim = [](std::string& s) { auto p = s.find_last_not_of(" \t\r\n"); if (p != std::string::npos) s.erase(p+1); };
        ltrim(key); rtrim(key); ltrim(val); rtrim(val);
        result[key] = val;
    }
    return result;
}

std::vector<std::vector<double>> OsuParser::read_section_list(
    const std::vector<std::string>& lines, const std::string& section) const
{
    static const std::regex num_re(R"([-+]?\d*\.?\d+)");
    std::vector<std::vector<double>> result;
    bool in_section = false;
    for (const auto& line : lines) {
        if (line == "[" + section + "]") { in_section = true; continue; }
        if (!line.empty() && line[0] == '[') { in_section = false; continue; }
        if (!in_section) continue;
        if (line.empty() || line[0] == '/' || line[0] == '\r') continue;
        // only parse lines that start with a digit or sign
        if (!std::isdigit((unsigned char)line[0]) && line[0] != '-' && line[0] != '+') continue;
        std::vector<double> nums;
        auto begin = std::sregex_iterator(line.begin(), line.end(), num_re);
        auto end   = std::sregex_iterator();
        for (auto it = begin; it != end; ++it)
            nums.push_back(std::stod(it->str()));
        if (!nums.empty()) result.push_back(nums);
    }
    return result;
}

double OsuParser::get_scroll_multiplier(double ms) const {
    double base_scroll = (slider_multiplier >= 1.37 && slider_multiplier <= 1.47)
                         ? 1.0
                         : slider_multiplier / 1.40;
    double current_scroll = 1.0;
    for (const auto& tp : timing_points) {
        if (tp[0] > ms) break;
        if (tp[1] < 0)
            current_scroll = -100.0 / tp[1];
    }
    return current_scroll * base_scroll;
}

double OsuParser::get_bpm_at(double ms) const {
    double bpm = 120.0;
    for (const auto& tp : timing_points) {
        if (tp[0] > ms) break;
        if (tp[1] > 0 && tp[1] < 60000)
            bpm = std::floor(60000.0 / tp[1]);
    }
    return bpm;
}

OsuParser::OsuParser(const fs::path& path) : file_path(path) {
    metadata = TJAMetadata();
    ex_data  = TJAEXData();

    auto lines = read_file_lines(path);
    if (lines.empty()) {
        spdlog::warn("OsuParser: empty or unreadable file: {}", path.string());
        return;
    }

    auto general    = read_section_dict(lines, "General");
    auto osu_meta   = read_section_dict(lines, "Metadata");
    auto difficulty = read_section_dict(lines, "Difficulty");
    auto tp_data    = read_section_list(lines, "TimingPoints");
    hit_objects_data = read_section_list(lines, "HitObjects");

    // Slider multiplier
    if (difficulty.count("SliderMultiplier"))
        slider_multiplier = std::stod(difficulty["SliderMultiplier"]);

    // Timing points: store {time_ms, beat_length}
    for (const auto& row : tp_data) {
        if (row.size() >= 2)
            timing_points.push_back({row[0], row[1]});
    }

    // Metadata
    if (osu_meta.count("Version"))
        metadata.title["en"] = osu_meta["Version"];
    if (osu_meta.count("Creator"))
        metadata.subtitle["en"] = osu_meta["Creator"];
    if (osu_meta.count("TitleUnicode"))
        metadata.title["ja"] = osu_meta["TitleUnicode"];
    if (osu_meta.count("ArtistUnicode"))
        metadata.subtitle["ja"] = osu_meta["ArtistUnicode"];

    if (general.count("AudioFilename"))
        metadata.wave = path.parent_path() / general["AudioFilename"];
    if (general.count("PreviewTime"))
        metadata.demostart = std::stod(general["PreviewTime"]) / 1000.0;

    metadata.offset = -30.0 / 1000.0;

    // Background/video from Events section
    {
        std::string full_text;
        bool in_events = false;
        for (const auto& l : lines) {
            if (l == "[Events]") { in_events = true; continue; }
            if (!l.empty() && l[0] == '[') { if (in_events) break; continue; }
            if (in_events) { full_text += l + "\n"; }
        }
        // Look for video or background: "Video,offset,"filename"" or "0,0,"filename""
        static const std::regex bg_re(R"re(^\s*(?:Video|\d+),\d+,"([^"]+)")re");
        std::istringstream ss(full_text);
        std::string el;
        while (std::getline(ss, el)) {
            std::smatch m;
            if (std::regex_search(el, m, bg_re)) {
                fs::path candidate = path.parent_path() / m[1].str();
                std::string ext = candidate.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".mp4" || ext == ".avi" || ext == ".mkv" || ext == ".mov" || ext == ".flv")
                    metadata.bgmovie = candidate;
                else if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp")
                    metadata.preimage = candidate;
                break;
            }
        }
    }

    // BPM from first uninherited timing point
    double first_beat_length = 500.0; // default 120 BPM
    for (const auto& tp : timing_points) {
        if (tp[1] > 0 && tp[1] < 60000) {
            first_beat_length = tp[1];
            break;
        }
    }
    metadata.bpm = std::floor(60000.0 / first_beat_length);

    // One course entry (difficulty 0 = Easy slot, used for all osu songs)
    metadata.course_data[0] = CourseData{};
}

NoteList& OsuParser::get_notes() {
    if (notes_built) return cached_notes;
    notes_built = true;

    double first_bpm = metadata.bpm;
    double first_beat_length = (first_bpm > 0) ? 60000.0 / first_bpm : 500.0;

    int counter = 0;

    for (const auto& line : hit_objects_data) {
        if (line.size() < 5) continue;

        double note_time = line[2];
        int obj_type     = (int)line[3];
        int hit_sound    = (int)line[4];
        double scroll    = get_scroll_multiplier(note_time);
        double bpm_here  = get_bpm_at(note_time);

        bool is_circle  = (obj_type & 1) != 0;  // type bit 0
        bool is_slider  = (obj_type & 2) != 0;  // type bit 1
        bool is_spinner = (obj_type & 8) != 0;  // type bit 3

        if (is_circle) {
            Note note;
            note.hit_ms  = note_time;
            note.bpm     = bpm_here;
            note.scroll_x = scroll;
            note.scroll_y = 0.0;
            note.display  = true;
            note.index    = counter++;
            note.moji     = 1;

            if (hit_sound == 0) {
                note.type = NoteType::DON;
            } else if (hit_sound == 2 || hit_sound == 8) {
                note.type = NoteType::KAT;
                note.moji = 4;
            } else if (hit_sound == 4) {
                note.type = NoteType::DON_L;
                note.moji = 5;
            } else if (hit_sound == 6 || hit_sound == 12) {
                note.type = NoteType::KAT_L;
                note.moji = 6;
            } else {
                // Fallback: odd sounds → don, even → kat
                note.type = ((hit_sound & 2) || (hit_sound & 8)) ? NoteType::KAT : NoteType::DON;
            }

            cached_notes.notes.push_back(note);

        } else if (is_slider) {
            // Drumroll: compute duration
            double slider_len = 0.0;
            if (line.size() >= 9)
                slider_len = line[8];
            else if (line.size() >= 7)
                slider_len = line[6];

            double beat_len_at = first_beat_length;
            for (const auto& tp : timing_points) {
                if (tp[0] > note_time) break;
                if (tp[1] > 0) beat_len_at = tp[1];
            }
            double slider_time = slider_len / (slider_multiplier * 100.0) * beat_len_at;

            bool big = (hit_sound == 4 || hit_sound == 6 || hit_sound == 12);
            NoteType head_type = big ? NoteType::ROLL_HEAD_L : NoteType::ROLL_HEAD;

            Note head;
            head.type     = head_type;
            head.hit_ms   = note_time;
            head.bpm      = bpm_here;
            head.scroll_x = scroll;
            head.scroll_y = 0.0;
            head.display  = true;
            head.index    = counter++;
            head.color    = 255;
            head.moji     = big ? 8 : 7;

            Note tail;
            tail.type     = NoteType::TAIL;
            tail.hit_ms   = note_time + slider_time;
            tail.bpm      = bpm_here;
            tail.scroll_x = scroll;
            tail.scroll_y = 0.0;
            tail.display  = true;
            tail.index    = counter++;
            tail.moji     = 10;

            cached_notes.notes.push_back(head);
            cached_notes.notes.push_back(tail);

        } else if (is_spinner) {
            // Balloon
            double end_time = (line.size() >= 6) ? line[5] : note_time + 1000.0;

            Note head;
            head.type     = NoteType::BALLOON_HEAD;
            head.hit_ms   = note_time;
            head.bpm      = bpm_here;
            head.scroll_x = scroll;
            head.scroll_y = 0.0;
            head.display  = true;
            head.index    = counter++;
            head.count    = 20;
            head.moji     = 10;

            Note tail;
            tail.type     = NoteType::TAIL;
            tail.hit_ms   = end_time;
            tail.bpm      = bpm_here;
            tail.scroll_x = scroll;
            tail.scroll_y = 0.0;
            tail.display  = true;
            tail.index    = counter++;
            tail.moji     = 9;

            cached_notes.notes.push_back(head);
            cached_notes.notes.push_back(tail);
        }
    }

    // Build timeline from uninherited timing points (BPM changes)
    for (const auto& tp : timing_points) {
        if (tp[1] > 0 && tp[1] < 60000) {
            TimelineObject tl;
            tl.start_time = tp[0];
            tl.end_time   = tp[0];
            tl.bpm        = std::floor(60000.0 / tp[1]);
            cached_notes.timeline.push_back(tl);
        }
    }

    modifier_moji(cached_notes);
    return cached_notes;
}

std::tuple<NoteList, std::deque<NoteList>, std::deque<NoteList>, std::deque<NoteList>>
OsuParser::notes_to_position(int /*diff*/) {
    return {get_notes(), {}, {}, {}};
}

std::string OsuParser::get_diff_hash(int /*difficulty*/) {
    const NoteList& notes = get_notes();
    if (notes.notes.empty()) return "";
    digestpp::md5 hasher;
    for (const Note& n : notes.notes) {
        auto h = n.hash();
        hasher.absorb(reinterpret_cast<const char*>(&h), sizeof(h));
    }
    return hasher.hexdigest();
}

std::string OsuParser::get_song_hash() {
    return get_diff_hash(0);
}
