#pragma once

// ---------------------------------------------------------------------------
// av.h  –  Thin C++ wrapper around libavformat / libavcodec / libswscale
//           designed to replace the PyAV usage in VideoPlayer.
//
// Link with: -lavformat -lavcodec -lavutil -lswscale
// ---------------------------------------------------------------------------

#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace av {

// ---------------------------------------------------------------------------
// Internal error helper
// ---------------------------------------------------------------------------
inline void check(int ret, const char* context = "") {
    if (ret < 0) {
        char buf[256];
        av_strerror(ret, buf, sizeof(buf));
        throw std::runtime_error(std::string(context) + ": " + buf);
    }
}

// ---------------------------------------------------------------------------
// AVPlane  –  a non-owning view over one plane of a decoded/reformatted frame
// ---------------------------------------------------------------------------
class AVPlane {
public:
    AVPlane(const uint8_t* data, std::size_t size)
        : data_(data), size_(size) {}

    const uint8_t* data() const { return data_; }
    std::size_t    size() const { return size_; }

private:
    const uint8_t* data_;
    std::size_t    size_;
};

// ---------------------------------------------------------------------------
// AVDecodedFrame  –  a single video frame, optionally reformatted to RGB24
// ---------------------------------------------------------------------------
class AVDecodedFrame {
public:
    explicit AVDecodedFrame(::AVFrame* frame)
        : frame_(frame), rgb_frame_(nullptr), sws_ctx_(nullptr) {}

    ~AVDecodedFrame() {
        if (rgb_frame_) {
            av_freep(&rgb_frame_->data[0]);
            av_frame_free(&rgb_frame_);
        }
        if (sws_ctx_) sws_freeContext(sws_ctx_);
        if (frame_)   av_frame_free(&frame_);
    }

    // Non-copyable
    AVDecodedFrame(const AVDecodedFrame&)            = delete;
    AVDecodedFrame& operator=(const AVDecodedFrame&) = delete;

    // Convert the frame in-place to packed RGB24 using swscale.
    void reformat(const std::string& fmt) {
        if (fmt != "rgb24") {
            throw std::runtime_error("av::AVDecodedFrame::reformat: only \"rgb24\" is supported");
        }
        if (rgb_frame_) return;  // already converted

        rgb_frame_ = av_frame_alloc();
        if (!rgb_frame_) throw std::runtime_error("av_frame_alloc failed");

        rgb_frame_->format = AV_PIX_FMT_RGB24;
        rgb_frame_->width  = frame_->width;
        rgb_frame_->height = frame_->height;

        int ret = av_image_alloc(
            rgb_frame_->data, rgb_frame_->linesize,
            frame_->width, frame_->height,
            AV_PIX_FMT_RGB24, 1);
        check(ret, "av_image_alloc");

        sws_ctx_ = sws_getContext(
            frame_->width,  frame_->height,
            static_cast<AVPixelFormat>(frame_->format),
            frame_->width,  frame_->height,
            AV_PIX_FMT_RGB24,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!sws_ctx_) throw std::runtime_error("sws_getContext failed");

        sws_scale(
            sws_ctx_,
            frame_->data,      frame_->linesize,      0, frame_->height,
            rgb_frame_->data,  rgb_frame_->linesize);
    }

    int width()  const { return frame_->width;  }
    int height() const { return frame_->height; }

    // Returns plane 0 of the (possibly reformatted) frame.
    // Must call reformat() first for RGB24 data.
    AVPlane plane(int index) const {
        if (index != 0) throw std::runtime_error("av::AVDecodedFrame::plane: only plane 0 is supported");
        const ::AVFrame* src = rgb_frame_ ? rgb_frame_ : frame_;
        std::size_t size = static_cast<std::size_t>(
            av_image_get_buffer_size(
                static_cast<AVPixelFormat>(src->format),
                src->width, src->height, 1));
        return AVPlane(src->data[0], size);
    }

private:
    ::AVFrame*   frame_;
    ::AVFrame*   rgb_frame_;
    SwsContext*  sws_ctx_;
};

// ---------------------------------------------------------------------------
// AVFrameDecoder  –  sequential video-frame decoder (replaces the PyAV
//                    generator returned by container.decode(video=0))
// ---------------------------------------------------------------------------
class AVFrameDecoder {
public:
    AVFrameDecoder(::AVFormatContext* fmt_ctx, int stream_index, const ::AVCodec* codec)
        : fmt_ctx_(fmt_ctx), stream_index_(stream_index)
    {
        codec_ctx_ = avcodec_alloc_context3(codec);
        if (!codec_ctx_) throw std::runtime_error("avcodec_alloc_context3 failed");

        check(avcodec_parameters_to_context(
                  codec_ctx_,
                  fmt_ctx_->streams[stream_index_]->codecpar),
              "avcodec_parameters_to_context");

        check(avcodec_open2(codec_ctx_, codec, nullptr), "avcodec_open2");

        packet_ = av_packet_alloc();
        if (!packet_) throw std::runtime_error("av_packet_alloc failed");
    }

    ~AVFrameDecoder() {
        if (codec_ctx_) avcodec_free_context(&codec_ctx_);
        if (packet_)    av_packet_free(&packet_);
    }

    AVFrameDecoder(const AVFrameDecoder&)            = delete;
    AVFrameDecoder& operator=(const AVFrameDecoder&) = delete;

    // Decode the next video frame into out_frame.
    // Returns true on success, false when the stream is exhausted.
    bool next(std::unique_ptr<AVDecodedFrame>& out_frame) {
        while (true) {
            // First, try to drain any frames already queued in the decoder.
            ::AVFrame* raw = av_frame_alloc();
            if (!raw) throw std::runtime_error("av_frame_alloc failed");

            int ret = avcodec_receive_frame(codec_ctx_, raw);
            if (ret == 0) {
                out_frame = std::make_unique<AVDecodedFrame>(raw);
                return true;
            }
            av_frame_free(&raw);

            if (ret == AVERROR_EOF) return false;
            if (ret != AVERROR(EAGAIN)) { check(ret, "avcodec_receive_frame"); }

            // Need more packets – read from the container.
            bool got_packet = false;
            while (av_read_frame(fmt_ctx_, packet_) >= 0) {
                if (packet_->stream_index == stream_index_) {
                    check(avcodec_send_packet(codec_ctx_, packet_), "avcodec_send_packet");
                    av_packet_unref(packet_);
                    got_packet = true;
                    break;
                }
                av_packet_unref(packet_);
            }

            if (!got_packet) {
                // Flush the decoder.
                avcodec_send_packet(codec_ctx_, nullptr);
            }
        }
    }

private:
    ::AVFormatContext* fmt_ctx_;       // non-owning – owned by AVContainer
    int                stream_index_;
    ::AVCodecContext*  codec_ctx_ = nullptr;
    ::AVPacket*        packet_    = nullptr;
};

// ---------------------------------------------------------------------------
// AVVideoStream  –  metadata wrapper for a video stream
// ---------------------------------------------------------------------------
class AVVideoStream {
public:
    explicit AVVideoStream(::AVStream* stream) : stream_(stream) {}

    std::optional<float> average_rate() const {
        if (stream_->avg_frame_rate.den == 0) return std::nullopt;
        return static_cast<float>(stream_->avg_frame_rate.num) /
               static_cast<float>(stream_->avg_frame_rate.den);
    }

    int width()  const { return stream_->codecpar->width;  }
    int height() const { return stream_->codecpar->height; }

    int index() const { return stream_->index; }

    const ::AVCodec* find_codec() const {
        const ::AVCodec* codec = avcodec_find_decoder(stream_->codecpar->codec_id);
        if (!codec) throw std::runtime_error("Could not find video decoder");
        return codec;
    }

private:
    ::AVStream* stream_;  // non-owning
};

// ---------------------------------------------------------------------------
// AVAudioStream  –  opaque wrapper for an audio stream; carries the raw
//                   encoded data so the Audio wrapper can mux it into a
//                   raylib Music stream.
//
//   If constructed via make_silent(duration_seconds), encoded_bytes() returns
//   a valid PCM WAV file containing silence for that duration so VideoPlayer
//   requires no special-casing for audio-less video files.
// ---------------------------------------------------------------------------
class AVAudioStream {
public:
    // Normal constructor – wraps a real stream from the container.
    explicit AVAudioStream(::AVStream* stream, ::AVFormatContext* fmt_ctx)
        : stream_(stream), fmt_ctx_(fmt_ctx), silent_(false), silent_duration_(0.0) {}

    // Silent constructor – no real stream, generates silence on demand.
    static std::unique_ptr<AVAudioStream> make_silent(double duration_seconds) {
        auto s = std::unique_ptr<AVAudioStream>(new AVAudioStream());
        s->silent_          = true;
        s->silent_duration_ = duration_seconds > 0.0 ? duration_seconds : 1.0;
        return s;
    }

    int  index()     const { return stream_ ? stream_->index : -1; }
    bool is_silent() const { return silent_; }

    // For real streams:   returns raw encoded packets (fed to sf_open_virtual).
    // For silent streams: returns a minimal 44100 Hz stereo 16-bit PCM WAV of silence.
    std::vector<uint8_t> encoded_bytes() const {
        if (silent_) return make_silence_wav(silent_duration_);
        return read_stream_bytes();
    }

    const ::AVStream* raw() const { return stream_; }

private:
    // Private default constructor used only by make_silent().
    AVAudioStream()
        : stream_(nullptr), fmt_ctx_(nullptr), silent_(false), silent_duration_(0.0) {}

    std::vector<uint8_t> read_stream_bytes() const {
        std::vector<uint8_t> out;
        ::AVPacket* pkt = av_packet_alloc();
        if (!pkt) throw std::runtime_error("av_packet_alloc failed");

        // Save and restore position so we don't disturb the container state.
        int64_t saved_pos = fmt_ctx_->pb ? avio_tell(fmt_ctx_->pb) : 0;
        av_seek_frame(fmt_ctx_, stream_->index, 0, AVSEEK_FLAG_BACKWARD);

        while (av_read_frame(fmt_ctx_, pkt) >= 0) {
            if (pkt->stream_index == stream_->index)
                out.insert(out.end(), pkt->data, pkt->data + pkt->size);
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);

        if (fmt_ctx_->pb && saved_pos > 0)
            avio_seek(fmt_ctx_->pb, saved_pos, SEEK_SET);

        return out;
    }

    // Builds a minimal valid PCM WAV in memory:
    //   44100 Hz, stereo, 16-bit, all zero samples.
    // libsndfile opens this via sf_open_virtual without complaint.
    static std::vector<uint8_t> make_silence_wav(double duration_seconds) {
        constexpr uint32_t sample_rate = 44100;
        constexpr uint16_t channels    = 2;
        constexpr uint16_t bits        = 16;
        constexpr uint16_t block_align = channels * (bits / 8);   // 4 bytes/frame
        constexpr uint32_t byte_rate   = sample_rate * block_align;

        const uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_seconds);
        const uint32_t data_size   = num_samples * block_align;
        const uint32_t chunk_size  = 36 + data_size;

        std::vector<uint8_t> wav;
        wav.reserve(44 + data_size);

        auto write_u16 = [&](uint16_t v) {
            wav.push_back(v & 0xFF);
            wav.push_back((v >> 8) & 0xFF);
        };
        auto write_u32 = [&](uint32_t v) {
            wav.push_back(v & 0xFF);
            wav.push_back((v >> 8)  & 0xFF);
            wav.push_back((v >> 16) & 0xFF);
            wav.push_back((v >> 24) & 0xFF);
        };
        auto write_cc = [&](const char* s) {
            wav.push_back(s[0]); wav.push_back(s[1]);
            wav.push_back(s[2]); wav.push_back(s[3]);
        };

        // RIFF header
        write_cc("RIFF"); write_u32(chunk_size); write_cc("WAVE");
        // fmt  chunk (16-byte PCM)
        write_cc("fmt "); write_u32(16);
        write_u16(1);            // PCM
        write_u16(channels);
        write_u32(sample_rate);
        write_u32(byte_rate);
        write_u16(block_align);
        write_u16(bits);
        // data chunk – all zeros = silence
        write_cc("data"); write_u32(data_size);
        wav.resize(wav.size() + data_size, 0x00);

        return wav;
    }

    ::AVStream*        stream_;          // non-owning; nullptr for silent streams
    ::AVFormatContext* fmt_ctx_;         // non-owning; nullptr for silent streams
    bool               silent_;
    double             silent_duration_;
};

// ---------------------------------------------------------------------------
// AVStreamCollection  –  returned by AVContainer::streams()
// ---------------------------------------------------------------------------
class AVStreamCollection {
public:
    explicit AVStreamCollection(::AVFormatContext* fmt_ctx) : fmt_ctx_(fmt_ctx) {}

    // Returns the Nth video stream (0-indexed).
    std::unique_ptr<AVVideoStream> video(int n = 0) const {
        int found = 0;
        for (unsigned i = 0; i < fmt_ctx_->nb_streams; ++i) {
            if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                if (found++ == n)
                    return std::make_unique<AVVideoStream>(fmt_ctx_->streams[i]);
            }
        }
        throw std::runtime_error("av::AVStreamCollection::video: stream not found");
    }

    // Returns the Nth audio stream (0-indexed).
    // If no audio stream exists, returns a silent synthetic stream sized to
    // the container duration so VideoPlayer needs no special-casing.
    std::unique_ptr<AVAudioStream> audio(int n = 0) const {
        int found = 0;
        for (unsigned i = 0; i < fmt_ctx_->nb_streams; ++i) {
            if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                if (found++ == n)
                    return std::make_unique<AVAudioStream>(fmt_ctx_->streams[i], fmt_ctx_);
            }
        }
        // No audio stream found – synthesise silence for the video duration.
        double duration_seconds = (fmt_ctx_->duration > 0)
            ? static_cast<double>(fmt_ctx_->duration) / AV_TIME_BASE
            : 1.0;
        return AVAudioStream::make_silent(duration_seconds);
    }

private:
    ::AVFormatContext* fmt_ctx_;  // non-owning
};

// ---------------------------------------------------------------------------
// AVContainer  –  top-level media file handle
// ---------------------------------------------------------------------------
class AVContainer {
public:
    // Factory – mirrors PyAV's av.open(path).
    static std::unique_ptr<AVContainer> open(const std::filesystem::path& path) {
        ::AVFormatContext* ctx = nullptr;
        check(avformat_open_input(&ctx, path.c_str(), nullptr, nullptr),
              "avformat_open_input");
        check(avformat_find_stream_info(ctx, nullptr),
              "avformat_find_stream_info");
        return std::unique_ptr<AVContainer>(new AVContainer(ctx));
    }

    ~AVContainer() { close(); }

    AVContainer(const AVContainer&)            = delete;
    AVContainer& operator=(const AVContainer&) = delete;

    // Duration in AV_TIME_BASE units (matches PyAV's container.duration).
    int64_t duration() const {
        return fmt_ctx_ ? fmt_ctx_->duration : 0;
    }

    // AV_TIME_BASE as a double (matches PyAV's av.time_base = 1/AV_TIME_BASE).
    static double time_base() {
        return static_cast<double>(AV_TIME_BASE);
    }

    AVStreamCollection streams() const {
        return AVStreamCollection(fmt_ctx_);
    }

    // Seek to a position in seconds (0 = beginning).
    void seek(double seconds) {
        int64_t ts = static_cast<int64_t>(seconds * AV_TIME_BASE);
        av_seek_frame(fmt_ctx_, -1, ts, seconds == 0.0 ? AVSEEK_FLAG_BACKWARD : 0);
    }

    // Create a sequential decoder for the video stream at the given index.
    std::unique_ptr<AVFrameDecoder> decode_video(int stream_index = 0) {
        auto vs = streams().video(stream_index);
        return std::make_unique<AVFrameDecoder>(fmt_ctx_, vs->index(), vs->find_codec());
    }

    void close() {
        if (fmt_ctx_) {
            avformat_close_input(&fmt_ctx_);
            fmt_ctx_ = nullptr;
        }
    }

private:
    explicit AVContainer(::AVFormatContext* ctx) : fmt_ctx_(ctx) {}
    ::AVFormatContext* fmt_ctx_ = nullptr;
};

} // namespace av
