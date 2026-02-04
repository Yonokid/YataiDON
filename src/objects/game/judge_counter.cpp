#include "judge_counter.h"
#include <cmath>

JudgeCounter::JudgeCounter()
    : good(0), ok(0), bad(0), drumrolls(0) {
    orange = ray::Color{253, 161, 0, 255};
    white = ray::WHITE;
}

void JudgeCounter::update(int good, int ok, int bad, int drumrolls) {
    this->good = good;
    this->ok = ok;
    this->bad = bad;
    this->drumrolls = drumrolls;
}

void JudgeCounter::draw_counter(float counter, float x, float y, float margin, ray::Color color) {
    std::string counter_str = std::to_string((int)std::round(counter));
    int counter_len = counter_str.length();

    for (int i = 0; i < counter_str.length(); i++) {
        tex.draw_texture("judge_counter", "counter", {
            .color = color,
            .frame = counter_str[i] - '0',
            .x = x - (counter_len - i) * margin,
            .y = y
        });
    }
}

void JudgeCounter::draw() {
    tex.draw_texture("judge_counter", "bg");
    tex.draw_texture("judge_counter", "total_percent");
    tex.draw_texture("judge_counter", "judgments");
    tex.draw_texture("judge_counter", "drumrolls");

    for (int i = 0; i < 4; i++) {
        tex.draw_texture("judge_counter", "percent", {
            .color = orange,
            .index = i
        });
    }

    int total_notes = good + ok + bad;
    if (total_notes == 0) {
        total_notes = 1;
    }

    float margin = tex.skin_config["judge_counter_margin"].x;

    draw_counter(good / (float)total_notes * 100,
                 tex.skin_config["judge_counter_1"].x,
                 tex.skin_config["judge_counter_1"].y,
                 margin, orange);

    draw_counter(ok / (float)total_notes * 100,
                 tex.skin_config["judge_counter_1"].x,
                 tex.skin_config["judge_counter_3"].y,
                 margin, orange);

    draw_counter(bad / (float)total_notes * 100,
                 tex.skin_config["judge_counter_1"].x,
                 tex.skin_config["judge_counter_4"].x,
                 margin, orange);

    draw_counter((good + ok) / (float)total_notes * 100,
                 tex.skin_config["judge_counter_3"].x,
                 tex.skin_config["judge_counter_4"].y,
                 margin, orange);

    draw_counter(good,
                 tex.skin_config["judge_counter_2"].x,
                 tex.skin_config["judge_counter_1"].y,
                 margin, white);

    draw_counter(ok,
                 tex.skin_config["judge_counter_2"].x,
                 tex.skin_config["judge_counter_3"].y,
                 margin, white);

    draw_counter(bad,
                 tex.skin_config["judge_counter_2"].x,
                 tex.skin_config["judge_counter_4"].x,
                 margin, white);

    draw_counter(drumrolls,
                 tex.skin_config["judge_counter_2"].x,
                 tex.skin_config["judge_counter_4"].width,
                 margin, white);
}
