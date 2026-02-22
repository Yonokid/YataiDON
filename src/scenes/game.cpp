#include "game.h"

void GameScreen::on_screen_start() {
    Screen::on_screen_start();
    mask_shader = ray::LoadShader("shader/dummy.vs", "shader/mask.fs");
    current_ms = 0;
    end_ms = 0;
    start_ms = 0;
    start_delay = 1000.0f;
    JudgePos::X = 414 * tex.screen_scale;
    JudgePos::Y = 256 * tex.screen_scale;
    song_started = false;
    paused = false;
    pause_time = 0;
    if (global_data.config->general.nijiiro_notes) {
        tex.textures["game"].erase("notes");
        tex.load_folder("game", "notes_nijiiro");
        tex.textures["game"]["notes"] = std::move(tex.textures["game"]["notes_nijiiro"]);
        tex.textures["game"].erase("notes_nijiiro");
    }
    auto rainbow_mask = std::dynamic_pointer_cast<SingleTexture>(tex.textures["balloon"]["rainbow_mask"]);
    auto rainbow = std::dynamic_pointer_cast<SingleTexture>(tex.textures["balloon"]["rainbow"]);
    if (rainbow_mask && rainbow) {
        SetShaderValueTexture(mask_shader, GetShaderLocation(mask_shader, "texture0"), rainbow_mask->texture);
        SetShaderValueTexture(mask_shader, GetShaderLocation(mask_shader, "texture1"), rainbow->texture);
    }
    SessionData& session_data = global_data.session_data[(int)global_data.player_num];
    init_tja(session_data.selected_song);
    spdlog::info("TJA initialized for song: {}", session_data.selected_song.string());
    load_hitsounds();
    song_info = SongInfo(session_data.song_title, session_data.genre_index);
    //self.result_transition = ResultTransition(global_data.player_num)
    //subtitle = self.parser.metadata.subtitle.get(global_data.config['general']['language'].lower(), '')
    bpm = parser->metadata.bpm;
    scene_preset = parser->metadata.scene_preset;
    //if self.movie is None:
        background.emplace(global_data.player_num, bpm, scene_preset);
        spdlog::info("Background initialized");
    //else:
        //self.background = None
        //logger.info("Movie initialized")
    //self.transition = Transition(session_data.song_title, subtitle, is_second=True)
    //self.allnet_indicator = AllNetIcon()
    //self.transition.start()
}

std::string GameScreen::on_screen_end(const std::string& next_screen) {
    song_started = false;
    end_ms = 0;
    ray::UnloadShader(mask_shader);

    //if self.movie is not None:
        //self.movie.stop()
        //logger.info("Movie stopped")
    if (background.has_value()) {
        background.reset();
        spdlog::info("Background unloaded");
    }

    return Screen::on_screen_end(next_screen);
}

void GameScreen::load_hitsounds() {
    fs::path sounds_dir = audio->sounds_path;
    if (global_data.hit_sound[(int)global_data.player_num] == -1) {
        audio->load_sound("", "hitsound_don_1p");
        audio->load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P1]) / "ka.wav", "hitsound_kat_1p");
        audio->load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P1]) / "don.wav", "hitsound_don_1p");
        audio->load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P2]) / "ka.wav", "hitsound_kat_2p");
        audio->load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P2]) / "don.wav", "hitsound_don_2p");
        spdlog::info("Loaded wav hit sounds for 1P and 2P");
    } else {
        audio->load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P1]) / "don.ogg", "hitsound_don_1p");
        audio->load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P1]) / "ka.ogg", "hitsound_kat_1p");
        audio->load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P2]) / "don.ogg", "hitsound_don_2p");
        audio->load_sound(sounds_dir / "hit_sounds" / std::to_string(global_data.hit_sound[(int)PlayerNum::P2]) / "ka.ogg", "hitsound_kat_2p");
        spdlog::info("Loaded ogg hit sounds for 1P and 2P");
    }
}

void GameScreen::init_tja(fs::path song) {
    /*
    if song.suffix == '.osu':
        self.start_delay = 0
        self.parser = OsuParser(song)
        else: */
    parser = TJAParser(song, start_delay);
    /*
    if self.parser.metadata.bgmovie != Path() and self.parser.metadata.bgmovie.exists():
        self.movie = VideoPlayer(self.parser.metadata.bgmovie)
    else:
        self.movie = None
        */
    auto& titles = parser->metadata.title;
    const std::string& lang = global_data.config->general.language;

    global_data.session_data[(int)global_data.player_num].song_title = titles.count(lang) ? titles.at(lang) : titles.at("en");
    if (fs::exists(parser->metadata.wave) && !song_music.has_value()) {
        song_music = audio->load_music_stream(parser->metadata.wave, "song");
    }

    player_1.emplace(parser, global_data.player_num, global_data.session_data[(int)global_data.player_num].selected_difficulty, false, global_data.modifiers[(int)global_data.player_num]);
    start_ms = get_current_ms() - parser->metadata.offset*1000;
    //self.precise_start = time.time() - self.parser.metadata.offset
}

void GameScreen::start_song(double ms_from_start) {
    if (ms_from_start >= parser->metadata.offset*1000 + start_delay - (double)global_data.config->general.audio_offset && !song_started) {
        if (song_music.has_value()) {
            audio->play_music_stream(song_music.value(), "music");
            spdlog::info("Song started at {}", ms_from_start);
        }
        /*
        if self.movie is not None:
            self.movie.start(get_current_ms())
            self.movie.set_volume(0.0)
         */
        song_started = true;
    }
}

std::nullopt_t GameScreen::global_keys() {
    if (ray::IsKeyPressed(global_data.config->keys.restart_key)) {
        if (song_music.has_value()) {
            audio->stop_music_stream(song_music.value());
        }
        init_tja(global_data.session_data[(int)global_data.player_num].selected_song);
        audio->play_sound("restart", "sound");
        song_started = false;
    }

    if (ray::IsKeyPressed(global_data.config->keys.back_key)) {
        if (song_music.has_value()) {
            audio->stop_music_stream(song_music.value());
        }
        //return self.on_screen_end('SONG_SELECT')
    }

    if (ray::IsKeyPressed(global_data.config->keys.pause_key)) {
        //pause_song();
    }

    return std::nullopt;
}

void GameScreen::update_background(double current_time) {
    //if self.movie is not None:
        //self.movie.update()
    //else:
        //if len(self.player_1.current_bars) > 0:
            //self.bpm = self.player_1.bpm
        if (background.has_value()) background->update(current_time);
            //self.background.update(current_time, self.bpm, self.player_1.gauge, None)
}

std::optional<std::string> GameScreen::update() {
    Screen::update();  // Call parent implementation

    volatile double current_time = get_current_ms();
    //self.transition.update(current_time)
    if (!paused) {
        current_ms = current_time - start_ms;
    }
    //if self.transition.is_finished:
    start_song(current_ms);
        //else:
        //self.start_ms = current_time - self.parser.metadata.offset*1000
    update_background(current_time);

    if (player_1.has_value()) {
        player_1->update(current_ms, current_time, background);
    }
    song_info.update(current_time);
    /*
    self.result_transition.update(current_time)
    if self.result_transition.is_finished and not audio.is_sound_playing('result_transition'):
        logger.info("Result transition finished, moving to RESULT screen")
        return self.on_screen_end('RESULT')
    elif self.current_ms >= self.player_1.end_time:
        session_data = global_data.session_data[global_data.player_num]
        session_data.result_data.score, session_data.result_data.good, session_data.result_data.ok, session_data.result_data.bad, session_data.result_data.max_combo, session_data.result_data.total_drumroll = self.player_1.get_result_score()
        if self.player_1.gauge is not None:
            session_data.result_data.gauge_length = self.player_1.gauge.gauge_length
        if self.end_ms != 0:
            if current_time >= self.end_ms + 1000:
                if self.player_1.ending_anim is None:
                    self.write_score()
                    logger.info("Score written and ending animations spawned")
                    self.spawn_ending_anims()
            if current_time >= self.end_ms + 8533.34:
                if not self.result_transition.is_started:
                    self.result_transition.start()
                    audio.play_sound('result_transition', 'voice')
                    logger.info("Result transition started and voice played")
        else:
            self.end_ms = current_time
        */
    return global_keys();
}

void GameScreen::draw_overlay() {
    song_info.draw();
    /*
    if not self.transition.is_finished:
        self.transition.draw()
    if self.result_transition.is_started:
        self.result_transition.draw()
    self.allnet_indicator.draw()*/
}

void GameScreen::draw() {
    //if self.movie is not None:
        //self.movie.draw()
    //elif self.background is not None:
    if (background.has_value()) background->draw_back();
    if (player_1.has_value()) {
        player_1->draw(current_ms, mask_shader);
    }
    if (background.has_value()) background->draw_fore();
    draw_overlay();
}
