#pragma once

#include "../../libs/file/config.h"

struct SettingsAssetState {
    fs::path skin;
    std::string language;
    AudioConfig audio;
    VolumeConfig volume;

    explicit SettingsAssetState(const Config& config)
        : skin(config.paths.skin), language(config.general.language), audio(config.audio), volume(config.volume) {}

    bool needs_reload(const Config& config) const {
        return config.paths.skin != skin || config.general.language != language || config.audio != audio ||
               config.volume != volume;
    }
};
