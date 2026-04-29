#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "config.h"
#ifndef __EMSCRIPTEN__
#include "av.h"
#endif
#include "spdlog/spdlog.h"

namespace fs = std::filesystem;

// Abstract audio engine interface.
// All call sites use this type so the concrete backend can be swapped
// at build time (PortAudio on desktop, raylib raudio on web/Android).
class IAudioEngine {
public:
    virtual ~IAudioEngine() = default;

    // Populated by each backend's constructor; used by load_screen_sounds.
    fs::path sounds_path;

    // Device lifecycle
    virtual bool  init_audio_device()      = 0;
    virtual void  close_audio_device()     = 0;
    virtual bool  is_audio_device_ready() const = 0;
    virtual void  set_master_volume(float volume) = 0;
    virtual float get_master_volume()      = 0;

    // Called once per frame.  No-op for the PortAudio backend (callback-driven);
    // the raylib backend needs it to call UpdateMusicStream on each active stream.
    virtual void update() {}

    // --- Sounds (short, fully-buffered clips) ---
    virtual std::string load_sound(const fs::path& file_path, const std::string& name) = 0;
    virtual void unload_sound(const std::string& name)  = 0;

    // Concrete: iterates the skin sound directory and calls load_sound().
    void load_screen_sounds(const std::string& screen_name);

    virtual void unload_all_sounds() = 0;
    virtual void play_sound(const std::string& name, const std::string& volume_preset = "") = 0;
    virtual void stop_sound(const std::string& name)    = 0;
    virtual bool is_sound_playing(const std::string& name) = 0;
    virtual void  set_sound_volume(const std::string& name, float volume) = 0;
    virtual void  set_sound_pan(const std::string& name,   float pan)    = 0;
    virtual float get_sound_time_played(const std::string& name) const   = 0;
    virtual void  seek_sound(const std::string& name, float position)    = 0;

    // --- Music streams (large, streamed audio) ---
    virtual std::string load_music_stream(const fs::path& file_path, const std::string& name) = 0;
#ifndef __EMSCRIPTEN__
    virtual std::string load_music_stream_memory(const av::AVAudioStream& audio_stream,
                                                  const std::string& name) = 0;
#endif
    virtual void  play_music_stream(const std::string& name, const std::string& volume_preset = "") = 0;
    virtual float get_music_time_length(const std::string& name) const = 0;
    virtual float get_music_time_played(const std::string& name) const = 0;
    virtual void  set_music_volume(const std::string& name, float volume) = 0;
    virtual bool  is_music_stream_valid(const std::string& name) const = 0;
    virtual bool  is_music_stream_playing(const std::string& name) const = 0;
    virtual void  stop_music_stream(const std::string& name)    = 0;
    virtual void  unload_music_stream(const std::string& name)  = 0;
    virtual void  unload_all_music() = 0;
    virtual void  seek_music_stream(const std::string& name, float position) = 0;
};

extern std::unique_ptr<IAudioEngine> audio;
