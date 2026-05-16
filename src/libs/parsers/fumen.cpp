#include "fumen.h"
#include <fstream>

#pragma pack(push, 1)

struct FumenJudgeTiming { float good, ok, bad; };

struct FumenHeader {
    FumenJudgeTiming judge_timings[36];
    uint32_t has_branches;
    uint32_t max_hp;
    uint32_t clear_hp;
    int32_t  hp_per_good;
    int32_t  hp_per_ok;
    int32_t  hp_per_bad;
    uint32_t max_combo;
    uint32_t hp_increase_ratio;
    uint32_t hp_increase_ratio_master;
    uint32_t good_diverge_points;
    uint32_t ok_diverge_points;
    uint32_t bad_diverge_points;
    uint32_t drumroll_diverge_points;
    uint32_t good_diverge_points_big;
    uint32_t ok_diverge_points_big;
    uint32_t drumroll_diverge_points_big;
    uint32_t balloon_diverge_points;
    uint32_t kusudama_diverge_points;
    uint32_t number_of_diverge_points;
    uint32_t max_score_value;
    uint32_t number_of_measures;
    uint32_t unknown1;
};

struct FumenMeasureData {
    float    bpm;
    float    measure_offset;
    uint8_t  is_gogo_time;
    uint8_t  is_bar_line_visible;
    uint16_t padding1;
    uint32_t in_normal_to_advanced;
    uint32_t in_normal_to_master;
    uint32_t in_advanced_to_master;
    uint32_t in_advanced_keep_advanced;
    uint32_t in_master_to_advanced;
    uint32_t in_master_keep_master;
    uint32_t padding2;
};

struct FumenNoteBase {
    uint32_t type;
    float    note_offset;
    uint32_t padding;
    uint16_t initial_score_value;
    uint16_t score_diff_times4;
    uint32_t unknown2;
    float    length;
};

#pragma pack(pop)

static_assert(sizeof(FumenHeader)      == 520, "FumenHeader size mismatch");
static_assert(sizeof(FumenMeasureData) ==  40, "FumenMeasureData size mismatch");
static_assert(sizeof(FumenNoteBase)    ==  24, "FumenNoteBase size mismatch");

// Fumen V2 note type values
static constexpr uint32_t FUMEN_DON        = 1;
static constexpr uint32_t FUMEN_DO         = 2;
static constexpr uint32_t FUMEN_KO         = 3;
static constexpr uint32_t FUMEN_KATSU      = 4;
static constexpr uint32_t FUMEN_KA         = 5;
static constexpr uint32_t FUMEN_RENDA      = 6;
static constexpr uint32_t FUMEN_BIG_DON    = 7;
static constexpr uint32_t FUMEN_BIG_KATSU  = 8;
static constexpr uint32_t FUMEN_BIG_RENDA  = 9;
static constexpr uint32_t FUMEN_BALLOON    = 10;
static constexpr uint32_t FUMEN_KUSUDAMA   = 12;

static NoteType map_note_type(uint32_t t) {
    switch (t) {
        case FUMEN_DON:
        case FUMEN_DO:        return NoteType::DON;
        case FUMEN_KO:
        case FUMEN_KATSU:
        case FUMEN_KA:        return NoteType::KAT;
        case FUMEN_RENDA:     return NoteType::ROLL_HEAD;
        case FUMEN_BIG_DON:   return NoteType::DON_L;
        case FUMEN_BIG_KATSU: return NoteType::KAT_L;
        case FUMEN_BIG_RENDA: return NoteType::ROLL_HEAD_L;
        case FUMEN_BALLOON:   return NoteType::BALLOON_HEAD;
        case FUMEN_KUSUDAMA:  return NoteType::KUSUDAMA;
        default:              return NoteType::DON;
    }
}

static bool is_roll(uint32_t t)    { return t == FUMEN_RENDA || t == FUMEN_BIG_RENDA; }
static bool is_balloon(uint32_t t) { return t == FUMEN_BALLOON || t == FUMEN_KUSUDAMA; }
static bool has_renda_padding(uint32_t t) { return t == FUMEN_RENDA || t == FUMEN_BIG_RENDA; }

FumenParser::FumenParser(const fs::path& path) : file_path(path) {
    metadata             = TJAMetadata();
    ex_data              = TJAEXData();
    metadata.title["en"] = path.stem().string();
    metadata.course_data[0] = CourseData{};
}

void FumenParser::build_notes() {
    if (parsed) return;
    parsed = true;

    std::ifstream f(file_path, std::ios::binary);
    if (!f) {
        spdlog::warn("FumenParser: cannot open {}", file_path.string());
        return;
    }

    FumenHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(FumenHeader));
    if (!f || hdr.number_of_measures == 0 || hdr.number_of_measures > 100000) {
        spdlog::warn("FumenParser: invalid header in {}", file_path.string());
        return;
    }

    double prev_bpm  = -1.0;
    bool   prev_gogo = false;
    int    idx       = 0;

    for (uint32_t m = 0; m < hdr.number_of_measures; m++) {
        FumenMeasureData mdata{};
        f.read(reinterpret_cast<char*>(&mdata), sizeof(FumenMeasureData));
        if (!f) break;

        double measure_ms = static_cast<double>(mdata.measure_offset);
        double bpm        = static_cast<double>(mdata.bpm);
        bool   gogo       = mdata.is_gogo_time != 0;
        bool   show_bar   = mdata.is_bar_line_visible != 0;

        if (m == 0) metadata.bpm = bpm;

        if (bpm != prev_bpm && bpm > 0.0) {
            TimelineObject tl;
            tl.start_time = measure_ms;
            tl.end_time   = measure_ms;
            tl.bpm        = bpm;
            cached_notes.timeline.push_back(tl);
            prev_bpm = bpm;
        }

        if (gogo != prev_gogo) {
            TimelineObject tl;
            tl.start_time = measure_ms;
            tl.end_time   = measure_ms;
            tl.gogo_time  = gogo;
            cached_notes.timeline.push_back(tl);
            prev_gogo = gogo;
        }

        float normal_scroll = 1.0f;
        for (int b = 0; b < 3; b++) {
            uint16_t note_count  = 0;
            uint16_t branch_unk  = 0;
            float    scroll      = 1.0f;

            f.read(reinterpret_cast<char*>(&note_count), sizeof(note_count));
            f.read(reinterpret_cast<char*>(&branch_unk), sizeof(branch_unk));
            f.read(reinterpret_cast<char*>(&scroll),     sizeof(scroll));
            if (!f) break;

            if (b == 0) normal_scroll = scroll;

            for (uint16_t n = 0; n < note_count; n++) {
                FumenNoteBase nb{};
                f.read(reinterpret_cast<char*>(&nb), sizeof(FumenNoteBase));
                if (!f) break;

                if (has_renda_padding(nb.type)) {
                    uint32_t extra[2]{};
                    f.read(reinterpret_cast<char*>(extra), 8);
                }

                if (b != 0) continue;

                double hit_ms = static_cast<double>(mdata.measure_offset)
                                + static_cast<double>(nb.note_offset);
                NoteType nt = map_note_type(nb.type);

                Note note;
                note.type     = nt;
                note.hit_ms   = hit_ms;
                note.load_ms  = hit_ms;
                note.unload_ms = hit_ms;
                note.bpm      = bpm;
                note.scroll_x = static_cast<double>(scroll);
                note.scroll_y = 0.0;
                note.display  = true;
                note.index    = idx++;

                if (is_roll(nb.type)) {
                    note.color = 255;
                    cached_notes.notes.push_back(note);

                    Note tail;
                    tail.type     = NoteType::TAIL;
                    tail.hit_ms   = hit_ms + static_cast<double>(nb.length);
                    tail.load_ms  = tail.hit_ms;
                    tail.unload_ms = tail.hit_ms;
                    tail.bpm      = bpm;
                    tail.scroll_x = static_cast<double>(scroll);
                    tail.scroll_y = 0.0;
                    tail.display  = true;
                    tail.index    = idx++;
                    cached_notes.notes.push_back(tail);

                } else if (is_balloon(nb.type)) {
                    note.count = 20;
                    cached_notes.notes.push_back(note);

                    Note tail;
                    tail.type     = NoteType::TAIL;
                    tail.hit_ms   = hit_ms + static_cast<double>(nb.length);
                    tail.load_ms  = tail.hit_ms;
                    tail.unload_ms = tail.hit_ms;
                    tail.bpm      = bpm;
                    tail.scroll_x = static_cast<double>(scroll);
                    tail.scroll_y = 0.0;
                    tail.display  = true;
                    tail.index    = idx++;
                    cached_notes.notes.push_back(tail);

                } else {
                    cached_notes.notes.push_back(note);
                }
            }

            if (b == 0) {
                Note barline;
                barline.type     = NoteType::BARLINE;
                barline.hit_ms   = measure_ms;
                barline.load_ms  = measure_ms;
                barline.unload_ms = measure_ms;
                barline.bpm      = bpm;
                barline.scroll_x = static_cast<double>(normal_scroll);
                barline.scroll_y = 0.0;
                barline.display  = show_bar;
                barline.index    = idx++;
                cached_notes.notes.push_back(barline);
            }
        }
    }

    modifier_moji(cached_notes);
}

std::tuple<NoteList, std::deque<NoteList>, std::deque<NoteList>, std::deque<NoteList>>
FumenParser::notes_to_position(int /*diff*/) {
    build_notes();
    return {cached_notes, {}, {}, {}};
}

std::string FumenParser::get_diff_hash(int /*difficulty*/) {
    build_notes();
    if (cached_notes.notes.empty()) return "";
    digestpp::md5 hasher;
    for (const Note& n : cached_notes.notes) {
        auto h = n.hash();
        hasher.absorb(reinterpret_cast<const char*>(&h), sizeof(h));
    }
    return hasher.hexdigest();
}

std::string FumenParser::get_song_hash() {
    return get_diff_hash(0);
}
