#include "fumen.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include "../math/md5.h"

#pragma pack(push, 1)

struct FumenJudgeTiming {
    float good, ok, bad;
};

struct FumenHeader {
    FumenJudgeTiming judge_timings[36];
    uint32_t has_branches;
    uint32_t max_hp;
    uint32_t clear_hp;
    int32_t hp_per_good;
    int32_t hp_per_ok;
    int32_t hp_per_bad;
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
    float bpm;
    float measure_offset;
    uint8_t is_gogo_time;
    uint8_t is_bar_line_visible;
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
    float note_offset;
    uint32_t padding;
    uint16_t initial_score_value;
    uint16_t score_diff_times4;
    uint32_t unknown2;
    float length;
};

#pragma pack(pop)

static_assert(sizeof(FumenHeader) == 520, "FumenHeader size mismatch");
static_assert(sizeof(FumenMeasureData) == 40, "FumenMeasureData size mismatch");
static_assert(sizeof(FumenNoteBase) == 24, "FumenNoteBase size mismatch");

// Fumen V2 note type values
static constexpr uint32_t FUMEN_DON = 1;
static constexpr uint32_t FUMEN_DO = 2;
static constexpr uint32_t FUMEN_KO = 3;
static constexpr uint32_t FUMEN_KATSU = 4;
static constexpr uint32_t FUMEN_KA = 5;
static constexpr uint32_t FUMEN_RENDA = 6;
static constexpr uint32_t FUMEN_BIG_DON = 7;
static constexpr uint32_t FUMEN_BIG_KATSU = 8;
static constexpr uint32_t FUMEN_BIG_RENDA = 9;
static constexpr uint32_t FUMEN_BALLOON = 10;
static constexpr uint32_t FUMEN_KUSUDAMA = 12;

static std::optional<NoteType> map_note_type(uint32_t t) {
    switch (t) {
    case FUMEN_DON:
    case FUMEN_DO:
        return NoteType::DON;
    case FUMEN_KO:
    case FUMEN_KATSU:
    case FUMEN_KA:
        return NoteType::KAT;
    case FUMEN_RENDA:
        return NoteType::ROLL_HEAD;
    case FUMEN_BIG_DON:
        return NoteType::DON_L;
    case FUMEN_BIG_KATSU:
        return NoteType::KAT_L;
    case FUMEN_BIG_RENDA:
        return NoteType::ROLL_HEAD_L;
    case FUMEN_BALLOON:
        return NoteType::BALLOON_HEAD;
    case FUMEN_KUSUDAMA:
        return NoteType::KUSUDAMA;
    default:
        return std::nullopt;
    }
}

static bool is_roll(uint32_t t) {
    return t == FUMEN_RENDA || t == FUMEN_BIG_RENDA;
}
static bool is_balloon(uint32_t t) {
    return t == FUMEN_BALLOON || t == FUMEN_KUSUDAMA;
}
static bool has_renda_padding(uint32_t t) {
    return t == FUMEN_RENDA || t == FUMEN_BIG_RENDA;
}

static uint32_t bswap32(uint32_t v) {
    return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24);
}
static void swap32(void* p, size_t words) {
    uint8_t* b = static_cast<uint8_t*>(p);
    for (size_t i = 0; i < words; i++) {
        uint32_t v;
        memcpy(&v, b + i * 4, 4);
        v = bswap32(v);
        memcpy(b + i * 4, &v, 4);
    }
}
static void swap16(void* p) {
    uint16_t* h = static_cast<uint16_t*>(p);
    *h = static_cast<uint16_t>((*h >> 8) | (*h << 8));
}

// Counts how many independent header fields look plausible when the buffer
// is interpreted with the given endianness, to break ties instead of relying
// on a single hard-coded float threshold.
static int score_endianness(const std::vector<uint8_t>& data, bool as_be) {
    auto rd_u32 = [&](size_t off) {
        uint32_t v;
        memcpy(&v, data.data() + off, 4);
        return as_be ? bswap32(v) : v;
    };
    auto rd_f32 = [&](size_t off) {
        uint32_t v = rd_u32(off);
        float f;
        memcpy(&f, &v, 4);
        return f;
    };
    int score = 0;
    if (rd_u32(offsetof(FumenHeader, max_hp)) <= 10000000u) {
        score++;
    }
    if (rd_u32(offsetof(FumenHeader, max_score_value)) <= 10000000u) {
        score++;
    }
    for (int i = 0; i < 3; i++) {
        float f = rd_f32(offsetof(FumenHeader, judge_timings) + i * sizeof(FumenJudgeTiming));
        if (std::isfinite(f) && f > 0.1f && f < 10000.0f) {
            score++;
        }
    }
    return score;
}

static bool chart_is_big_endian(const std::vector<uint8_t>& data) {
    if (data.size() < 520) {
        return false;
    }
    uint32_t le;
    memcpy(&le, data.data() + offsetof(FumenHeader, number_of_measures), 4);
    uint32_t be = bswap32(le);
    bool le_ok = le > 0 && le <= 100000;
    bool be_ok = be > 0 && be <= 100000;
    if (le_ok != be_ok) {
        return be_ok;
    }

    int le_score = score_endianness(data, false);
    int be_score = score_endianness(data, true);
    if (le_score != be_score) {
        return be_score > le_score;
    }

    float f;
    memcpy(&f, data.data(), 4);
    return !(f > 0.1f && f < 10000.0f);
}

static void swap_header(FumenHeader& h) {
    swap32(&h, sizeof(FumenHeader) / 4); // floats and u32/s32 all round-trip
}
static void swap_measure(FumenMeasureData& m) {
    swap32(&m.bpm, 2);
    swap16(&m.padding1);
    swap32(&m.in_normal_to_advanced, 7);
}
static void swap_note(FumenNoteBase& n) {
    swap32(&n.type, 3);
    swap16(&n.initial_score_value);
    swap16(&n.score_diff_times4);
    swap32(&n.unknown2, 1);
    swap32(&n.length, 1);
}

FumenParser::FumenParser(const fs::path& path, int start_delay)
    : file_path(path), start_delay(static_cast<double>(start_delay)) {
    metadata = TJAMetadata();
    ex_data = TJAEXData();

    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        song_id = path.filename().string();
        library = gen4::library_for(path);
    }

    const gen4::SongEntry* entry = library ? library->find(song_id) : nullptr;
    if (!entry && !song_id.empty()) {
        gen3_library = gen3::library_for(file_path);
        const gen3::SongEntry* ge = gen3_library ? gen3_library->find(song_id) : nullptr;
        if (ge) {
            // The XML is Japanese-only, so the title is the title everywhere.
            metadata.title["ja"] = ge->title;
            metadata.title["en"] = ge->title;
            for (int d = 0; d < 5; d++) {
                if (!gen3_library->has_difficulty(song_id, d)) {
                    continue;
                }
                CourseData course;
                course.level = ge->stars[d];
                metadata.course_data[d] = course;
            }
            metadata.wave = gen3_library->sound_path(song_id);
            return;
        }
        gen3_library = nullptr;
    }
    if (!entry) {
        metadata.title["en"] = path.stem().string();
        metadata.course_data[0] = CourseData{};
        return;
    }

    for (const auto& [lang, text] : entry->title) {
        if (!text.empty()) {
            metadata.title[lang] = text;
        }
    }
    for (const auto& [lang, text] : entry->subtitle) {
        if (!text.empty()) {
            metadata.subtitle[lang] = text;
        }
    }
    if (!metadata.title.contains("en")) {
        metadata.title["en"] = song_id;
    }

    for (int d = 0; d < 5; d++) {
        if (!library->has_difficulty(song_id, d)) {
            continue;
        }
        CourseData course;
        course.level = entry->stars[d];
        course.is_branching = entry->branch[d];
        metadata.course_data[d] = course;
    }

    if (!entry->sound_file.empty()) {
        metadata.wave = library->root() / (entry->sound_file + ".nus3bank");
    }
}

std::vector<uint8_t> FumenParser::read_chart(int diff) {
    if (library) {
        return library->load_chart(song_id, diff);
    }

    if (gen3_library) {
        std::ifstream f(gen3_library->chart_path(song_id, diff), std::ios::binary);
        if (!f) {
            return {};
        }
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }

    static const std::vector<uint8_t> key = gen4::derive_key(FUMEN_SEED);
    std::vector<uint8_t> plain = gen4::load_encrypted(file_path, key);
    if (!plain.empty()) {
        return plain;
    }

    std::ifstream f(file_path, std::ios::binary);
    if (!f) {
        spdlog::warn("FumenParser: cannot open {}", file_path.string());
        return {};
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void FumenParser::build_notes(int diff) {
    if (cached_diff == diff) {
        return;
    }
    cached_diff = -1;
    cached_notes = NoteList();

    std::vector<uint8_t> data = read_chart(diff);
    size_t pos = 0;
    auto take = [&](void* dst, size_t n) {
        if (pos + n > data.size()) {
            return false;
        }
        memcpy(dst, data.data() + pos, n);
        pos += n;
        return true;
    };

    bool big_endian = chart_is_big_endian(data);

    FumenHeader hdr{};
    if (!take(&hdr, sizeof(FumenHeader))) {
        spdlog::warn("FumenParser: no readable chart in {} difficulty {}", file_path.string(), diff);
        return;
    }
    if (big_endian) {
        swap_header(hdr);
    }
    if (hdr.number_of_measures == 0 || hdr.number_of_measures > 100000) {
        spdlog::warn("FumenParser: no readable chart in {} difficulty {}", file_path.string(), diff);
        return;
    }

    double prev_bpm = -1.0;
    bool prev_gogo = false;
    int idx = 0;
    bool truncated = false;

    for (uint32_t m = 0; m < hdr.number_of_measures; m++) {
        FumenMeasureData mdata{};
        if (!take(&mdata, sizeof(FumenMeasureData))) {
            truncated = true;
            break;
        }
        if (big_endian) {
            swap_measure(mdata);
        }

        double bpm = static_cast<double>(mdata.bpm);

        double lead_in = bpm > 0.0 ? 60000.0 / bpm * 4.0 : 0.0;
        double measure_ms = static_cast<double>(mdata.measure_offset) + lead_in + start_delay;
        bool gogo = mdata.is_gogo_time != 0;
        bool show_bar = mdata.is_bar_line_visible != 0;

        if (m == 0) {
            metadata.bpm = bpm;
        }

        if (bpm != prev_bpm && bpm > 0.0) {
            TimelineObject tl;
            tl.start_time = measure_ms;
            tl.end_time = measure_ms;
            tl.bpm = bpm;
            cached_notes.timeline.push_back(tl);
            prev_bpm = bpm;
        }

        if (gogo != prev_gogo) {
            TimelineObject tl;
            tl.start_time = measure_ms;
            tl.end_time = measure_ms;
            tl.gogo_time = gogo;
            cached_notes.timeline.push_back(tl);
            prev_gogo = gogo;
        }

        float normal_scroll = 1.0f;
        for (int b = 0; b < 3; b++) {
            uint16_t note_count = 0;
            uint16_t branch_unk = 0;
            float scroll = 1.0f;

            if (!take(&note_count, sizeof(note_count)) || !take(&branch_unk, sizeof(branch_unk)) ||
                !take(&scroll, sizeof(scroll))) {
                truncated = true;
                break;
            }
            if (big_endian) {
                swap16(&note_count);
                swap16(&branch_unk);
                swap32(&scroll, 1);
            }

            if (b == 0) {
                normal_scroll = scroll;
            }

            for (uint16_t n = 0; n < note_count; n++) {
                FumenNoteBase nb{};
                if (!take(&nb, sizeof(FumenNoteBase))) {
                    truncated = true;
                    break;
                }
                if (big_endian) {
                    swap_note(nb);
                }

                if (has_renda_padding(nb.type)) {
                    uint32_t extra[2]{};
                    if (!take(extra, 8)) {
                        truncated = true;
                        break;
                    }
                }

                if (b != 0) {
                    continue;
                }

                auto nt_opt = map_note_type(nb.type);
                if (!nt_opt) {
                    continue; // unknown/invalid note type: skip rather than emit a spurious DON
                }
                NoteType nt = *nt_opt;

                double hit_ms = measure_ms + static_cast<double>(nb.note_offset);
                double note_length =
                    std::isfinite(nb.length) && nb.length >= 0.0f ? static_cast<double>(nb.length) : 0.0;

                Note note;
                note.type = nt;
                note.hit_ms = hit_ms;
                note.load_ms = hit_ms;
                note.unload_ms = hit_ms;
                note.bpm = bpm;
                note.scroll_x = static_cast<double>(scroll);
                note.scroll_y = 0.0;
                note.display = true;
                note.index = idx++;

                if (is_roll(nb.type)) {
                    note.color = 255;
                    cached_notes.notes.push_back(note);

                    Note tail;
                    tail.type = NoteType::TAIL;
                    tail.hit_ms = hit_ms + note_length;
                    tail.load_ms = tail.hit_ms;
                    tail.unload_ms = tail.hit_ms;
                    tail.bpm = bpm;
                    tail.scroll_x = static_cast<double>(scroll);
                    tail.scroll_y = 0.0;
                    tail.display = true;
                    tail.index = idx++;
                    cached_notes.notes.push_back(tail);

                } else if (is_balloon(nb.type)) {
                    int hits = static_cast<int>(static_cast<uint16_t>(nb.unknown2 & 0xFFFF));
                    note.count = hits > 0 ? hits : 20;
                    cached_notes.notes.push_back(note);

                    Note tail;
                    tail.type = NoteType::TAIL;
                    tail.hit_ms = hit_ms + note_length;
                    tail.load_ms = tail.hit_ms;
                    tail.unload_ms = tail.hit_ms;
                    tail.bpm = bpm;
                    tail.scroll_x = static_cast<double>(scroll);
                    tail.scroll_y = 0.0;
                    tail.display = true;
                    tail.index = idx++;
                    cached_notes.notes.push_back(tail);

                } else {
                    cached_notes.notes.push_back(note);
                }
            }

            if (truncated) {
                break;
            }

            if (b == 0) {
                Note barline;
                barline.type = NoteType::BARLINE;
                barline.hit_ms = measure_ms;
                barline.load_ms = measure_ms;
                barline.unload_ms = measure_ms;
                barline.bpm = bpm;
                barline.scroll_x = static_cast<double>(normal_scroll);
                barline.scroll_y = 0.0;
                barline.display = show_bar;
                barline.index = idx++;
                cached_notes.notes.push_back(barline);
            }
        }

        if (truncated) {
            break;
        }
    }

    if (truncated) {
        spdlog::warn("FumenParser: truncated/corrupt chart in {} difficulty {}", file_path.string(), diff);
    }

    std::ranges::stable_sort(cached_notes.notes, [](const Note& a, const Note& b) { return a.hit_ms < b.hit_ms; });
    for (size_t i = 0; i < cached_notes.notes.size(); i++) {
        cached_notes.notes[i].index = static_cast<int>(i);
    }

    modifier_moji(cached_notes);
    cached_diff = diff;
}

std::tuple<NoteList, std::deque<NoteList>, std::deque<NoteList>, std::deque<NoteList>>
FumenParser::notes_to_position(int diff) {
    build_notes(diff);
    return {cached_notes, {}, {}, {}};
}

std::string FumenParser::get_diff_hash(int difficulty) {
    build_notes(difficulty);
    if (cached_notes.notes.empty()) {
        return "";
    }
    std::vector<unsigned char> buffer;
    for (const Note& n : cached_notes.notes) {
        auto h = n.hash();
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&h);
        buffer.insert(buffer.end(), bytes, bytes + sizeof(h));
    }
    return md5_hexdigest(buffer);
}

std::string FumenParser::get_song_hash() {
    // The oni chart stands in for the song: every song has one, and it is the
    // one difficulty that is never a cut-down arrangement of another.
    return get_diff_hash(3);
}
