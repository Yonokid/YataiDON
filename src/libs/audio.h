#pragma once

#include "audio_engine.h"

#ifndef AUDIO_BACKEND_RAYLIB

#include <mutex>
#include <unordered_map>

#include "config.h"
#include "audio/portaudio.h"
#include "global_data.h"

#include "audio/sndfile.h"
#include "audio/samplerate.h"
#include "av.h"
#include "spdlog/spdlog.h"
namespace fs = std::filesystem;

struct VirtualFile {
    const std::vector<uint8_t>* data = nullptr;
    sf_count_t                  pos  = 0;
};

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
    std::shared_ptr<std::vector<uint8_t>> memory_buffer;
    VirtualFile                           vio_cursor;
};


class AudioEngine : public IAudioEngine {
public:
    AudioEngine(int device_type, float sample_rate, unsigned long buffer_size, const VolumeConfig& volume_presets);
    ~AudioEngine() override;

    bool  init_audio_device()      override;
    void  close_audio_device()     override;
    bool  is_audio_device_ready() const override;
    void  set_master_volume(float volume) override;
    float get_master_volume()      override;

    std::string load_sound(const fs::path& file_path, const std::string& name) override;
    void unload_sound(const std::string& name)  override;
    void unload_all_sounds() override;
    void play_sound(const std::string& name, const std::string& volume_preset = "") override;
    void stop_sound(const std::string& name)    override;
    bool is_sound_playing(const std::string& name) override;
    void  set_sound_volume(const std::string& name, float volume) override;
    void  set_sound_pan(const std::string& name,   float pan)    override;
    float get_sound_time_played(const std::string& name) const   override;
    void  seek_sound(const std::string& name, float position)    override;

    std::string load_music_stream(const fs::path& file_path, const std::string& name) override;
    std::string load_music_stream_memory(const av::AVAudioStream& audio_stream, const std::string& name) override;
    void  play_music_stream(const std::string& name, const std::string& volume_preset = "") override;
    float get_music_time_length(const std::string& name) const override;
    float get_music_time_played(const std::string& name) const override;
    void  set_music_volume(const std::string& name, float volume) override;
    bool  is_music_stream_playing(const std::string& name) const override;
    void  stop_music_stream(const std::string& name)    override;
    void  unload_music_stream(const std::string& name)  override;
    void  unload_all_music() override;
    void  seek_music_stream(const std::string& name, float position) override;

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

    static int port_audio_callback(const void *inputBuffer, void *outputBuffer,
                                unsigned long framesPerBuffer,
                                const PaStreamCallbackTimeInfo* timeInfo,
                                PaStreamCallbackFlags statusFlags,
                                void *userData);

    std::string path_to_string(const fs::path& path) const;
};

#endif  // AUDIO_BACKEND_RAYLIB
