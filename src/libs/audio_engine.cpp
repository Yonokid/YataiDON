#include "audio_engine.h"
#include "global_data.h"
#include <iostream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

AudioEngine::AudioEngine(int device_type, float sample_rate, int buffer_size,
                         const VolumeConfig& volume_presets,
                         const std::optional<fs::path>& sounds_path)
    : deviceType(std::max(device_type, 0))
    , targetSampleRate(sample_rate < 0 ? 44100.0f : sample_rate)
    , bufferSize(buffer_size)
    , volumePresets(volume_presets)
    , audioDeviceReady(false)
{
    fs::path skin_path = fs::path("Skins") / global_data.config->paths.skin / "Sounds";
    if (fs::exists(skin_path)) {
        soundsPath = skin_path;
    } else {
        TraceLog(LOG_ERROR, "Skin directory not found, skipping audio initialization");
    }
}

AudioEngine::~AudioEngine() {
    if (audioDeviceReady) {
        closeAudioDevice();
    }
}

void AudioEngine::setLogLevel(int level) {
    set_log_level(level);
}

void AudioEngine::listHostApis() const {
    list_host_apis();
}

std::string AudioEngine::getHostApiName(int api_id) const {
    const char* result = get_host_api_name(api_id);
    return result ? std::string(result) : "";
}

bool AudioEngine::initAudioDevice() {
    try {
        init_audio_device(deviceType, targetSampleRate, bufferSize);
        audioDeviceReady = is_audio_device_ready();

        if (audioDeviceReady) {
            std::string don_path = pathToString(soundsPath / "don.wav");
            don = load_sound(don_path.c_str());

            std::string kat_path = pathToString(soundsPath / "ka.wav");
            kat = load_sound(kat_path.c_str());

            std::cout << "Audio device initialized successfully" << std::endl;
        }

        return audioDeviceReady;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize audio device: " << e.what() << std::endl;
        return false;
    }
}

void AudioEngine::closeAudioDevice() {
    try {
        // Clean up all sounds and music
        unloadAllSounds();
        unloadAllMusic();

        unload_sound(don);
        unload_sound(kat);
        close_audio_device();
        audioDeviceReady = false;

        std::cout << "Audio device closed" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error closing audio device: " << e.what() << std::endl;
    }
}

bool AudioEngine::isAudioDeviceReady() const {
    return is_audio_device_ready();
}

void AudioEngine::setMasterVolume(float volume) {
    set_master_volume(std::clamp(volume, 0.0f, 1.0f));
}

float AudioEngine::getMasterVolume() const {
    return get_master_volume();
}

std::string AudioEngine::pathToString(const fs::path& path) const {
#ifdef _WIN32
    // Convert to ANSI codepage for Windows (CP932 for Japanese)
    std::wstring wpath = path.wstring();
    int size = WideCharToMultiByte(932, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size > 0) {
        std::string result(size - 1, '\0');
        WideCharToMultiByte(932, 0, wpath.c_str(), -1, &result[0], size, nullptr, nullptr);
        return result;
    }
#endif
    return path.string();
}

std::string AudioEngine::loadSound(const fs::path& file_path, const std::string& name) {
    try {
        std::string path_str = pathToString(file_path);
        sound snd = load_sound(path_str.c_str());

        if (!is_sound_valid(snd)) {
            // Try UTF-8 encoding as fallback
            path_str = file_path.string();
            snd = load_sound(path_str.c_str());
        }

        if (is_sound_valid(snd)) {
            sounds[name] = snd;
            return name;
        } else {
            std::cerr << "Failed to load sound: " << file_path << std::endl;
            return "";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading sound " << file_path << ": " << e.what() << std::endl;
        return "";
    }
}

void AudioEngine::unloadSound(const std::string& name) {
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        unload_sound(it->second);
        sounds.erase(it);
    } else {
        std::cerr << "Sound " << name << " not found" << std::endl;
    }
}

void AudioEngine::loadScreenSounds(const std::string& screen_name) {
    fs::path path = soundsPath / screen_name;
    if (!fs::exists(path)) {
        std::cerr << "Sounds for screen " << screen_name << " not found" << std::endl;
        return;
    }

    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.is_directory()) {
            for (const auto& file : fs::directory_iterator(entry.path())) {
                loadSound(file.path(), entry.path().stem().string() + "_" + file.path().stem().string());
            }
        } else if (entry.is_regular_file()) {
            loadSound(entry.path(), entry.path().stem().string());
        }
    }

    fs::path global_path = soundsPath / "global";
    if (fs::exists(global_path)) {
        for (const auto& entry : fs::directory_iterator(global_path)) {
            if (entry.is_directory()) {
                for (const auto& file : fs::directory_iterator(entry.path())) {
                    loadSound(file.path(), entry.path().stem().string() + "_" + file.path().stem().string());
                }
            } else if (entry.is_regular_file()) {
                loadSound(entry.path(), entry.path().stem().string());
            }
        }
    }
}

void AudioEngine::unloadAllSounds() {
    auto sound_names = sounds;  // Copy to avoid iterator invalidation
    for (const auto& [name, _] : sound_names) {
        unloadSound(name);
    }
}

void AudioEngine::playSound(const std::string& name, const std::string& volume_preset) {
    float volume = 1.0f;
    if (!volume_preset.empty()) {
        if (volume_preset == "sound") volume = volumePresets.sound;
        else if (volume_preset == "music") volume = volumePresets.music;
        else if (volume_preset == "voice") volume = volumePresets.voice;
        else if (volume_preset == "hitsound") volume = volumePresets.hitsound;
        else if (volume_preset == "attract_mode") volume = volumePresets.attract_mode;
    }

    if (name == "don") {
        if (!volume_preset.empty()) {
            set_sound_volume(don, volume);
        }
        play_sound(don);
    } else if (name == "kat") {
        if (!volume_preset.empty()) {
            set_sound_volume(kat, volume);
        }
        play_sound(kat);
    } else {
        auto it = sounds.find(name);
        if (it != sounds.end()) {
            if (!volume_preset.empty()) {
                set_sound_volume(it->second, volume);
            }
            play_sound(it->second);
        } else {
            std::cerr << "Sound " << name << " not found" << std::endl;
        }
    }
}

void AudioEngine::stopSound(const std::string& name) {
    if (name == "don") {
        stop_sound(don);
    } else if (name == "kat") {
        stop_sound(kat);
    } else {
        auto it = sounds.find(name);
        if (it != sounds.end()) {
            stop_sound(it->second);
        } else {
            std::cerr << "Sound " << name << " not found" << std::endl;
        }
    }
}

bool AudioEngine::isSoundPlaying(const std::string& name) const {
    if (name == "don") {
        return is_sound_playing(don);
    } else if (name == "kat") {
        return is_sound_playing(kat);
    }

    auto it = sounds.find(name);
    if (it != sounds.end()) {
        return is_sound_playing(it->second);
    } else {
        std::cerr << "Sound " << name << " not found" << std::endl;
        return false;
    }
}

void AudioEngine::setSoundVolume(const std::string& name, float volume) {
    if (name == "don") {
        set_sound_volume(don, volume);
    } else if (name == "kat") {
        set_sound_volume(kat, volume);
    } else {
        auto it = sounds.find(name);
        if (it != sounds.end()) {
            set_sound_volume(it->second, volume);
        } else {
            std::cerr << "Sound " << name << " not found" << std::endl;
        }
    }
}

void AudioEngine::setSoundPan(const std::string& name, float pan) {
    if (name == "don") {
        set_sound_pan(don, pan);
    } else if (name == "kat") {
        set_sound_pan(kat, pan);
    } else {
        auto it = sounds.find(name);
        if (it != sounds.end()) {
            set_sound_pan(it->second, pan);
        } else {
            std::cerr << "Sound " << name << " not found" << std::endl;
        }
    }
}

std::string AudioEngine::loadMusicStream(const fs::path& file_path, const std::string& name) {
    std::string path_str = pathToString(file_path);
    music mus = load_music_stream(path_str.c_str());

    if (!is_music_valid(mus)) {
        // Try UTF-8 encoding as fallback
        path_str = file_path.string();
        mus = load_music_stream(path_str.c_str());
    }

    if (is_music_valid(mus)) {
        musicStreams[name] = mus;
        std::cout << "Loaded music stream from " << file_path << " as " << name << std::endl;
        return name;
    } else {
        std::cerr << "Failed to load music: " << file_path << std::endl;
        return "";
    }
}

void AudioEngine::playMusicStream(const std::string& name, const std::string& volume_preset) {
    auto it = musicStreams.find(name);
    if (it != musicStreams.end()) {
        seek_music_stream(it->second, 0);
        if (!volume_preset.empty()) {
            float volume = 1.0f;
            if (volume_preset == "sound") volume = volumePresets.sound;
            else if (volume_preset == "music") volume = volumePresets.music;
            else if (volume_preset == "voice") volume = volumePresets.voice;
            else if (volume_preset == "hitsound") volume = volumePresets.hitsound;
            else if (volume_preset == "attract_mode") volume = volumePresets.attract_mode;

            set_music_volume(it->second, volume);
        }
        play_music_stream(it->second);
    } else {
        std::cerr << "Music stream " << name << " not found" << std::endl;
    }
}

void AudioEngine::updateMusicStream(const std::string& name) {
    auto it = musicStreams.find(name);
    if (it != musicStreams.end()) {
        update_music_stream(it->second);
    } else {
        std::cerr << "Music stream " << name << " not found" << std::endl;
    }
}

float AudioEngine::getMusicTimeLength(const std::string& name) const {
    auto it = musicStreams.find(name);
    if (it != musicStreams.end()) {
        return get_music_time_length(it->second);
    } else {
        std::cerr << "Music stream " << name << " not found" << std::endl;
        return 0.0f;
    }
}

float AudioEngine::getMusicTimePlayed(const std::string& name) const {
    auto it = musicStreams.find(name);
    if (it != musicStreams.end()) {
        return get_music_time_played(it->second);
    } else {
        std::cerr << "Music stream " << name << " not found" << std::endl;
        return 0.0f;
    }
}

void AudioEngine::setMusicVolume(const std::string& name, float volume) {
    auto it = musicStreams.find(name);
    if (it != musicStreams.end()) {
        set_music_volume(it->second, volume);
    } else {
        std::cerr << "Music stream " << name << " not found" << std::endl;
    }
}

bool AudioEngine::isMusicStreamPlaying(const std::string& name) const {
    auto it = musicStreams.find(name);
    if (it != musicStreams.end()) {
        return is_music_stream_playing(it->second);
    } else {
        std::cerr << "Music stream " << name << " not found" << std::endl;
        return false;
    }
}

void AudioEngine::stopMusicStream(const std::string& name) {
    auto it = musicStreams.find(name);
    if (it != musicStreams.end()) {
        stop_music_stream(it->second);
    } else {
        std::cerr << "Music stream " << name << " not found" << std::endl;
    }
}

void AudioEngine::unloadMusicStream(const std::string& name) {
    auto it = musicStreams.find(name);
    if (it != musicStreams.end()) {
        unload_music_stream(it->second);
        musicStreams.erase(it);
    } else {
        std::cerr << "Music stream " << name << " not found" << std::endl;
    }
}

void AudioEngine::unloadAllMusic() {
    auto music_names = musicStreams;  // Copy to avoid iterator invalidation
    for (const auto& [name, _] : music_names) {
        unloadMusicStream(name);
    }
}

void AudioEngine::seekMusicStream(const std::string& name, float position) {
    auto it = musicStreams.find(name);
    if (it != musicStreams.end()) {
        seek_music_stream(it->second, position);
    } else {
        std::cerr << "Music stream " << name << " not found" << std::endl;
    }
}

// Global audio instance
std::unique_ptr<AudioEngine> audio;
