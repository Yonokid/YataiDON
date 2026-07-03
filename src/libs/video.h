#pragma once
#include <filesystem>
#include <optional>
#include <array>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include "ray.h"
#include "av.h"

namespace fs = std::filesystem;

class VideoPlayer {
private:
    struct DecodedFrame {
        std::vector<uint8_t> bytes;
        int width  = 0;
        int height = 0;
        int index  = 0;
    };

    std::optional<ray::Texture2D>           texture;
    std::array<bool, 2>                     is_finished_arr = {false, false};
    bool                                    audio_started   = false;
    std::string                             audio_s         = {};
    bool                                    is_static       = false;

    float                                   fps             = 0.f;
    double                                  duration        = 0.0;
    float                                   width           = 0.f;
    float                                   height          = 0.f;
    int                                     frame_count     = 0;

    std::optional<double>                   start_ms;
    int                                     frame_index     = 0;
    double                                  frame_duration  = 0.0;

    // Decode worker: frames are decoded ahead on a separate thread so
    // ffmpeg decode + swscale never stall the game loop
    static constexpr size_t                 MAX_QUEUED_FRAMES = 4;
    std::thread                             decode_thread;
    std::mutex                              queue_mutex;
    std::condition_variable                 queue_cv;
    std::deque<DecodedFrame>                frame_queue;    // guarded by queue_mutex
    std::vector<std::vector<uint8_t>>       spare_buffers;  // guarded by queue_mutex
    bool                                    decode_eof = false; // guarded by queue_mutex
    std::atomic<bool>                       decode_stop{false};

    // av:: wrapper objects (frame_generator/current_decoded_frame are
    // touched only by the decode thread while it runs)
    std::unique_ptr<av::AVContainer>        container;
    std::unique_ptr<av::AVVideoStream>      video_stream;
    std::unique_ptr<av::AVAudioStream>      audio_stream;
    std::unique_ptr<av::AVFrameDecoder>     frame_generator;
    std::unique_ptr<av::AVDecodedFrame>     current_decoded_frame;

    void audio_manager();
    void decode_loop();
    void stop_decode_thread();
    void upload_frame(const DecodedFrame& frame);

public:
    VideoPlayer(fs::path path);
    ~VideoPlayer();

    void start(double current_ms);
    bool is_finished() const;
    bool is_started()  const;
    void set_volume(float volume);
    void update(double current_ms);
    void draw();
    void stop();
};
