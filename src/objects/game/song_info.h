#pragma once

#include "../../libs/gfx/text.h"

class SongNum {
private:
    std::unique_ptr<OutlinedText> text;

public:
    float width = 0.0f;
    float height = 0.0f;
    SongNum() = default;
    SongNum(int song_num, float outline_override = -1.0f);
    // `config_key`: the name of the text_config / skin_config entry, for its label
    SongNum(int value, SC key, const std::string& config_key);

    void draw(float x, float y, float fade);
};

class SongInfo {
private:
    std::string song_name;
    int genre = 0;
    FadeAnimation* fade = nullptr;
    std::unique_ptr<OutlinedText> song_title;
    std::unique_ptr<OutlinedText> song_subtitle;
    std::unique_ptr<OutlinedText> genre_text;
    std::unique_ptr<SongNum> song_num;
    std::unique_ptr<SongNum> song_max;

    TextureObject* t_genre = nullptr;
    TextureObject* t_song_num_plate = nullptr;
    Shader genre_shader;
    bool genre_shader_loaded = false;

public:
    SongInfo() = default;
    SongInfo(const std::string& song_name, const std::string& subtitle, bool show_subtitle, int genre, int song_num,
             int song_total = 0, const std::string& genre_label = "");
    SongInfo(const SongInfo&) = delete;
    SongInfo& operator=(const SongInfo&) = delete;
    SongInfo(SongInfo&& other) noexcept;
    SongInfo& operator=(SongInfo&& other) noexcept;
    ~SongInfo();

    void update(double current_ms);
    // (dx, dy) moves the whole block (the AI battle shows it at the bottom right)
    void draw(float dx = 0.0f, float dy = 0.0f);
};
