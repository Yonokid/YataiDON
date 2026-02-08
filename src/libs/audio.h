#pragma once

#include <mutex>
#include <unordered_map>

#include "config.h"
#include "audio/portaudio.h"
#include "global_data.h"

#include "audio/sndfile.h"
#include "audio/samplerate.h"
#include "spdlog/spdlog.h"

namespace fs = std::filesystem;

struct sound {
    float* data;                    // Audio sample data (interleaved stereo or mono)
    unsigned int frame_count;       // Total number of frames in the sound
    unsigned int sample_rate;       // Original sample rate of the audio
    unsigned int channels;          // Number of channels (1 = mono, 2 = stereo)

    bool is_playing;                // Whether the sound is currently playing
    unsigned int current_frame;     // Current playback position in frames
    bool loop;                      // Whether to loop the sound

    float volume;                   // Volume multiplier (0.0 to 1.0+)
    float pan;                      // Stereo pan (0.0 = left, 0.5 = center, 1.0 = right)
    float pitch;                    // Pitch/speed multiplier (1.0 = normal)

    SRC_STATE* resampler;           // libsamplerate state (if needed)
    float* resample_buffer;         // Buffer for resampled audio
};

struct music {
    SNDFILE* file_handle;           // libsndfile file handle for streaming
    SF_INFO file_info;              // Audio file information

    float* stream_buffer;           // Buffer for reading from file
    unsigned int buffer_size;       // Size of stream buffer in frames
    unsigned int buffer_position;   // Current position in stream buffer
    unsigned int frames_in_buffer;  // Number of valid frames currently in buffer

    bool is_playing;                // Whether the music is currently playing
    unsigned long long current_frame; // Current playback position in frames
    bool loop;                      // Whether to loop the music

    float volume;                   // Volume multiplier (0.0 to 1.0+)
    float pan;                      // Stereo pan (0.0 = left, 0.5 = center, 1.0 = right)
    float pitch;                    // Pitch/speed multiplier (1.0 = normal)

    SRC_STATE* resampler;           // libsamplerate state (if needed)
    float* resample_buffer;         // Buffer for resampled audio

    std::string file_path;          // Path to the audio file
};


class AudioEngine {
public:
    AudioEngine(int device_type, float sample_rate, unsigned long buffer_size, const VolumeConfig& volume_presets);
    ~AudioEngine();

    fs::path sounds_path;

    bool init_audio_device();
    void close_audio_device();
    bool is_audio_device_ready() const;
    void set_master_volume(float volume);
    float get_master_volume();

    std::string load_sound(const fs::path& file_path, const std::string& name);
    void unload_sound(const std::string& name);
    void load_screen_sounds(const std::string& screen_name);
    void unload_all_sounds();
    void play_sound(const std::string& name, const std::string& volume_preset = "");
    void stop_sound(const std::string& name);
    bool is_sound_playing(const std::string& name);
    void set_sound_volume(const std::string& name, float volume);
    void set_sound_pan(const std::string& name, float pan);

    std::string load_music_stream(const fs::path& file_path, const std::string& name);
    void play_music_stream(const std::string& name, const std::string& volume_preset = "");
    float get_music_time_length(const std::string& name) const;
    float get_music_time_played(const std::string& name) const;
    void set_music_volume(const std::string& name, float volume);
    bool is_music_stream_playing(const std::string& name) const;
    void stop_music_stream(const std::string& name);
    void unload_music_stream(const std::string& name);
    void unload_all_music();
    void seek_music_stream(const std::string& name, float position);

private:
    int host_api_index;
    double target_sample_rate;
    unsigned long buffer_size;
    VolumeConfig volume_presets;
    bool is_ready;
    mutable std::mutex lock;
    float master_volume;

    PaStream* stream;
    PaStreamParameters output_parameters;

    std::unordered_map<std::string, sound> sounds;
    std::unordered_map<std::string, music> music_streams;

    std::string don;
    std::string kat;

    static int port_audio_callback(const void *inputBuffer, void *outputBuffer,
                                unsigned long framesPerBuffer,
                                const PaStreamCallbackTimeInfo* timeInfo,
                                PaStreamCallbackFlags statusFlags,
                                void *userData);

    std::string path_to_string(const fs::path& path) const;
};

extern std::unique_ptr<AudioEngine> audio;
