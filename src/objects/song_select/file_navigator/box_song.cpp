#include "box_song.h"
#include "../../../libs/text.h"
#include "navigator.h"
#include "../../../libs/audio.h"
#include <thread>

namespace {
    double bgm_resume_at   = 0.0;   // 0 = nothing pending
    int    preview_holders = 0;     // focused song boxes that own the bgm slot
}

void SongBox::reset_bgm_slot() {
    bgm_resume_at   = 0.0;
    preview_holders = 0;
}

void SongBox::service_bgm_resume(double current_ms) {
    if (bgm_resume_at <= 0.0) return;
    if (preview_holders > 0) { bgm_resume_at = 0.0; return; }   // a song took it
    if (current_ms < bgm_resume_at) return;
    bgm_resume_at = 0.0;
    audio.play_sound("bgm", VolumePreset::MUSIC);
}

SongBox::SongBox(const fs::path& path, const BoxDef& box_def, SongParser parser)
    : BaseBox(path, box_def)
{
    song_genre_index = genre_index;
    song_genre_label = box_def.genre_label;

    parser.get_metadata();
    auto& titles = parser.metadata.title;
    const std::string& lang = global_data.config->general.language;
    text_name = titles.count(lang) ? titles.at(lang) : titles.count("en") ? titles.at("en") : titles.empty() ? "" : titles.begin()->second;

    auto& subtitles = parser.metadata.subtitle;
    text_subtitle = subtitles.count(lang) ? subtitles.at(lang) : subtitles.count("en") ? subtitles.at("en") : subtitles.empty() ? "" : subtitles.begin()->second;

    audio.queue_song_loudness(parser.metadata.wave);
    this->parser = std::move(parser);

    is_favorite = false;
    diff_fade_in = (FadeAnimation*)tex.get_animation(12);
    refresh_scores();
}

void SongBox::refresh_scores() {
    hashes = scores_manager.get_hashes(path);
#ifdef SUPPORT_FUMEN
    bool cheap_hash = !std::holds_alternative<FumenParser>(parser.impl);
#else
    bool cheap_hash = true;
#endif
    for (const auto& [course, course_data] : parser.metadata.course_data) {
        if (course < 0 || course >= static_cast<int>(hashes.size()))
            continue;
        if (hashes[course].empty() && cheap_hash)
            hashes[course] = parser.get_diff_hash(course);
    }
    for (int i = 0; i < 5; i++) {
        const std::string& p1_id = global_data.config->network.access_code_1;
        const std::string& p2_id = global_data.config->network.access_code_2;
        scores[i] = scores_manager.get_score(hashes[i], i, p1_id);
        scores_p2[i] = navigator.is_2p
            ? scores_manager.get_score(hashes[i], i, p2_id)
            : std::nullopt;
    }
    score_history.reset();
}

std::string SongBox::hash_for(int difficulty) {
    if (difficulty < 0 || difficulty >= (int)hashes.size()) return "";
    if (hashes[difficulty].empty()) {
        hashes[difficulty] = parser.get_diff_hash(difficulty);
        if (!hashes[difficulty].empty())
            scores_manager.add_path_binding(path, hashes);
    }
    return hashes[difficulty];
}

void SongBox::reset() {
    BaseBox::reset();
    diff_fade_in = (FadeAnimation*)tex.get_animation(12);
    if (audio.is_music_stream_valid("preview")) {
        audio.unload_music_stream("preview");
    }
    music_playing = false;
    if (preview_thread.joinable()) preview_thread.join();
    preview_load.reset();
    preview_attempted = false;
    release_preview_slot();
    score_history.reset();
    box_opened_at = 0.0;
}

std::vector<Difficulty> SongBox::get_diffs() {
    std::vector<Difficulty> diffs;
    for (const auto& [diff, level] : parser.metadata.course_data) {
        diffs.push_back(Difficulty(diff));
    }
    return diffs;
}

void SongBox::preregister_text() {
    BaseBox::preregister_text();
    float base_sub_font = (float)tex.skin_config[SC::YB_SUBTITLE].font_size;
    float sub_font = utf8_char_count(text_subtitle) >= 30 ? base_sub_font - 10.0f * tex.screen_scale : base_sub_font;
    font_manager.register_text(text_subtitle, (int)sub_font);
    float base_name_font = (float)tex.skin_config[SC::SONG_BOX_NAME].font_size;
    float name_font = utf8_char_count(text_name) >= 30 ? base_name_font - 10.0f * tex.screen_scale : base_name_font;
    font_manager.register_text(text_name, (int)name_font);
    font_manager.register_text("BPM\n0123456789", tex.skin_config[SC::SONG_BOX_BPM].font_size);
}

void SongBox::load_text() {
    BaseBox::load_text();
    bpm_text = make_unique<OutlinedText>("BPM\n" + std::to_string(static_cast<int>(parser.metadata.bpm)), tex.skin_config[SC::SONG_BOX_BPM].font_size, ray::WHITE, ray::BLACK, false);
    if (exists(parser.metadata.preimage)) {
        if (preimage.has_value()) ray::UnloadTexture(preimage.value());
        preimage = ray::LoadTexture(parser.metadata.preimage.string().c_str());
        ray::GenTextureMipmaps(&preimage.value());
        ray::SetTextureFilter(preimage.value(), ray::TEXTURE_FILTER_TRILINEAR);
    }
    text_loaded = true;
}

void SongBox::update(double current_time) {
    BaseBox::update(current_time);
    diff_fade_in->update(current_time);

    // update() runs every frame for every box of the list: the audio file is looked at once,
    // when this box is first opened, instead of building paths / stat-ing it every frame
    if (yellow_box_active && wave_kind == WaveKind::UNKNOWN) {
        audio.queue_song_loudness(parser.metadata.wave, true);
        const auto wave_ext = parser.metadata.wave.extension();
        const bool bank = wave_ext == ".nus3bank" || wave_ext == ".nub";
        std::error_code ec;
        const bool exists = !parser.metadata.wave.empty() && fs::exists(parser.metadata.wave, ec);
        wave_kind = !exists ? WaveKind::MISSING : bank ? WaveKind::BANK : WaveKind::STREAM;
    }
    const bool is_bank = wave_kind == WaveKind::BANK;
    const bool is_stream = wave_kind == WaveKind::STREAM;
    // The yellow-box open animation (slide-in ~133ms + left_out ~217ms) finishes in ~350ms;
    // wait that long rather than polling the animation, which the Lua skin now drives.
    bool box_opened = get_current_ms() - bar_open_started_at > 350;

    if (is_bank && yellow_box_active && !music_playing && !preview_load &&
        !preview_attempted && get_current_ms() - bar_open_started_at > 250) {
        preview_attempted = true;
        preview_load = std::make_shared<PreviewLoad>();
        if (preview_thread.joinable()) preview_thread.join();
        preview_thread = std::thread([state = preview_load, wave = parser.metadata.wave] {
            state->ok = audio.prepare_nus3bank_pcm(wave, state->pcm, true);
            state->done.store(true, std::memory_order_release);
        });
    }

    if (is_stream && yellow_box_active && box_opened && !music_playing) {
        audio.load_music_stream(parser.metadata.wave, "preview");
        if (audio.is_music_stream_valid("preview")) {
            music_playing = true;
            audio.stop_sound("bgm");
            audio.play_music_stream("preview", VolumePreset::MUSIC);
            audio.seek_music_stream("preview", parser.metadata.demostart);
        }
    }

    if (preview_load && preview_load->done.load(std::memory_order_acquire) &&
        yellow_box_active && box_opened && !music_playing) {
        auto state = std::move(preview_load);
        if (state->ok) {
            float demo_start = state->pcm.preview_ms > 0
                             ? state->pcm.preview_ms / 1000.0f
                             : parser.metadata.demostart;
            audio.load_music_stream_prepared(std::move(state->pcm), "preview");
            if (audio.is_music_stream_valid("preview")) {
                music_playing = true;
                audio.stop_sound("bgm");
                audio.play_music_stream("preview", VolumePreset::MUSIC);
                audio.seek_music_stream("preview", demo_start);
            }
        }
    }

    if (!score_history) {
        for (const auto& s : scores) {
            if (s.has_value()) {
                score_history = std::make_unique<ScoreHistory>(scores, current_time);
                break;
            }
        }
    }

    if (score_history)
        score_history->update(current_time);
}

void SongBox::expand_box() {
    BaseBox::expand_box();
    box_opened_at = get_current_ms();
    if (!holds_preview_slot && fs::exists(parser.metadata.wave)) {
        holds_preview_slot = true;
        preview_holders++;
    }
}

void SongBox::release_preview_slot() {
    if (!holds_preview_slot) return;
    holds_preview_slot = false;
    if (preview_holders > 0) preview_holders--;
}

void SongBox::close_box() {
    BaseBox::close_box();
    box_opened_at = 0.0;
    if (preview_thread.joinable()) preview_thread.join();
    preview_load.reset();
    preview_attempted = false;
    release_preview_slot();
    if (music_playing) {
        if (audio.is_music_stream_valid("preview")) {
            audio.stop_music_stream("preview");
            audio.unload_music_stream("preview");
        }
        bgm_resume_at = get_current_ms() + 330.0;
        music_playing = false;
    }
}

void SongBox::draw_score_history() {
    if (!score_history) return;
    if (!yellow_box_opened) return;
    if (get_current_ms() < box_opened_at + 3000.0) return;
    score_history->draw();
}

void SongBox::enter_box() {
    is_diff_select = true;
    diff_fade_in->start();
}
