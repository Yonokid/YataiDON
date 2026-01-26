#pragma once

#include <set>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include "global_data.h"

inline double get_current_ms() {
    using namespace std::chrono;
    auto now = high_resolution_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
    return static_cast<double>(ms);
}

class BaseAnimation {
protected:
    double duration;
    double delay;
    double delay_saved;
    double start_ms;
    bool is_started;
    bool is_reversing;
    bool unlocked;
    bool loop;
    bool lock_input;

    double easeIn(double progress, const std::string& ease_type);

    double easeOut(double progress, const std::string& ease_type);

    double applyEasing(double progress, const std::optional<std::string>& ease_in_opt,
                      const std::optional<std::string>& ease_out_opt);

public:
    double attribute;
    bool is_finished;

    BaseAnimation(double duration, double delay = 0.0, bool loop = false, bool lock_input = false)
        : duration(duration), delay(delay), delay_saved(delay),
          start_ms(get_current_ms()), is_finished(false), is_started(false),
          is_reversing(false), unlocked(false), loop(loop),
          lock_input(lock_input), attribute(0) {}

    virtual ~BaseAnimation() = default;

    virtual void update(double current_time_ms);

    virtual void restart();

    void start();

    void pause();

    void unpause();

    virtual void reset();

    virtual std::unique_ptr<BaseAnimation> copy() const = 0;

    bool isFinished() const { return is_finished; }
    bool isStarted() const { return is_started; }
};

class FadeAnimation : public BaseAnimation {
private:
    double initial_opacity;
    double final_opacity;
    double initial_opacity_saved;
    double final_opacity_saved;
    std::optional<std::string> ease_in;
    std::optional<std::string> ease_out;
    std::optional<double> reverse_delay;
    std::optional<double> reverse_delay_saved;

public:
    FadeAnimation(double duration, double initial_opacity = 1.0, bool loop = false,
                  bool lock_input = false, double final_opacity = 0.0, double delay = 0.0,
                  std::optional<std::string> ease_in = std::nullopt,
                  std::optional<std::string> ease_out = std::nullopt,
                  std::optional<double> reverse_delay = std::nullopt)
        : BaseAnimation(duration, delay, loop, lock_input),
          initial_opacity(initial_opacity), final_opacity(final_opacity),
          initial_opacity_saved(initial_opacity), final_opacity_saved(final_opacity),
          ease_in(ease_in), ease_out(ease_out),
          reverse_delay(reverse_delay), reverse_delay_saved(reverse_delay) {
        attribute = initial_opacity;
    }

    void restart() override;

    void update(double current_time_ms) override;

    std::unique_ptr<BaseAnimation> copy() const override;
};

class MoveAnimation : public BaseAnimation {
private:
    int total_distance;
    int start_position;
    int total_distance_saved;
    int start_position_saved;
    std::optional<std::string> ease_in;
    std::optional<std::string> ease_out;
    std::optional<double> reverse_delay;
    std::optional<double> reverse_delay_saved;

public:
    MoveAnimation(double duration, int total_distance = 0, bool loop = false,
                  bool lock_input = false, int start_position = 0, double delay = 0.0,
                  std::optional<double> reverse_delay = std::nullopt,
                  std::optional<std::string> ease_in = std::nullopt,
                  std::optional<std::string> ease_out = std::nullopt)
        : BaseAnimation(duration, delay, loop, lock_input),
          total_distance(total_distance), start_position(start_position),
          total_distance_saved(total_distance), start_position_saved(start_position),
          ease_in(ease_in), ease_out(ease_out),
          reverse_delay(reverse_delay), reverse_delay_saved(reverse_delay) {
        attribute = start_position;
    }

    void restart() override;

    void update(double current_time_ms) override;

    std::unique_ptr<BaseAnimation> copy() const override;
};

class TextureChangeAnimation : public BaseAnimation {
private:
    struct TextureFrame {
        double start;
        double end;
        int index;
    };
    std::vector<TextureFrame> textures;

public:
    TextureChangeAnimation(double duration, const std::vector<std::tuple<double, double, int>>& textures,
                          bool loop = false, bool lock_input = false, double delay = 0.0)
        : BaseAnimation(duration, delay, loop, lock_input) {
        for (const auto& [start, end, index] : textures) {
            this->textures.push_back({start, end, index});
        }
        if (!this->textures.empty()) {
            attribute = this->textures[0].index;
        }
    }

    void reset() override;

    void update(double current_time_ms) override;

    std::unique_ptr<BaseAnimation> copy() const override;
};

class TextStretchAnimation : public BaseAnimation {
public:
    TextStretchAnimation(double duration, double delay = 0.0, bool loop = false, bool lock_input = false)
        : BaseAnimation(duration, delay, loop, lock_input) {}

    void update(double current_time_ms) override;

    std::unique_ptr<BaseAnimation> copy() const override;
};

class TextureResizeAnimation : public BaseAnimation {
private:
    double initial_size;
    double final_size;
    double initial_size_saved;
    double final_size_saved;
    std::optional<std::string> ease_in;
    std::optional<std::string> ease_out;
    std::optional<double> reverse_delay;
    std::optional<double> reverse_delay_saved;

public:
    TextureResizeAnimation(double duration, double initial_size = 1.0, bool loop = false,
                          bool lock_input = false, double final_size = 0.0, double delay = 0.0,
                          std::optional<double> reverse_delay = std::nullopt,
                          std::optional<std::string> ease_in = std::nullopt,
                          std::optional<std::string> ease_out = std::nullopt)
        : BaseAnimation(duration, delay, loop, lock_input),
          initial_size(initial_size), final_size(final_size),
          initial_size_saved(initial_size), final_size_saved(final_size),
          ease_in(ease_in), ease_out(ease_out),
          reverse_delay(reverse_delay), reverse_delay_saved(reverse_delay) {
        attribute = initial_size;
    }

    void restart() override;

    void update(double current_time_ms) override;

    std::unique_ptr<BaseAnimation> copy() const override;
};

using namespace rapidjson;

class AnimationParser {
private:
    std::map<int, Value> raw_anims;
    Document::AllocatorType* allocator;

    // Helper to get a value from a JSON object with type checking
    template<typename T>
    std::optional<T> getOptional(const Value& obj, const char* key);

    Value resolveValue(const Value& ref_obj, std::set<int>& visited);

    Value findRefs(int anim_id, std::set<int>& visited);

    std::unique_ptr<BaseAnimation> createAnimation(const Value& anim_obj);

public:
    std::map<int, std::unique_ptr<BaseAnimation>> parse_animations(const Value& animation_json);

    std::map<int, std::unique_ptr<BaseAnimation>> parseAnimationsFromString(const std::string& json_str);
};
