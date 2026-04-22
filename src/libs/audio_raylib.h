#pragma once

#ifdef AUDIO_BACKEND_RAYLIB

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "audio_engine.h"
#include "ray.h"

// Holds a raylib Music stream plus the in-memory WAV buffer that must stay
// alive as long as the stream is open (drwav keeps a raw pointer into it).
struct RaylibMusicEntry {
    ray::Music music;
    std::shared_ptr<std::vector<uint8_t>> memory_data;  // non-null for memory-loaded streams
    float  volume = 1.0f;
};

struct RaylibSoundEntry {
    ray::Sound sound;
    float  volume     = 1.0f;
    double play_start = 0.0;   // wall time (GetTime()) when PlaySound was last called
};

// Raylib raudio backend.  Used on platforms that cannot link PortAudio
// (web/Emscripten, Android).  Enabled by the AUDIO_BACKEND_RAYLIB define
// which is set in CMakeLists.txt for those targets.
//
// Key difference from the PortAudio backend: raylib uses a pull-model
// streaming API that requires UpdateMusicStream() to be called every frame.
// update() handles that; call it once per frame from the main loop.
class RaylibAudioEngine : public IAudioEngine {
public:
    explicit RaylibAudioEngine(const VolumeConfig& volume_presets);
    ~RaylibAudioEngine() override;

    bool  init_audio_device()      override;
    void  close_audio_device()     override;
    bool  is_audio_device_ready() const override;
    void  set_master_volume(float volume) override;
    float get_master_volume()      override;

    // Must be called once per frame to keep music streams from stalling.
    void update() override;

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
#ifndef __EMSCRIPTEN__
    std::string load_music_stream_memory(const av::AVAudioStream& audio_stream,
                                          const std::string& name) override;
#endif
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
    VolumeConfig volume_presets;
    bool  is_ready     = false;
    float master_volume = 1.0f;

    std::unordered_map<std::string, RaylibSoundEntry> sounds;
    std::unordered_map<std::string, RaylibMusicEntry> music_streams;

    // Returns the volume scalar for a named preset, or 1.0 if unrecognised.
    float preset_volume(const std::string& preset) const;
};

#endif  // AUDIO_BACKEND_RAYLIB
