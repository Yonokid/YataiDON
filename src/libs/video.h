#pragma once
#include "config.h"
#include "audio.h"
#include "texture.h"

#ifdef __EMSCRIPTEN__

#include <filesystem>
namespace fs = std::filesystem;

// Video playback is not supported on web. All methods are no-ops.
class VideoPlayer {
public:
    VideoPlayer(fs::path) {}
    ~VideoPlayer() = default;
    void start(double) {}
    bool is_finished() const { return true; }
    bool is_started()  const { return false; }
    void set_volume(float) {}
    void update(double) {}
    void draw() {}
    void stop() {}
};

#else  // !__EMSCRIPTEN__

#include "av.h"

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>
#include <cstdint>

namespace fs = std::filesystem;

class VideoPlayer {
private:
    std::optional<ray::Texture2D>           texture;
    std::array<bool, 2>                     is_finished_arr = {false, false};
    bool                                    audio_started   = false;
    std::string                             audio_s         = {};
    std::vector<uint8_t>                    current_frame_data;
    bool                                    is_static       = false;

    float                                   fps             = 0.f;
    double                                  duration        = 0.0;
    float                                   width           = 0.f;
    float                                   height          = 0.f;
    int                                     frame_count     = 0;

    std::vector<double>                     frame_timestamps;
    std::optional<double>                   start_ms;
    int                                     frame_index     = 0;
    double                                  frame_duration  = 0.0;

    // av:: wrapper objects
    std::unique_ptr<av::AVContainer>        container;
    std::unique_ptr<av::AVVideoStream>      video_stream;
    std::unique_ptr<av::AVAudioStream>      audio_stream;
    std::unique_ptr<av::AVFrameDecoder>     frame_generator;
    std::unique_ptr<av::AVDecodedFrame>     current_decoded_frame;

    void audio_manager();
    void init_frame_generator();
    bool get_next_frame_bytes(std::vector<uint8_t>& out_bytes, int& out_width, int& out_height);
    bool load_frame(int index);

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

#endif  // __EMSCRIPTEN__
