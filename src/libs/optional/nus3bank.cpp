#include "nus3bank.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>
#include <fstream>

extern "C" {
#include <g719.h>
}

namespace gen4 {

namespace {

uint32_t read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
uint16_t read_be16(const uint8_t* p) {
    return (uint16_t)(((uint32_t)p[0] << 8) | p[1]);
}
uint32_t read_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

struct Bnsf {
    int    channels      = 0;
    int    sample_rate   = 0;
    int    num_samples   = 0;
    int    block_size    = 0;   // bytes per frame, all channels together
    int    block_samples = 0;   // samples per frame per channel, 960 for G.719
    size_t data_offset   = 0;
    size_t data_size     = 0;
};

int find_preview_ms(const uint8_t* tone, size_t size) {
    if (size < 16) return 0;
    for (size_t i = 4; i + 12 <= size; i += 4) {
        if (read_le32(tone + i) != 0xFFFFFFFFu) continue;
        uint32_t rate = read_le32(tone + i + 4);
        uint32_t ch   = read_le32(tone + i + 8);
        if (rate < 8000 || rate > 192000 || ch < 1 || ch > 8) continue;
        uint32_t preview = read_le32(tone + i - 4);
        if (preview > 30u * 60u * 1000u) continue;   // half an hour: not a time
        return (int)preview;
    }
    return 0;
}

bool find_pack(const std::vector<uint8_t>& file, size_t& pack, size_t& pack_size,
               int& preview_ms) {
    if (file.size() < 0x18 || memcmp(file.data(), "NUS3", 4) != 0) {
        spdlog::warn("gen4 audio: not a NUS3 container");
        return false;
    }
    if (memcmp(file.data() + 8, "BANKTOC ", 8) != 0) {
        spdlog::warn("gen4 audio: no BANKTOC where one is expected");
        return false;
    }

    size_t pos = 0x14 + read_le32(file.data() + 0x10);
    pack = 0; pack_size = 0;
    while (pos + 8 <= file.size()) {
        uint32_t size = read_le32(file.data() + pos + 4);
        if (memcmp(file.data() + pos, "TONE", 4) == 0 && pos + 8 + size <= file.size()) {
            preview_ms = find_preview_ms(file.data() + pos + 8, size);
        }
        if (memcmp(file.data() + pos, "PACK", 4) == 0) {
            pack      = pos + 8;
            pack_size = size;
            break;
        }
        pos += 8 + size;
    }
    if (!pack || pack + pack_size > file.size()) {
        spdlog::warn("gen4 audio: no PACK chunk");
        return false;
    }
    return true;
}

bool parse_container(const std::vector<uint8_t>& file, size_t pack, size_t pack_size,
                     Bnsf& out) {
    const uint8_t* p = file.data() + pack;
    if (pack_size < 0x2C || memcmp(p, "BNSF", 4) != 0) {
        spdlog::warn("gen4 audio: PACK does not start with a BNSF stream");
        return false;
    }
    if (memcmp(p + 8, "IS22", 4) != 0) {
        spdlog::warn("gen4 audio: codec tag is {:.4s}, only IS22 is supported",
                     (const char*)(p + 8));
        return false;
    }
    if (memcmp(p + 0x0C, "sfmt", 4) != 0) {
        spdlog::warn("gen4 audio: no sfmt where one is expected");
        return false;
    }

    const uint8_t* fmt = p + 0x14;
    uint16_t flags     = read_be16(fmt);
    out.channels       = read_be16(fmt + 2);
    out.sample_rate    = (int)read_be32(fmt + 4);
    out.num_samples    = (int)read_be32(fmt + 8);
    out.block_size     = read_be16(fmt + 0x10);
    out.block_samples  = read_be16(fmt + 0x12);

    if (flags != 0) {
        spdlog::warn("gen4 audio: stream is flagged {}, expected plain", flags);
        return false;
    }
    if (out.channels < 1 || out.channels > 2 || out.block_size <= 0) {
        spdlog::warn("gen4 audio: unsupported stream shape ({} ch, block {})",
                     out.channels, out.block_size);
        return false;
    }

    // sdat follows sfmt and holds the codec payload.
    size_t sfmt_size = read_be32(p + 0x10);
    size_t sdat = 0x0C + 4 + 4 + sfmt_size;
    if (pack + sdat + 8 > file.size() || memcmp(p + sdat, "sdat", 4) != 0) {
        spdlog::warn("gen4 audio: no sdat where one is expected");
        return false;
    }
    out.data_size   = read_be32(p + sdat + 4);
    out.data_offset = pack + sdat + 8;
    if (out.data_offset + out.data_size > file.size())
        out.data_size = file.size() - out.data_offset;

    return true;
}

struct IdspChannel {
    int16_t coef[16] = {};
    int16_t hist1 = 0, hist2 = 0;
};

void idsp_decode_frame(const uint8_t* frame, IdspChannel& ch, int16_t* out14) {
    int scale = 1 << (frame[0] & 0x0F);
    int ci    = ((frame[0] >> 4) & 0x07) * 2;
    int c1 = ch.coef[ci], c2 = ch.coef[ci + 1];
    for (int i = 0; i < 14; i++) {
        int nib = frame[1 + i / 2];
        nib = (i & 1) ? (nib & 0x0F) : (nib >> 4);
        if (nib > 7) nib -= 16;
        int sample = (((nib * scale) << 11) + 1024 + c1 * ch.hist1 + c2 * ch.hist2) >> 11;
        if (sample >  32767) sample =  32767;
        if (sample < -32768) sample = -32768;
        ch.hist2 = ch.hist1;
        ch.hist1 = (int16_t)sample;
        out14[i] = ch.hist1;
    }
}

bool decode_idsp(const std::vector<uint8_t>& file, size_t pack, size_t pack_size,
                 DecodedAudio& out) {
    if (pack_size < 0x40) return false;
    const uint8_t* p = file.data() + pack;

    auto be = [&](size_t off) { return read_be32(p + off); };
    int      channels   = (int)be(0x08);
    int      rate       = (int)be(0x0C);
    uint32_t samples    = be(0x10);
    uint32_t interleave = be(0x1C);
    uint32_t hdr_off    = be(0x20);
    uint32_t hdr_size   = be(0x24);
    uint32_t data_off   = be(0x28);
    uint32_t data_size  = be(0x2C);   // per channel

    if (channels < 1 || channels > 2 || rate <= 0 || samples == 0 ||
        (size_t)hdr_off + (size_t)channels * hdr_size > pack_size ||
        (size_t)data_off + (size_t)channels * data_size > pack_size) {
        spdlog::warn("gen4 audio: IDSP stream shape not understood");
        return false;
    }
    if (interleave == 0) interleave = data_size;   // planar: one block each

    std::vector<IdspChannel> chans(channels);
    for (int c = 0; c < channels; c++) {
        const uint8_t* h = p + hdr_off + (size_t)c * hdr_size;
        for (int i = 0; i < 16; i++)
            chans[c].coef[i] = (int16_t)read_be16(h + 0x1C + i * 2);
        chans[c].hist1 = (int16_t)read_be16(h + 0x40);
        chans[c].hist2 = (int16_t)read_be16(h + 0x42);
    }

    out.channels    = channels;
    out.sample_rate = rate;
    out.samples.clear();
    out.samples.reserve((size_t)samples * channels);

    // Per-channel decode buffers, filled block by block and merged.
    const uint32_t frames_per_block  = interleave / 8;
    const uint32_t samples_per_block = frames_per_block * 14;
    std::vector<std::vector<int16_t>> pcm(channels,
                                          std::vector<int16_t>(samples_per_block));

    uint32_t blocks = (data_size + interleave - 1) / interleave;
    for (uint32_t b = 0; b < blocks; b++) {
        uint32_t block_bytes = std::min<uint32_t>(interleave, data_size - b * interleave);
        uint32_t block_frames = block_bytes / 8;
        for (int c = 0; c < channels; c++) {
            const uint8_t* src = p + data_off +
                ((size_t)b * channels + c) * interleave;
            for (uint32_t f = 0; f < block_frames; f++)
                idsp_decode_frame(src + f * 8, chans[c], pcm[c].data() + f * 14);
        }
        for (uint32_t s = 0; s < block_frames * 14; s++)
            for (int c = 0; c < channels; c++)
                out.samples.push_back((float)pcm[c][s] / 32768.0f);
    }

    // The header's sample count is the real length; the rest is frame padding.
    if ((size_t)samples * channels < out.samples.size())
        out.samples.resize((size_t)samples * channels);

    return !out.samples.empty();
}

}  // namespace

bool decode_nus3bank(const fs::path& path, DecodedAudio& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        spdlog::warn("gen4 audio: cannot open {}", path.string());
        return false;
    }
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());

    size_t pack = 0, pack_size = 0;
    int    preview_ms = 0;
    if (!find_pack(file, pack, pack_size, preview_ms)) return false;
    out.preview_ms = preview_ms;

    // The Japanese builds pack IDSP; the Chinese ones BNSF/G.719.
    if (pack_size >= 4 && memcmp(file.data() + pack, "IDSP", 4) == 0) {
        if (!decode_idsp(file, pack, pack_size, out)) {
            spdlog::warn("gen4 audio: {} decoded to nothing", path.filename().string());
            return false;
        }
        spdlog::debug("gen4 audio: {} decoded (IDSP), {} frames, {} Hz, {} ch",
                      path.filename().string(), out.frame_count(), out.sample_rate, out.channels);
        return true;
    }
    Bnsf info;
    if (!parse_container(file, pack, pack_size, info)) return false;

    // One decoder per channel, each fed its own frames: the channels are
    // interleaved a whole frame at a time, not sample by sample.
    int frame_bytes = info.block_size / info.channels;
    std::vector<g719_handle*> decoders(info.channels, nullptr);
    for (int c = 0; c < info.channels; c++) {
        decoders[c] = g719_init(frame_bytes);
        if (!decoders[c]) {
            spdlog::warn("gen4 audio: decoder would not start for {}", path.string());
            for (g719_handle* h : decoders) if (h) g719_free(h);
            return false;
        }
    }

    out.channels    = info.channels;
    out.sample_rate = info.sample_rate;
    out.samples.clear();
    out.samples.reserve((size_t)info.num_samples * info.channels);

    const int    samples_per_frame = info.block_samples > 0 ? info.block_samples : 960;
    const uint8_t* data = file.data() + info.data_offset;
    size_t       pos    = 0;
    std::vector<std::vector<int16_t>> pcm(info.channels,
                                          std::vector<int16_t>(samples_per_frame));

    while (pos + (size_t)info.block_size <= info.data_size) {
        for (int c = 0; c < info.channels; c++) {
            g719_decode_frame(decoders[c],
                              (void*)(data + pos + (size_t)c * frame_bytes),
                              pcm[c].data());
        }
        pos += info.block_size;

        for (int s = 0; s < samples_per_frame; s++)
            for (int c = 0; c < info.channels; c++)
                out.samples.push_back((float)pcm[c][s] / 32768.0f);
    }

    for (g719_handle* h : decoders) g719_free(h);

    // The header's sample count is the real length; the last frame is padding.
    size_t wanted = (size_t)info.num_samples * info.channels;
    if (info.num_samples > 0 && wanted < out.samples.size())
        out.samples.resize(wanted);

    if (out.samples.empty()) {
        spdlog::warn("gen4 audio: {} decoded to nothing", path.filename().string());
        return false;
    }

    spdlog::debug("gen4 audio: {} decoded, {} frames, {} Hz, {} ch",
                  path.filename().string(), out.frame_count(), out.sample_rate, out.channels);
    return true;
}

}  // namespace gen4
