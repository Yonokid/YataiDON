#pragma once

#include "config.h"
#include "av.h"
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#ifdef _WIN32
#include <RtAudio.h>
#include "audio/wdmks_exclusive.h"
#endif
#include <sndfile.h>
#include <samplerate.h>
#include <memory>
#include <atomic>
#include <shared_mutex>
#include <vector>

#ifdef __ANDROID__
// NDK r27d libc++ omits std::atomic_ref; polyfill with __atomic builtins
namespace std {
    template<typename T>
    struct atomic_ref {
        T& ref;
        explicit atomic_ref(T& t) noexcept : ref(t) {}
        T load(memory_order order = memory_order_seq_cst) const noexcept {
            T val;
            __atomic_load(&ref, &val, static_cast<int>(order));
            return val;
        }
        void store(T desired, memory_order order = memory_order_seq_cst) noexcept {
            __atomic_store(&ref, &desired, static_cast<int>(order));
        }
        T fetch_add(T val, memory_order order = memory_order_seq_cst) noexcept {
            return __atomic_fetch_add(&ref, val, static_cast<int>(order));
        }
    };
}
#endif

namespace fs = std::filesystem;

enum class VolumePreset {
    NONE,
    SOUND,
    MUSIC,
    VOICE,
    HITSOUND,
    ATTRACT_MODE,
};

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

    float*     pcm_data        = nullptr; // Android FFmpeg fallback: fully decoded PCM
    sf_count_t pcm_total_frames = 0;
};

class AudioEngine {
public:
    fs::path sounds_path;

    AudioEngine();
    ~AudioEngine();

    bool  init_audio_device(const fs::path& sounds_path, const AudioConfig& audio_config, const VolumeConfig& volume_presets);
    void  close_audio_device();
    bool  is_audio_device_ready() const;
    void  set_master_volume(float volume);
    float get_master_volume();

    void load_screen_sounds(const std::string& screen_name);

    std::string load_sound(const fs::path& file_path, const std::string& name);
    void unload_sound(const std::string& name);
    void unload_all_sounds();
    void play_sound(const std::string& name, VolumePreset volume_preset = VolumePreset::NONE);
    void stop_sound(const std::string& name);
    bool is_sound_playing(const std::string& name);
    void  set_sound_volume(const std::string& name, float volume);
    void  set_sound_pan(const std::string& name,   float pan);
    void  set_sound_pitch(const std::string& name, float pitch);
    float get_sound_time_played(const std::string& name) const;
    void  seek_sound(const std::string& name, float position);

    std::string load_music_stream(const fs::path& file_path, const std::string& name);
#ifndef __EMSCRIPTEN__
    std::string load_music_stream_memory(const av::AVAudioStream& audio_stream, const std::string& name);
#endif
    void  play_music_stream(const std::string& name, VolumePreset volume_preset = VolumePreset::NONE);
    float get_music_time_length(const std::string& name) const;
    float get_music_time_played(const std::string& name) const;
    void  set_music_volume(const std::string& name, float volume);
    bool  is_music_stream_valid(const std::string& name) const;
    bool  is_music_stream_playing(const std::string& name) const;
    void  stop_music_stream(const std::string& name);
    void  unload_music_stream(const std::string& name);
    void  unload_all_music();
    void  seek_music_stream(const std::string& name, float position);

private:
    double target_sample_rate;
    unsigned long buffer_size;
    VolumeConfig volume_presets;
    bool is_ready;
    mutable std::shared_mutex rw_lock;
    std::atomic<float> master_volume;

    SDL_AudioStream*   sdl_stream = nullptr;
    bool               sdl_audio_subsystem_initialized = false;
    std::vector<float> sdl_scratch_buffer;

#ifdef _WIN32
    RtAudio* rt_audio = nullptr;  // ASIO (device_type == 6) only
#endif

    std::unordered_map<std::string, sound> sounds;
    std::unordered_map<std::string, music> music_streams;

    std::string path_to_string(const fs::path& path) const;

    static void mix(float* out, unsigned int framesPerBuffer, AudioEngine* engine);
    static void sdl_audio_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);
#ifdef _WIN32
    static int  rt_audio_callback(void* outputBuffer, void* inputBuffer,
                                   unsigned int framesPerBuffer, double streamTime,
                                   unsigned int status, void* userData);
#endif
};

extern AudioEngine audio;
