#pragma once

#include <map>
#include <memory>
#include <optional>
#include <rapidjson/document.h>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "../time.h"

enum class EaseType {
    Quadratic,
    Cubic,
    Exponential
};

bool is_input_locked();
void reset_input_lock();
void set_modal_input_lock(bool locked);

class BaseAnimation {
protected:
    double delay;
    double delay_saved;
    double start_ms;
    double paused_at_ms{0.0};
    bool unlocked{true};
    bool loop;
    bool lock_input;

    double easeIn(double progress, EaseType ease_type);

    double easeOut(double progress, EaseType ease_type);

    double applyEasing(double progress, const std::optional<EaseType>& ease_in_opt,
                       const std::optional<EaseType>& ease_out_opt);

public:
    double attribute{0};
    double duration;
    bool is_finished{false};
    bool is_started{false};
    bool is_reversing{false};

    BaseAnimation(double duration, double delay = 0.0, bool loop = false, bool lock_input = false);

    virtual ~BaseAnimation();

    virtual void update(double current_time_ms);

    virtual void restart();

    void start();
    void unlock_input();

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
    std::optional<EaseType> ease_in;
    std::optional<EaseType> ease_out;
    std::optional<double> reverse_delay;
    std::optional<double> reverse_delay_saved;

public:
    FadeAnimation(double duration, double initial_opacity = 1.0, bool loop = false, bool lock_input = false,
                  double final_opacity = 0.0, double delay = 0.0, std::optional<EaseType> ease_in = std::nullopt,
                  std::optional<EaseType> ease_out = std::nullopt, std::optional<double> reverse_delay = std::nullopt);

    void restart() override;

    void update(double current_time_ms) override;

    std::unique_ptr<BaseAnimation> copy() const override;
};

class MoveAnimation : public BaseAnimation {
public:
    struct Waypoint {
        double at;
        int value;
        // Eases the segment ending at this waypoint (previous point -> this one).
        // The animation's own ease_in/ease_out instead ease the final segment
        // (last waypoint -> the implicit end point at duration/total_distance).
        std::optional<EaseType> ease_in;
        std::optional<EaseType> ease_out;
    };

private:
    int total_distance;
    int total_distance_saved;
    int start_position_saved;
    std::optional<EaseType> ease_in;
    std::optional<EaseType> ease_out;
    std::optional<double> reverse_delay;
    std::optional<double> reverse_delay_saved;
    std::vector<Waypoint> waypoints;

public:
    int start_position;
    MoveAnimation(double duration, int total_distance = 0, bool loop = false, bool lock_input = false,
                  int start_position = 0, double delay = 0.0, std::optional<double> reverse_delay = std::nullopt,
                  std::optional<EaseType> ease_in = std::nullopt, std::optional<EaseType> ease_out = std::nullopt,
                  std::vector<Waypoint> waypoints = {});

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
    TextureChangeAnimation(double duration, const std::vector<std::tuple<double, double, int>>& keyframes,
                           bool loop = false, bool lock_input = false, double delay = 0.0);

    void reset() override;

    void update(double current_time_ms) override;

    std::unique_ptr<BaseAnimation> copy() const override;
};

class TextStretchAnimation : public BaseAnimation {
public:
    TextStretchAnimation(double duration, double delay = 0.0, bool loop = false, bool lock_input = false);

    void update(double current_time_ms) override;

    std::unique_ptr<BaseAnimation> copy() const override;
};

class TextureResizeAnimation : public BaseAnimation {
private:
    double initial_size;
    double final_size;
    double initial_size_saved;
    double final_size_saved;
    std::optional<EaseType> ease_in;
    std::optional<EaseType> ease_out;
    std::optional<double> reverse_delay;
    std::optional<double> reverse_delay_saved;

public:
    TextureResizeAnimation(double duration, double initial_size = 1.0, bool loop = false, bool lock_input = false,
                           double final_size = 0.0, double delay = 0.0,
                           std::optional<double> reverse_delay = std::nullopt,
                           std::optional<EaseType> ease_in = std::nullopt,
                           std::optional<EaseType> ease_out = std::nullopt);

    void restart() override;

    void update(double current_time_ms) override;

    std::unique_ptr<BaseAnimation> copy() const override;
};

using namespace rapidjson;

class AnimationParser {
private:
    std::map<int, Value> raw_anims;
    Document::AllocatorType* allocator;

    Value resolveValue(const Value& ref_obj, std::set<int>& visited);

    Value findRefs(int anim_id, std::set<int>& visited);

    std::unique_ptr<BaseAnimation> createAnimation(const Value& anim_obj);

public:
    std::unordered_map<int, std::unique_ptr<BaseAnimation>> parse_animations(const Value& animation_json);

    std::unordered_map<int, std::unique_ptr<BaseAnimation>> parseAnimationsFromString(const std::string& json_str);
};
