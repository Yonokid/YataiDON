#pragma once

#include "../libs/file/config.h"
#include "../libs/screen.h"
#include "../objects/global/allnet_indicator.h"
#include "../objects/global/coin_overlay.h"
#include "../objects/global/indicator.h"
#include "../objects/settings/settings_box_manager.h"

// Settings that require reloading the skin's assets when changed.
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

class SettingsScreen : public Screen {
private:
    std::unique_ptr<SettingsBoxManager> box_manager;
    std::optional<Indicator> indicator;
    std::optional<CoinOverlay> coin_overlay;
    std::optional<AllNetIcon> allnet_indicator;
    std::string username_on_entry;
    std::optional<SettingsAssetState> assets_on_entry;

    TextureObject* t_background = nullptr;
    TextureObject* t_footer = nullptr;

    std::optional<Screens> handle_input();

public:
    SettingsScreen() : Screen("settings") {}

    void on_screen_start() override;
    Screens on_screen_end(Screens next_screen) override;
    std::optional<Screens> update() override;
    void draw() override;
};
