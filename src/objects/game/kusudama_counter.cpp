#include "kusudama_counter.h"

KusudamaCounter::KusudamaCounter(int total)
    : balloon_total(total), balloon_count(0), is_popped(false) {
    move_down = (MoveAnimation*)tex.get_animation(11);
    move_up = (MoveAnimation*)tex.get_animation(12);
    renda_move_up = (MoveAnimation*)tex.get_animation(13);
    renda_move_down = (MoveAnimation*)tex.get_animation(18);
    renda_fade_in = (FadeAnimation*)tex.get_animation(14);
    renda_fade_out = (FadeAnimation*)tex.get_animation(20);
    stretch = (TextStretchAnimation*)tex.get_animation(15);
    breathing = (MoveAnimation*)tex.get_animation(16);
    renda_breathe = (MoveAnimation*)tex.get_animation(17);
    open = (TextureChangeAnimation*)tex.get_animation(19);
    fade_out = (FadeAnimation*)tex.get_animation(21);

    move_down->start();
    move_up->start();
    renda_move_up->start();
    renda_move_down->start();
    renda_fade_in->start();

    open->reset();
    renda_fade_out->reset();
    fade_out->reset();
}

void KusudamaCounter::update_count(int count) {
    if (balloon_count != count) {
        balloon_count = count;
        stretch->start();
        breathing->start();
    }
}

void KusudamaCounter::update(double current_ms, bool popped) {
    if (popped && !is_popped) {
        is_popped = true;
        open->start();
        renda_fade_out->start();
        fade_out->start();
        fade_out->start();
    }
    move_down->update(current_ms);
    move_up->update(current_ms);
    renda_move_up->update(current_ms);
    renda_move_down->update(current_ms);
    renda_fade_in->update(current_ms);
    renda_fade_out->update(current_ms);
    fade_out->update(current_ms);
    stretch->update(current_ms);
    breathing->update(current_ms);
    renda_breathe->update(current_ms);
    open->update(current_ms);
}

void KusudamaCounter::draw() {
    float y = move_down->attribute - move_up->attribute;
    float renda_y = -renda_move_up->attribute + renda_move_down->attribute + renda_breathe->attribute;
    tex.draw_texture("kusudama", "kusudama", {.frame=(int)open->attribute, .scale=(float)breathing->attribute, .center=true, .y=y, .fade=fade_out->attribute});
    tex.draw_texture("kusudama", "renda", {.y=renda_y, .fade=std::min(renda_fade_in->attribute, renda_fade_out->attribute)});

    if (move_up->is_finished && !is_popped) {
        int int_counter = std::max(0, balloon_total - balloon_count);
        if (int_counter == 0) return;
        std::string counter = std::to_string(int_counter);
        int total_width = counter.length() * tex.skin_config["kusudama_counter_margin"].x;
        for (int i = 0; i < counter.size(); i++) {
            char digit = counter[i];
            tex.draw_texture("kusudama", "counter", {.frame=digit - '0', .x=-(total_width / 2.0f) + (i * tex.skin_config["kusudama_counter_margin"].x), .y=(float)-stretch->attribute, .y2=(float)stretch->attribute});
        }
    }
}

bool KusudamaCounter::is_finished() const {
    return fade_out->is_finished;
}
