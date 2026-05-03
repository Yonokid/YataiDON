#include "timer.h"
#include "../../libs/global_data.h"
#include "../../libs/texture.h"
#include "../../libs/audio_engine.h"

Timer::Timer(int time, double current_time_ms, std::function<void()> confirm_func)
    : time(time), last_time(current_time_ms), counter(std::to_string(time)),
      confirm_func(confirm_func), is_finished(false),
      is_frozen(global_data.config->general.timer_frozen) {
    num_resize = (TextureResizeAnimation*)global_tex.get_animation(9);
    highlight_resize = (TextureResizeAnimation*)global_tex.get_animation(10);
    highlight_fade = (FadeAnimation*)global_tex.get_animation(11);
}

void Timer::update(double current_ms) {
    if (time == 0 && !is_finished && !audio->is_sound_playing("voice_timer_0")) {
        is_finished = true;
        confirm_func();
    }
    num_resize->update(current_ms);
    highlight_resize->update(current_ms);
    highlight_fade->update(current_ms);
    if (is_frozen) return;
    if (current_ms >= last_time + 1000 && time > 0) {
        time--;
        last_time = current_ms;
        counter = std::to_string(time);
        if (time < 10) {
            audio->play_sound("timer_blip", "sound");
            num_resize->start();
            highlight_fade->start();
            highlight_resize->start();
        }
        if (time == 10) {
            audio->play_sound("voice_timer_10", "voice");
        } else if (time == 5) {
            audio->play_sound("voice_timer_5", "voice");
        } else if (time == 0) {
            audio->play_sound("voice_timer_0", "voice");
        }
    }
}

void Timer::draw(float x, float y) {
    std::string counter_name;
    if (time < 10) {
        global_tex.draw_texture(TIMER::BG_RED);
        counter_name = "counter_white";
        global_tex.draw_texture(TIMER::HIGHLIGHT, {.scale=(float)highlight_resize->attribute, .center=true, .fade=highlight_fade->attribute});
    } else {
        global_tex.draw_texture(TIMER::BG);
        counter_name = "counter_black";
    }
    float margin = global_tex.skin_config[SC::TIMER_TEXT_MARGIN].x;
    float total_width = counter.size() * margin;
    for (int i = 0; i < (int)counter.size(); i++) {
        int digit = counter[i] - '0';
        global_tex.draw_texture(tex_id_map.at("timer/" + (counter_name)), {.frame=digit, .scale=(float)num_resize->attribute, .center=true, .x=-(total_width/2) + (i * margin)});
    }
}
