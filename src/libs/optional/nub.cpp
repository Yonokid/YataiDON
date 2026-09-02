#include "nub.h"

#include <spdlog/spdlog.h>
#include <cstring>
#include <fstream>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace gen3 {

namespace {

size_t find_riff(const std::vector<uint8_t>& file) {
    size_t limit = std::min<size_t>(file.size(), 0x2000);
    for (size_t i = 0; i + 4 <= limit; i += 4)
        if (memcmp(file.data() + i, "RIFF", 4) == 0) return i;
    return SIZE_MAX;
}

// avio read callback over the in-memory slice.
struct MemReader {
    const uint8_t* data;
    size_t         size;
    size_t         pos = 0;
};

int mem_read(void* opaque, uint8_t* buf, int buf_size) {
    MemReader* r = static_cast<MemReader*>(opaque);
    size_t left = r->size - r->pos;
    size_t n = std::min<size_t>(left, (size_t)buf_size);
    if (n == 0) return AVERROR_EOF;
    memcpy(buf, r->data + r->pos, n);
    r->pos += n;
    return (int)n;
}

int64_t mem_seek(void* opaque, int64_t offset, int whence) {
    MemReader* r = static_cast<MemReader*>(opaque);
    if (whence == AVSEEK_SIZE) return (int64_t)r->size;
    size_t base = whence == SEEK_CUR ? r->pos : whence == SEEK_END ? r->size : 0;
    int64_t target = (int64_t)base + offset;
    if (target < 0 || target > (int64_t)r->size) return AVERROR(EINVAL);
    r->pos = (size_t)target;
    return target;
}

// One decoded frame's samples appended as interleaved floats, whatever
// packing the codec used.
bool append_samples(const AVFrame* frame, int channels, std::vector<float>& out) {
    switch (frame->format) {
        case AV_SAMPLE_FMT_FLTP:
            for (int s = 0; s < frame->nb_samples; s++)
                for (int c = 0; c < channels; c++)
                    out.push_back(reinterpret_cast<const float*>(frame->data[c])[s]);
            return true;
        case AV_SAMPLE_FMT_FLT: {
            const float* p = reinterpret_cast<const float*>(frame->data[0]);
            out.insert(out.end(), p, p + (size_t)frame->nb_samples * channels);
            return true;
        }
        case AV_SAMPLE_FMT_S16P:
            for (int s = 0; s < frame->nb_samples; s++)
                for (int c = 0; c < channels; c++)
                    out.push_back(reinterpret_cast<const int16_t*>(frame->data[c])[s] / 32768.0f);
            return true;
        case AV_SAMPLE_FMT_S16: {
            const int16_t* p = reinterpret_cast<const int16_t*>(frame->data[0]);
            for (int i = 0; i < frame->nb_samples * channels; i++)
                out.push_back(p[i] / 32768.0f);
            return true;
        }
        default:
            return false;
    }
}

}  // namespace

bool decode_nub(const fs::path& path, gen4::DecodedAudio& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        spdlog::warn("nub audio: cannot open {}", path.string());
        return false;
    }
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());

    size_t riff = find_riff(file);
    if (riff == SIZE_MAX) {
        spdlog::warn("nub audio: no RIFF stream inside {}", path.filename().string());
        return false;
    }

    // The container header carries the song-select preview point at 0xE0,
    // big-endian milliseconds. (Verified against the same songs' gen 4 banks,
    // whose TONE chunks name identical values.)
    out.preview_ms = 0;
    if (file.size() >= 0xE4) {
        uint32_t preview = ((uint32_t)file[0xE0] << 24) | ((uint32_t)file[0xE1] << 16) |
                           ((uint32_t)file[0xE2] << 8)  |  (uint32_t)file[0xE3];
        if (preview <= 30u * 60u * 1000u) out.preview_ms = (int)preview;
    }

    // The RIFF's fact chunk: real sample count and the encoder's priming
    // delay. FFmpeg hands back the delay samples as audio, which starts the
    // whole song 50 ms late against the chart, so they are cut afterwards.
    uint32_t fact_samples = 0, fact_delay = 0;
    {
        size_t pos = riff + 12;
        while (pos + 8 < file.size()) {
            uint32_t sz = (uint32_t)file[pos+4] | ((uint32_t)file[pos+5] << 8) |
                          ((uint32_t)file[pos+6] << 16) | ((uint32_t)file[pos+7] << 24);
            if (memcmp(file.data() + pos, "fact", 4) == 0 && sz >= 12 && pos + 8 + sz <= file.size()) {
                auto le32 = [&](size_t o) {
                    return (uint32_t)file[o] | ((uint32_t)file[o+1] << 8) |
                           ((uint32_t)file[o+2] << 16) | ((uint32_t)file[o+3] << 24);
                };
                fact_samples = le32(pos + 8);
                fact_delay   = le32(pos + 16);
                break;
            }
            if (memcmp(file.data() + pos, "data", 4) == 0) break;
            pos += 8 + sz + (sz & 1);
        }
    }

    MemReader reader{ file.data() + riff, file.size() - riff };

    // The buffer avio reads through; freed via the context below.
    unsigned char* avio_buf = (unsigned char*)av_malloc(0x4000);
    AVIOContext* avio = avio_alloc_context(avio_buf, 0x4000, 0, &reader,
                                           mem_read, nullptr, mem_seek);
    AVFormatContext* fmt = avformat_alloc_context();
    if (!avio || !fmt) return false;
    fmt->pb = avio;

    bool ok = false;
    AVCodecContext* codec_ctx = nullptr;
    AVPacket* pkt = nullptr;
    AVFrame*  frame = nullptr;

    do {
        if (avformat_open_input(&fmt, nullptr, nullptr, nullptr) < 0) {
            spdlog::warn("nub audio: FFmpeg will not open {}", path.filename().string());
            fmt = nullptr;   // freed by the failed open
            break;
        }
        if (avformat_find_stream_info(fmt, nullptr) < 0) break;

        int idx = -1;
        for (unsigned i = 0; i < fmt->nb_streams; i++)
            if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) { idx = (int)i; break; }
        if (idx < 0) break;

        const AVCodec* codec = avcodec_find_decoder(fmt->streams[idx]->codecpar->codec_id);
        if (!codec) {
            spdlog::warn("nub audio: no decoder for the stream in {}", path.filename().string());
            break;
        }
        codec_ctx = avcodec_alloc_context3(codec);
        if (!codec_ctx ||
            avcodec_parameters_to_context(codec_ctx, fmt->streams[idx]->codecpar) < 0 ||
            avcodec_open2(codec_ctx, codec, nullptr) < 0)
            break;

        out.channels    = codec_ctx->ch_layout.nb_channels;
        out.sample_rate = codec_ctx->sample_rate;
        out.samples.clear();
        if (out.channels < 1 || out.channels > 2) break;

        pkt   = av_packet_alloc();
        frame = av_frame_alloc();
        if (!pkt || !frame) break;

        bool bad_format = false;
        while (av_read_frame(fmt, pkt) >= 0) {
            if (pkt->stream_index == idx && avcodec_send_packet(codec_ctx, pkt) >= 0) {
                while (avcodec_receive_frame(codec_ctx, frame) >= 0)
                    if (!append_samples(frame, out.channels, out.samples)) { bad_format = true; break; }
            }
            av_packet_unref(pkt);
            if (bad_format) break;
        }
        if (!bad_format) {
            avcodec_send_packet(codec_ctx, nullptr);
            while (avcodec_receive_frame(codec_ctx, frame) >= 0)
                if (!append_samples(frame, out.channels, out.samples)) break;
        }

        // Cut the priming delay off the front and the padding off the end, so
        // sample zero of the result is sample zero of the song.
        if (fact_delay > 0 && (size_t)fact_delay * out.channels < out.samples.size())
            out.samples.erase(out.samples.begin(),
                              out.samples.begin() + (size_t)fact_delay * out.channels);
        if (fact_samples > 0 && (size_t)fact_samples * out.channels < out.samples.size())
            out.samples.resize((size_t)fact_samples * out.channels);

        ok = !bad_format && !out.samples.empty();
        if (!ok)
            spdlog::warn("nub audio: {} decoded to nothing", path.filename().string());
    } while (false);

    if (frame) av_frame_free(&frame);
    if (pkt) av_packet_free(&pkt);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (fmt) avformat_close_input(&fmt);
    if (avio) {
        av_freep(&avio->buffer);
        avio_context_free(&avio);
    }

    if (ok)
        spdlog::debug("nub audio: {} decoded, {} frames, {} Hz, {} ch",
                      path.filename().string(), out.frame_count(), out.sample_rate, out.channels);
    return ok;
}

}  // namespace green
