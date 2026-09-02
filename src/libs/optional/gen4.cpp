#include "gen4.h"

#include <spdlog/spdlog.h>
#include <rapidjson/document.h>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <mutex>

#include "../miniz/miniz.h"
#include "../md5.h"


namespace gen4 {

// ---------------------------------------------------------------- SHA-256

namespace {

const uint32_t SHA_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

void sha256_block(uint32_t state[8], const uint8_t* p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
               ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + ch + SHA_K[i] + w[i];
        uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

std::string to_hex_upper(const std::vector<uint8_t>& bytes) {
    static const char* digits = "0123456789ABCDEF";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 0x0F]);
    }
    return out;
}

// ------------------------------------------------------------------- AES

const uint8_t AES_SBOX[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16,
};

uint8_t AES_INV_SBOX[256];

void build_inv_sbox() {
    static bool built = false;
    if (built) return;
    for (int i = 0; i < 256; i++) AES_INV_SBOX[AES_SBOX[i]] = (uint8_t)i;
    built = true;
}

inline uint8_t xtime(uint8_t x) { return (uint8_t)((x << 1) ^ ((x >> 7) * 0x1B)); }

uint8_t gmul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) p ^= a;
        a = xtime(a);
        b >>= 1;
    }
    return p;
}

// AES-256: 14 rounds, 60 words of round key.
void expand_key(const uint8_t key[32], uint8_t round_keys[240]) {
    memcpy(round_keys, key, 32);
    uint8_t rcon = 1;
    for (int i = 8; i < 60; i++) {
        uint8_t t[4];
        memcpy(t, round_keys + (i - 1) * 4, 4);
        if (i % 8 == 0) {
            uint8_t tmp = t[0];
            t[0] = (uint8_t)(AES_SBOX[t[1]] ^ rcon);
            t[1] = AES_SBOX[t[2]];
            t[2] = AES_SBOX[t[3]];
            t[3] = AES_SBOX[tmp];
            rcon = xtime(rcon);
        } else if (i % 8 == 4) {
            for (int j = 0; j < 4; j++) t[j] = AES_SBOX[t[j]];
        }
        for (int j = 0; j < 4; j++)
            round_keys[i * 4 + j] = (uint8_t)(round_keys[(i - 8) * 4 + j] ^ t[j]);
    }
}

void add_round_key(uint8_t s[16], const uint8_t* rk) {
    for (int i = 0; i < 16; i++) s[i] ^= rk[i];
}

void inv_shift_rows(uint8_t s[16]) {
    uint8_t t;
    t = s[13]; s[13] = s[9];  s[9]  = s[5];  s[5]  = s[1];  s[1]  = t;
    t = s[2];  s[2]  = s[10]; s[10] = t;     t     = s[6];  s[6]  = s[14]; s[14] = t;
    t = s[3];  s[3]  = s[7];  s[7]  = s[11]; s[11] = s[15]; s[15] = t;
}

void inv_sub_bytes(uint8_t s[16]) {
    for (int i = 0; i < 16; i++) s[i] = AES_INV_SBOX[s[i]];
}

void inv_mix_columns(uint8_t s[16]) {
    for (int c = 0; c < 4; c++) {
        uint8_t* p = s + c * 4;
        uint8_t a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
        p[0] = (uint8_t)(gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9));
        p[1] = (uint8_t)(gmul(a0, 9)  ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13));
        p[2] = (uint8_t)(gmul(a0, 13) ^ gmul(a1, 9)  ^ gmul(a2, 14) ^ gmul(a3, 11));
        p[3] = (uint8_t)(gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9)  ^ gmul(a3, 14));
    }
}

void decrypt_block(const uint8_t round_keys[240], uint8_t block[16]) {
    add_round_key(block, round_keys + 14 * 16);
    for (int round = 13; round >= 1; round--) {
        inv_shift_rows(block);
        inv_sub_bytes(block);
        add_round_key(block, round_keys + round * 16);
        inv_mix_columns(block);
    }
    inv_shift_rows(block);
    inv_sub_bytes(block);
    add_round_key(block, round_keys);
}

}  // namespace

std::vector<uint8_t> sha256(const uint8_t* data, size_t len) {
    uint32_t state[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

    size_t full = len / 64;
    for (size_t i = 0; i < full; i++) sha256_block(state, data + i * 64);

    uint8_t tail[128] = {};
    size_t rest = len - full * 64;
    memcpy(tail, data + full * 64, rest);
    tail[rest] = 0x80;
    size_t tail_len = (rest < 56) ? 64 : 128;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        tail[tail_len - 1 - i] = (uint8_t)(bits >> (i * 8));
    for (size_t i = 0; i < tail_len; i += 64) sha256_block(state, tail + i);

    std::vector<uint8_t> out(32);
    for (int i = 0; i < 8; i++) {
        out[i * 4]     = (uint8_t)(state[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(state[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(state[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(state[i]);
    }
    return out;
}

std::vector<uint8_t> aes256_cbc_decrypt(const std::vector<uint8_t>& key,
                                        const uint8_t* iv,
                                        const uint8_t* data, size_t len) {
    if (key.size() != 32 || len == 0 || len % 16 != 0) return {};
    build_inv_sbox();

    uint8_t round_keys[240];
    expand_key(key.data(), round_keys);

    std::vector<uint8_t> out(len);
    uint8_t prev[16];
    memcpy(prev, iv, 16);

    for (size_t off = 0; off < len; off += 16) {
        uint8_t block[16], cipher[16];
        memcpy(cipher, data + off, 16);
        memcpy(block, cipher, 16);
        decrypt_block(round_keys, block);
        for (int i = 0; i < 16; i++) out[off + i] = (uint8_t)(block[i] ^ prev[i]);
        memcpy(prev, cipher, 16);
    }

    // PKCS#7, unpadded by hand so that a wrong key is a clear failure rather
    // than a buffer of noise handed on to the inflater.
    uint8_t pad = out.back();
    if (pad == 0 || pad > 16 || pad > out.size()) {
        spdlog::warn("gen4: bad PKCS#7 padding ({}), wrong key?", (int)pad);
        return {};
    }
    for (size_t i = out.size() - pad; i < out.size(); i++) {
        if (out[i] != pad) {
            spdlog::warn("gen4: PKCS#7 padding not uniform, wrong key?");
            return {};
        }
    }
    out.resize(out.size() - pad);
    return out;
}

std::vector<uint8_t> gzip_inflate(const uint8_t* data, size_t len) {
    // Both the tables and the charts are gzip, not bare zlib, so the member
    // header is stepped over and the deflate stream fed in raw.
    if (len < 18 || data[0] != 0x1F || data[1] != 0x8B) {
        spdlog::warn("gen4: not a gzip stream");
        return {};
    }

    size_t pos = 10;
    uint8_t flags = data[3];
    if (flags & 0x04) {                       // FEXTRA
        if (pos + 2 > len) return {};
        size_t extra = (size_t)data[pos] | ((size_t)data[pos + 1] << 8);
        pos += 2 + extra;
    }
    if (flags & 0x08) while (pos < len && data[pos++]) {}   // FNAME
    if (flags & 0x10) while (pos < len && data[pos++]) {}   // FCOMMENT
    if (flags & 0x02) pos += 2;                             // FHCRC
    if (pos >= len) return {};

    // The gzip trailer carries the uncompressed size, which saves growing the
    // output buffer blindly.
    uint32_t isize = (uint32_t)data[len - 4] | ((uint32_t)data[len - 3] << 8) |
                     ((uint32_t)data[len - 2] << 16) | ((uint32_t)data[len - 1] << 24);

    std::vector<uint8_t> out(isize ? isize : (len * 4));
    mz_stream stream = {};
    stream.next_in   = data + pos;
    stream.avail_in  = (unsigned int)(len - pos - 8);
    stream.next_out  = out.data();
    stream.avail_out = (unsigned int)out.size();

    if (mz_inflateInit2(&stream, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK) {
        spdlog::warn("gen4: inflate init failed");
        return {};
    }
    int status = mz_inflate(&stream, MZ_FINISH);
    mz_inflateEnd(&stream);
    if (status != MZ_STREAM_END && status != MZ_OK) {
        spdlog::warn("gen4: inflate failed ({})", status);
        return {};
    }
    out.resize(stream.total_out);
    return out;
}

std::vector<uint8_t> derive_key(const std::string& seed) {
    std::vector<uint8_t> buf(seed.begin(), seed.end());
    for (uint8_t& b : buf) b ^= 0x01;

    std::string hex = to_hex_upper(sha256(buf.data(), buf.size()));
    for (int i = 0; i < 1000; i++)
        hex = to_hex_upper(sha256((const uint8_t*)hex.data(), hex.size()));

    uint32_t digest_words[4];
    md5((const uint8_t*)hex.data(), hex.size(), digest_words);

    std::vector<uint8_t> digest(16);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            digest[i * 4 + j] = (uint8_t)(digest_words[i] >> (j * 8));

    std::string key_text = to_hex_upper(digest);
    return std::vector<uint8_t>(key_text.begin(), key_text.end());
}

bool looks_encrypted(const std::vector<uint8_t>& raw) {
    return raw.size() >= 32 && raw.size() % 16 == 0;
}

// ------------------------------------------------------------- Library

namespace {

// The suffix each difficulty's chart file carries, in the game's difficulty
// order. Ura is a separate file rather than a flag on oni.
const char* DIFF_SUFFIX[5] = { "_e", "_n", "_h", "_m", "_x" };

// wordlist column -> the language codes this game uses.
const struct { const char* field; const char* lang; } WORD_LANGS[] = {
    { "japaneseText",  "ja"    },
    { "englishUsText", "en"    },
    { "chineseTText",  "zh-tw" },
    { "koreanText",    "ko"    },
    { "chineseSText",  "zh-cn" },
};

std::string json_string(const rapidjson::Value& v, const char* name) {
    auto it = v.FindMember(name);
    if (it == v.MemberEnd() || !it->value.IsString()) return "";
    return it->value.GetString();
}

int json_int(const rapidjson::Value& v, const char* name) {
    auto it = v.FindMember(name);
    if (it == v.MemberEnd() || !it->value.IsInt()) return 0;
    return it->value.GetInt();
}

bool json_bool(const rapidjson::Value& v, const char* name) {
    auto it = v.FindMember(name);
    if (it == v.MemberEnd() || !it->value.IsBool()) return false;
    return it->value.GetBool();
}

}  // namespace

bool Library::load(const fs::path& root) {
    is_loaded = false;
    entries.clear();
    data_root = root;

    std::vector<uint8_t> table_key = derive_key(DATATABLE_SEED);
    fumen_key = derive_key(FUMEN_SEED);

    std::vector<uint8_t> music = load_encrypted(root / "datatable" / "musicinfo.bin", table_key);
    if (music.empty()) {
        spdlog::warn("gen4: no readable musicinfo under {}", root.string());
        return false;
    }

    rapidjson::Document doc;
    doc.Parse(reinterpret_cast<const char*>(music.data()), music.size());
    if (doc.HasParseError() || !doc.HasMember("items") || !doc["items"].IsArray()) {
        spdlog::warn("gen4: musicinfo is not the expected JSON");
        return false;
    }

    // Difficulty names as musicinfo spells them, in difficulty order.
    static const char* STAR_FIELDS[5]   = { "starEasy", "starNormal", "starHard", "starMania", "starUra" };
    static const char* BRANCH_FIELDS[5] = { "branchEasy", "branchNormal", "branchHard", "branchMania", "branchUra" };

    for (const auto& item : doc["items"].GetArray()) {
        if (!item.IsObject()) continue;
        SongEntry e;
        e.id         = json_string(item, "id");
        if (e.id.empty()) continue;
        e.unique_id  = json_int(item, "uniqueId");
        e.genre_no   = json_int(item, "genreNo");
        e.sound_file = json_string(item, "songFileName");
        for (int d = 0; d < 5; d++) {
            e.stars[d]  = json_int(item, STAR_FIELDS[d]);
            e.branch[d] = json_bool(item, BRANCH_FIELDS[d]);
        }
        entries.push_back(std::move(e));
    }

    // Titles live in a separate table, keyed by the song id.
    std::vector<uint8_t> words = load_encrypted(root / "datatable" / "wordlist.bin", table_key);
    if (!words.empty()) {
        rapidjson::Document wdoc;
        wdoc.Parse(reinterpret_cast<const char*>(words.data()), words.size());
        if (!wdoc.HasParseError() && wdoc.HasMember("items") && wdoc["items"].IsArray()) {
            std::map<std::string, const rapidjson::Value*> by_key;
            for (const auto& item : wdoc["items"].GetArray()) {
                if (!item.IsObject()) continue;
                std::string key = json_string(item, "key");
                if (!key.empty()) by_key[key] = &item;
            }
            for (SongEntry& e : entries) {
                auto title = by_key.find("song_" + e.id);
                auto sub   = by_key.find("song_sub_" + e.id);
                for (const auto& col : WORD_LANGS) {
                    if (title != by_key.end())
                        e.title[col.lang] = json_string(*title->second, col.field);
                    if (sub != by_key.end())
                        e.subtitle[col.lang] = json_string(*sub->second, col.field);
                }
            }
        } else {
            spdlog::warn("gen4: wordlist is not the expected JSON, songs will show their ids");
        }
    }

    // The running order decides what each genre holds and in what order.
    std::vector<uint8_t> order = load_encrypted(root / "datatable" / "music_order.bin", table_key);
    if (!order.empty()) {
        rapidjson::Document odoc;
        odoc.Parse(reinterpret_cast<const char*>(order.data()), order.size());
        if (!odoc.HasParseError() && odoc.HasMember("items") && odoc["items"].IsArray()) {
            for (const auto& item : odoc["items"].GetArray()) {
                if (!item.IsObject()) continue;
                OrderEntry e;
                e.id       = json_string(item, "id");
                e.genre_no = json_int(item, "genreNo");
                if (!e.id.empty()) order_entries.push_back(std::move(e));
            }
        }
    }
    if (order_entries.empty()) {
        // No running order to go by: fall back to the genre on each song, in
        // the order the song table lists them.
        spdlog::warn("gen4: no music order, falling back to each song's own genre");
        for (const SongEntry& e : entries)
            order_entries.push_back({ e.genre_no, e.id });
    }

    is_loaded = true;
    spdlog::info("gen4: {} songs, {} listings from {}",
                 entries.size(), order_entries.size(), root.string());
    return true;
}

const SongEntry* Library::find(const std::string& id) const {
    for (const SongEntry& e : entries)
        if (e.id == id) return &e;
    return nullptr;
}

bool Library::has_difficulty(const std::string& id, int difficulty) const {
    if (difficulty < 0 || difficulty > 4) return false;
    std::error_code ec;
    return fs::exists(data_root / "fumen" / id / (id + DIFF_SUFFIX[difficulty] + ".bin"), ec);
}

std::vector<uint8_t> Library::load_chart(const std::string& id, int difficulty) const {
    if (difficulty < 0 || difficulty > 4) return {};
    fs::path path = data_root / "fumen" / id / (id + DIFF_SUFFIX[difficulty] + ".bin");
    std::error_code ec;
    if (!fs::exists(path, ec)) return {};

    std::vector<uint8_t> plain = load_encrypted(path, fumen_key);
    if (!plain.empty()) return plain;

    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
}

int genre_index_for(int genre_no) {
    static const int TO_GENRE_INDEX[8] = {
        1,  // pops     -> JPOP
        2,  // anime    -> ANIME
        4,  // kids     -> CHILDREN
        3,  // vocaloid -> VOCALOID
        7,  // game     -> GAME
        8,  // namco    -> NAMCO
        5,  // variety  -> VARIETY
        6,  // classic  -> CLASSICAL
    };
    if (genre_no < 0 || genre_no > 7) return 8;
    return TO_GENRE_INDEX[genre_no];
}

std::string genre_name(int genre_no, const std::string& language) {
    static const char* NAMES[8][3] = {
        // ja                      en                  zh
        { "ポップス",              "J-POP",            "流行" },
        { "アニメ",                "Anime",            "動畫" },
        { "キッズ",                "Kids",             "兒童" },
        { "ボーカロイド",          "Vocaloid",         "VOCALOID" },
        { "ゲームミュージック",    "Game Music",       "遊戲音樂" },
        { "ナムコオリジナル",      "Namco Original",   "南夢宮原創" },
        { "バラエティ",            "Variety",          "綜合" },
        { "クラシック",            "Classical",        "古典" },
    };
    if (genre_no < 0 || genre_no > 7) return "";
    int column = 0;
    if (language.rfind("en", 0) == 0)      column = 1;
    else if (language.rfind("zh", 0) == 0) column = 2;
    return NAMES[genre_no][column];
}

std::vector<int> genres_present(const Library& library) {
    std::vector<int> found;
    for (const OrderEntry& e : library.order()) {
        if (std::find(found.begin(), found.end(), e.genre_no) == found.end())
            found.push_back(e.genre_no);
    }
    std::sort(found.begin(), found.end());
    return found;
}

fs::path genre_path(const fs::path& data_root, int genre_no) {
    return data_root / ("@genre_" + std::to_string(genre_no));
}

int genre_of_path(const fs::path& path) {
    std::string name = path.filename().string();
    if (name.rfind("@genre_", 0) != 0) return -1;
    try {
        return std::stoi(name.substr(7));
    } catch (...) {
        return -1;
    }
}

static bool is_root_dir(const fs::path& dir) {
    static std::mutex               mutex;
    static std::map<fs::path, bool> cache;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = cache.find(dir);
        if (it != cache.end()) return it->second;
    }
    std::error_code ec;
    bool ok = fs::exists(dir / "datatable" / "musicinfo.bin", ec) &&
              fs::is_directory(dir / "fumen", ec);
    std::lock_guard<std::mutex> lock(mutex);
    cache[dir] = ok;
    return ok;
}

fs::path find_data_root(const fs::path& path) {
    for (fs::path dir = path; !dir.empty(); dir = dir.parent_path()) {
        if (is_root_dir(dir)) return dir;
        if (dir == dir.parent_path()) break;
    }
    return {};
}

const Library* library_for(const fs::path& path) {
    fs::path root = find_data_root(path);
    if (root.empty()) return nullptr;

    static std::map<std::string, Library> cache;
    static std::mutex cache_mutex;

    std::lock_guard<std::mutex> lock(cache_mutex);
    auto it = cache.find(root.string());
    if (it == cache.end()) {
        Library lib;
        lib.load(root);
        it = cache.emplace(root.string(), std::move(lib)).first;
    }
    return it->second.loaded() ? &it->second : nullptr;
}

std::vector<uint8_t> load_encrypted(const fs::path& path, const std::vector<uint8_t>& key) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        spdlog::warn("gen4: cannot open {}", path.string());
        return {};
    }
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
    if (!looks_encrypted(raw)) {
        spdlog::warn("gen4: {} is not a whole number of blocks", path.string());
        return {};
    }

    // The IV is the first block of the file, the rest is the ciphertext.
    std::vector<uint8_t> plain =
        aes256_cbc_decrypt(key, raw.data(), raw.data() + 16, raw.size() - 16);
    if (plain.empty()) return {};

    return gzip_inflate(plain.data(), plain.size());
}

}  // namespace gen4
