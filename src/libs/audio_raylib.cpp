#ifdef AUDIO_BACKEND_RAYLIB

#include "audio_raylib.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float RaylibAudioEngine::preset_volume(const std::string& preset) const {
    if (preset == "sound")        return volume_presets.sound;
    if (preset == "music")        return volume_presets.music;
    if (preset == "voice")        return volume_presets.voice;
    if (preset == "hitsound")     return volume_presets.hitsound;
    if (preset == "attract_mode") return volume_presets.attract_mode;
    return 1.0f;
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

RaylibAudioEngine::RaylibAudioEngine(const VolumeConfig& volume_presets)
    : volume_presets(volume_presets)
{
    // sounds_path is set by the caller (init_audio in YataiDON.cpp) after construction.
}

RaylibAudioEngine::~RaylibAudioEngine() {
    if (is_ready) {
        close_audio_device();
    }
}

// ---------------------------------------------------------------------------
// Device lifecycle
// ---------------------------------------------------------------------------

bool RaylibAudioEngine::init_audio_device() {
    ray::InitAudioDevice();
    is_ready = ray::IsAudioDeviceReady();
    if (is_ready) {
        ray::SetMasterVolume(master_volume);
        spdlog::info("Audio device initialized (raylib raudio backend)");
    } else {
        spdlog::error("Failed to initialize raylib audio device");
    }
    return is_ready;
}

void RaylibAudioEngine::close_audio_device() {
    unload_all_sounds();
    unload_all_music();
    ray::CloseAudioDevice();
    is_ready = false;
    spdlog::info("Audio device closed (raylib raudio backend)");
}

bool RaylibAudioEngine::is_audio_device_ready() const {
    return is_ready;
}

void RaylibAudioEngine::set_master_volume(float volume) {
    master_volume = std::clamp(volume, 0.0f, 1.0f);
    ray::SetMasterVolume(master_volume);
}

float RaylibAudioEngine::get_master_volume() {
    return master_volume;
}

// ---------------------------------------------------------------------------
// Per-frame update — must be called once per frame to keep streams flowing
// ---------------------------------------------------------------------------

void RaylibAudioEngine::update() {
    for (auto& [name, entry] : music_streams) {
        if (ray::IsMusicStreamPlaying(entry.music)) {
            ray::UpdateMusicStream(entry.music);
        }
    }
}

// ---------------------------------------------------------------------------
// Sounds
// ---------------------------------------------------------------------------

std::string RaylibAudioEngine::load_sound(const fs::path& file_path, const std::string& name) {
    ray::Sound snd = ray::LoadSound(file_path.string().c_str());
    if (snd.stream.buffer == nullptr) {
        spdlog::error("Failed to load sound: {}", file_path.string());
        return "";
    }
    sounds[name] = { snd, 1.0f };
    spdlog::debug("Loaded sound: {}", name);
    return name;
}

void RaylibAudioEngine::unload_sound(const std::string& name) {
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        ray::UnloadSound(it->second.sound);
        sounds.erase(it);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

void RaylibAudioEngine::unload_all_sounds() {
    for (auto& [name, entry] : sounds) {
        ray::UnloadSound(entry.sound);
    }
    sounds.clear();
    spdlog::info("All sounds unloaded");
}

void RaylibAudioEngine::play_sound(const std::string& name, const std::string& volume_preset) {
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        if (!volume_preset.empty()) {
            it->second.volume = preset_volume(volume_preset);
            ray::SetSoundVolume(it->second.sound, it->second.volume);
        }
        it->second.play_start = ray::GetTime();
        ray::PlaySound(it->second.sound);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

void RaylibAudioEngine::stop_sound(const std::string& name) {
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        ray::StopSound(it->second.sound);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

bool RaylibAudioEngine::is_sound_playing(const std::string& name) {
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        return ray::IsSoundPlaying(it->second.sound);
    }
    spdlog::warn("Sound {} not found", name);
    return false;
}

void RaylibAudioEngine::set_sound_volume(const std::string& name, float volume) {
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        it->second.volume = std::clamp(volume, 0.0f, 1.0f);
        ray::SetSoundVolume(it->second.sound, it->second.volume);
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

void RaylibAudioEngine::set_sound_pan(const std::string& name, float pan) {
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        ray::SetSoundPan(it->second.sound, std::clamp(pan, 0.0f, 1.0f));
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

float RaylibAudioEngine::get_sound_time_played(const std::string& name) const {
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        if (!ray::IsSoundPlaying(it->second.sound)) return 0.0f;
        return (float)(ray::GetTime() - it->second.play_start);
    }
    spdlog::warn("Sound {} not found", name);
    return 0.0f;
}

void RaylibAudioEngine::seek_sound(const std::string& name, float position) {
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        // Raylib has no native sound seeking; adjust play_start so that
        // get_sound_time_played returns the correct elapsed time.
        it->second.play_start = ray::GetTime() - (double)position;
    } else {
        spdlog::warn("Sound {} not found", name);
    }
}

// ---------------------------------------------------------------------------
// Music streams
// ---------------------------------------------------------------------------

std::string RaylibAudioEngine::load_music_stream(const fs::path& file_path, const std::string& name) {
    ray::Music music = ray::LoadMusicStream(file_path.string().c_str());
    if (music.stream.buffer == nullptr) {
        spdlog::error("Failed to load music stream: {}", file_path.string());
        return "";
    }
    music_streams[name] = { music, nullptr, 1.0f };
    spdlog::debug("Loaded music stream: {}", name);
    return name;
}

#ifndef __EMSCRIPTEN__
std::string RaylibAudioEngine::load_music_stream_memory(const av::AVAudioStream& audio_stream,
                                                          const std::string& name) {
    // encoded_bytes() returns a fully-formed WAV file in memory.
    std::vector<uint8_t> wav = audio_stream.encoded_bytes();
    if (wav.empty()) {
        spdlog::error("load_music_stream_memory: no audio data for '{}'", name);
        return "";
    }

    auto data_ptr = std::make_shared<std::vector<uint8_t>>(std::move(wav));

    // LoadMusicStreamFromMemory keeps a raw pointer into our buffer, so the
    // shared_ptr in RaylibMusicEntry keeps it alive for the stream's lifetime.
    ray::Music music = ray::LoadMusicStreamFromMemory(".wav",
                                                      data_ptr->data(),
                                                      static_cast<int>(data_ptr->size()));
    if (music.stream.buffer == nullptr) {
        spdlog::error("load_music_stream_memory: LoadMusicStreamFromMemory failed for '{}'", name);
        return "";
    }

    music_streams[name] = { music, data_ptr, 1.0f };
    spdlog::debug("Loaded memory music stream: '{}'", name);
    return name;
}
#endif  // __EMSCRIPTEN__

void RaylibAudioEngine::play_music_stream(const std::string& name, const std::string& volume_preset) {
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        if (!volume_preset.empty()) {
            it->second.volume = preset_volume(volume_preset);
            ray::SetMusicVolume(it->second.music, it->second.volume);
        }
        ray::PlayMusicStream(it->second.music);
    } else {
        spdlog::warn("Music stream {} not found", name);
    }
}

float RaylibAudioEngine::get_music_time_length(const std::string& name) const {
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        return ray::GetMusicTimeLength(it->second.music);
    }
    spdlog::warn("Music stream {} not found", name);
    return 0.0f;
}

float RaylibAudioEngine::get_music_time_played(const std::string& name) const {
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        return ray::GetMusicTimePlayed(it->second.music);
    }
    spdlog::warn("Music stream {} not found", name);
    return 0.0f;
}

void RaylibAudioEngine::set_music_volume(const std::string& name, float volume) {
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        it->second.volume = std::clamp(volume, 0.0f, 1.0f);
        ray::SetMusicVolume(it->second.music, it->second.volume);
    } else {
        spdlog::warn("Music stream {} not found", name);
    }
}

bool RaylibAudioEngine::is_music_stream_valid(const std::string& name) const {
    auto it = music_streams.find(name);
    return it != music_streams.end();
}

bool RaylibAudioEngine::is_music_stream_playing(const std::string& name) const {
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        return ray::IsMusicStreamPlaying(it->second.music);
    }
    spdlog::warn("Music stream {} not found", name);
    return false;
}

void RaylibAudioEngine::stop_music_stream(const std::string& name) {
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        ray::StopMusicStream(it->second.music);
    } else {
        spdlog::warn("Music stream {} not found", name);
    }
}

void RaylibAudioEngine::unload_music_stream(const std::string& name) {
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        ray::StopMusicStream(it->second.music);
        ray::UnloadMusicStream(it->second.music);
        music_streams.erase(it);
        spdlog::debug("Unloaded music stream: {}", name);
    } else {
        spdlog::warn("Music stream {} not found", name);
    }
}

void RaylibAudioEngine::unload_all_music() {
    for (auto& [name, entry] : music_streams) {
        ray::StopMusicStream(entry.music);
        ray::UnloadMusicStream(entry.music);
    }
    music_streams.clear();
    spdlog::info("All music streams unloaded");
}

void RaylibAudioEngine::seek_music_stream(const std::string& name, float position) {
    auto it = music_streams.find(name);
    if (it != music_streams.end()) {
        ray::SeekMusicStream(it->second.music, position);
    } else {
        spdlog::warn("Music stream {} not found", name);
    }
}

#endif  // AUDIO_BACKEND_RAYLIB
