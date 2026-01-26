#pragma once

#ifdef _WIN32
#include <windows.h>
#endif
#include <iostream>
#include <unordered_map>

#include "config.h"

#include "audio/audio.h"

namespace fs = std::filesystem;

class AudioEngine {
public:
    AudioEngine(int device_type, float sample_rate, int buffer_size,
                const VolumeConfig& volume_presets,
                const std::optional<fs::path>& sounds_path = std::nullopt);

    ~AudioEngine();
    fs::path soundsPath;

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    void setLogLevel(int level);
    void listHostApis() const;
    std::string getHostApiName(int api_id) const;
    bool initAudioDevice();
    void closeAudioDevice();
    bool isAudioDeviceReady() const;
    void setMasterVolume(float volume);
    float getMasterVolume() const;

    std::string loadSound(const fs::path& file_path, const std::string& name);
    void unloadSound(const std::string& name);
    void loadScreenSounds(const std::string& screen_name);
    void unloadAllSounds();
    void playSound(const std::string& name, const std::string& volume_preset = "");
    void stopSound(const std::string& name);
    bool isSoundPlaying(const std::string& name) const;
    void setSoundVolume(const std::string& name, float volume);
    void setSoundPan(const std::string& name, float pan);

    std::string loadMusicStream(const fs::path& file_path, const std::string& name);
    void playMusicStream(const std::string& name, const std::string& volume_preset = "");
    void updateMusicStream(const std::string& name);
    float getMusicTimeLength(const std::string& name) const;
    float getMusicTimePlayed(const std::string& name) const;
    void setMusicVolume(const std::string& name, float volume);
    bool isMusicStreamPlaying(const std::string& name) const;
    void stopMusicStream(const std::string& name);
    void unloadMusicStream(const std::string& name);
    void unloadAllMusic();
    void seekMusicStream(const std::string& name, float position);

private:
    int deviceType;
    float targetSampleRate;
    int bufferSize;
    VolumeConfig volumePresets;

    std::unordered_map<std::string, sound> sounds;
    std::unordered_map<std::string, music> musicStreams;

    sound don;
    sound kat;
    bool audioDeviceReady;

    std::string pathToString(const fs::path& path) const;
};

extern std::unique_ptr<AudioEngine> audio;
