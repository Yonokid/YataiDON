#pragma once

#include <chrono>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <functional>
#include <stdexcept>
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
    bool is_finished;
    bool is_started;
    bool is_reversing;
    bool unlocked;
    bool loop;
    bool lock_input;

    double easeIn(double progress, const std::string& ease_type) {
        if (ease_type == "quadratic") {
            return progress * progress;
        } else if (ease_type == "cubic") {
            return progress * progress * progress;
        } else if (ease_type == "exponential") {
            return progress == 0 ? 0 : std::pow(2, 10 * (progress - 1));
        }
        return progress;
    }

    double easeOut(double progress, const std::string& ease_type) {
        if (ease_type == "quadratic") {
            return progress * (2 - progress);
        } else if (ease_type == "cubic") {
            return 1 - std::pow(1 - progress, 3);
        } else if (ease_type == "exponential") {
            return progress == 1 ? 1 : 1 - std::pow(2, -10 * progress);
        }
        return progress;
    }

    double applyEasing(double progress, const std::optional<std::string>& ease_in_opt,
                      const std::optional<std::string>& ease_out_opt) {
        if (ease_in_opt.has_value()) {
            return easeIn(progress, ease_in_opt.value());
        } else if (ease_out_opt.has_value()) {
            return easeOut(progress, ease_out_opt.value());
        }
        return progress;
    }

public:
    double attribute;

    BaseAnimation(double duration, double delay = 0.0, bool loop = false, bool lock_input = false)
        : duration(duration), delay(delay), delay_saved(delay),
          start_ms(get_current_ms()), is_finished(false), is_started(false),
          is_reversing(false), unlocked(false), loop(loop),
          lock_input(lock_input), attribute(0) {}

    virtual ~BaseAnimation() = default;

    virtual void update(double current_time_ms) {
        if (loop && is_finished) {
            restart();
        }
        if (lock_input && is_finished && !unlocked) {
            unlocked = true;
            global_data.input_locked--;
        }
    }

    virtual void restart() {
        start_ms = get_current_ms();
        is_finished = false;
        delay = delay_saved;
        unlocked = false;
        if (lock_input) {
            global_data.input_locked++;
        }
    }

    void start() {
        is_started = true;
        restart();
    }

    void pause() {
        is_started = false;
        if (lock_input) {
            global_data.input_locked--;
        }
    }

    void unpause() {
        is_started = true;
        if (lock_input) {
            global_data.input_locked++;
        }
    }

    virtual void reset() {
        restart();
        pause();
    }

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

    void restart() override {
        BaseAnimation::restart();
        reverse_delay = reverse_delay_saved;
        initial_opacity = initial_opacity_saved;
        final_opacity = final_opacity_saved;
        attribute = initial_opacity;
    }

    void update(double current_time_ms) override {
        if (!is_started) return;
        BaseAnimation::update(current_time_ms);

        double elapsed_time = current_time_ms - start_ms;

        if (elapsed_time <= delay) {
            attribute = initial_opacity;
        } else if (elapsed_time >= delay + duration) {
            attribute = final_opacity;

            if (reverse_delay.has_value()) {
                start_ms = current_time_ms;
                delay = reverse_delay.value();
                std::swap(initial_opacity, final_opacity);
                reverse_delay = std::nullopt;
                is_reversing = true;
            } else {
                is_finished = true;
            }
        } else {
            double animation_time = elapsed_time - delay;
            double progress = animation_time / duration;
            progress = std::max(0.0, std::min(1.0, progress));
            progress = applyEasing(progress, ease_in, ease_out);
            attribute = initial_opacity + progress * (final_opacity - initial_opacity);
        }
    }

    std::unique_ptr<BaseAnimation> copy() const override {
        return std::make_unique<FadeAnimation>(
            duration, initial_opacity_saved, loop, lock_input,
            final_opacity_saved, delay_saved, ease_in, ease_out, reverse_delay_saved
        );
    }
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

    void restart() override {
        BaseAnimation::restart();
        reverse_delay = reverse_delay_saved;
        total_distance = total_distance_saved;
        start_position = start_position_saved;
        attribute = start_position;
    }

    void update(double current_time_ms) override {
        if (!is_started) return;
        BaseAnimation::update(current_time_ms);

        double elapsed_time = current_time_ms - start_ms;

        if (elapsed_time < delay) {
            attribute = start_position;
        } else if (elapsed_time >= delay + duration) {
            attribute = start_position + total_distance;
            if (reverse_delay.has_value()) {
                start_ms = current_time_ms;
                delay = reverse_delay.value();
                start_position = start_position + total_distance;
                total_distance = -total_distance;
                reverse_delay = std::nullopt;
            } else {
                is_finished = true;
            }
        } else {
            double progress = (elapsed_time - delay) / duration;
            progress = applyEasing(progress, ease_in, ease_out);
            attribute = start_position + (total_distance * progress);
        }
    }

    std::unique_ptr<BaseAnimation> copy() const override {
        return std::make_unique<MoveAnimation>(
            duration, total_distance_saved, loop, lock_input,
            start_position_saved, delay_saved, reverse_delay_saved, ease_in, ease_out
        );
    }
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

    void reset() override {
        BaseAnimation::reset();
        if (!textures.empty()) {
            attribute = textures[0].index;
        }
    }

    void update(double current_time_ms) override {
        if (!is_started) return;
        BaseAnimation::update(current_time_ms);

        double elapsed_time = current_time_ms - start_ms;
        if (elapsed_time < delay) return;

        double animation_time = elapsed_time - delay;
        if (animation_time <= duration) {
            for (const auto& frame : textures) {
                if (frame.start < animation_time && animation_time <= frame.end) {
                    attribute = frame.index;
                }
            }
        } else {
            is_finished = true;
        }
    }

    std::unique_ptr<BaseAnimation> copy() const override {
        std::vector<std::tuple<double, double, int>> tex_tuples;
        for (const auto& frame : textures) {
            tex_tuples.emplace_back(frame.start, frame.end, frame.index);
        }
        return std::make_unique<TextureChangeAnimation>(
            duration, tex_tuples, loop, lock_input, delay_saved
        );
    }
};

class TextStretchAnimation : public BaseAnimation {
public:
    TextStretchAnimation(double duration, double delay = 0.0, bool loop = false, bool lock_input = false)
        : BaseAnimation(duration, delay, loop, lock_input) {}

    void update(double current_time_ms) override {
        if (!is_started) return;
        BaseAnimation::update(current_time_ms);

        double elapsed_time = current_time_ms - start_ms;
        if (elapsed_time < delay) return;

        double animation_time = elapsed_time - delay;
        if (animation_time <= duration) {
            attribute = 2 + 5 * (static_cast<int>(animation_time) / 25);
        } else if (animation_time <= duration + 116) {
            int frame_time = static_cast<int>((animation_time - duration) / 16.57);
            attribute = 2 + 10 - (2 * (frame_time + 1));
        } else {
            attribute = 0;
            is_finished = true;
        }
    }

    std::unique_ptr<BaseAnimation> copy() const override {
        return std::make_unique<TextStretchAnimation>(duration, delay_saved, loop, lock_input);
    }
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

    void restart() override {
        BaseAnimation::restart();
        reverse_delay = reverse_delay_saved;
        initial_size = initial_size_saved;
        final_size = final_size_saved;
        attribute = initial_size;
    }

    void update(double current_time_ms) override {
        if (!is_started) return;
        else {
            is_started = !is_finished;
        }
        BaseAnimation::update(current_time_ms);

        double elapsed_time = current_time_ms - start_ms;

        if (elapsed_time <= delay) {
            attribute = initial_size;
        } else if (elapsed_time >= delay + duration) {
            attribute = final_size;

            if (reverse_delay.has_value()) {
                start_ms = current_time_ms;
                delay = reverse_delay.value();
                std::swap(initial_size, final_size);
                reverse_delay = std::nullopt;
            } else {
                is_finished = true;
            }
        } else {
            double animation_time = elapsed_time - delay;
            double progress = animation_time / duration;
            progress = applyEasing(progress, ease_in, ease_out);
            attribute = initial_size + ((final_size - initial_size) * progress);
        }
    }

    std::unique_ptr<BaseAnimation> copy() const override {
        return std::make_unique<TextureResizeAnimation>(
            duration, initial_size_saved, loop, lock_input,
            final_size_saved, delay_saved, reverse_delay_saved, ease_in, ease_out
        );
    }
};

using namespace rapidjson;

class AnimationParser {
private:
    std::map<std::string, Value> raw_anims;
    Document::AllocatorType* allocator;

    // Helper to get a value from a JSON object with type checking
    template<typename T>
    std::optional<T> getOptional(const Value& obj, const char* key) {
        if (!obj.HasMember(key)) return std::nullopt;

        if constexpr (std::is_same_v<T, double>) {
            if (obj[key].IsDouble()) return obj[key].GetDouble();
            if (obj[key].IsInt()) return static_cast<double>(obj[key].GetInt());
        } else if constexpr (std::is_same_v<T, int>) {
            if (obj[key].IsInt()) return obj[key].GetInt();
        } else if constexpr (std::is_same_v<T, bool>) {
            if (obj[key].IsBool()) return obj[key].GetBool();
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (obj[key].IsString()) return std::string(obj[key].GetString());
        }
        return std::nullopt;
    }

    Value resolveValue(const Value& ref_obj, std::set<std::string>& visited) {
        if (!ref_obj.HasMember("property")) {
            throw std::runtime_error("Reference requires 'property' field");
        }

        std::string ref_id;
        if (ref_obj["reference_id"].IsString()) {
            ref_id = ref_obj["reference_id"].GetString();
        } else if (ref_obj["reference_id"].IsInt()) {
            ref_id = std::to_string(ref_obj["reference_id"].GetInt());
        } else {
            throw std::runtime_error("reference_id must be string or int");
        }
        std::string ref_property = ref_obj["property"].GetString();

        if (raw_anims.find(ref_id) == raw_anims.end()) {
            throw std::runtime_error("Referenced animation " + ref_id + " not found");
        }

        Value resolved_anim = findRefs(ref_id, visited);

        if (!resolved_anim.HasMember(ref_property.c_str())) {
            throw std::runtime_error("Property '" + ref_property + "' not found in animation " + ref_id);
        }

        Value base_value;
        base_value.CopyFrom(resolved_anim[ref_property.c_str()], *allocator);

        if (ref_obj.HasMember("init_val")) {
            const Value& init_val_obj = ref_obj["init_val"];

            if (init_val_obj.IsObject() && init_val_obj.HasMember("reference_id")) {
                Value init_val = resolveValue(init_val_obj, visited);

                // Handle numeric addition with type coercion
                double base_num = base_value.IsDouble() ? base_value.GetDouble() :
                                 (base_value.IsInt() ? static_cast<double>(base_value.GetInt()) : 0.0);
                double init_num = init_val.IsDouble() ? init_val.GetDouble() :
                                 (init_val.IsInt() ? static_cast<double>(init_val.GetInt()) : 0.0);

                if (base_value.IsDouble() || init_val.IsDouble()) {
                    base_value.SetDouble(base_num + init_num);
                } else {
                    base_value.SetInt(static_cast<int>(base_num + init_num));
                }
            } else {
                // Handle numeric addition with type coercion
                double base_num = base_value.IsDouble() ? base_value.GetDouble() :
                                 (base_value.IsInt() ? static_cast<double>(base_value.GetInt()) : 0.0);
                double init_num = init_val_obj.IsDouble() ? init_val_obj.GetDouble() :
                                 (init_val_obj.IsInt() ? static_cast<double>(init_val_obj.GetInt()) : 0.0);

                if (base_value.IsDouble() || init_val_obj.IsDouble()) {
                    base_value.SetDouble(base_num + init_num);
                } else {
                    base_value.SetInt(static_cast<int>(base_num + init_num));
                }
            }
        }

        return base_value;
    }

    Value findRefs(const std::string& anim_id, std::set<std::string>& visited) {
        if (visited.find(anim_id) != visited.end()) {
            throw std::runtime_error("Circular reference detected involving animation " + anim_id);
        }

        visited.insert(anim_id);

        Value animation;
        animation.CopyFrom(raw_anims[anim_id], *allocator);

        for (auto it = animation.MemberBegin(); it != animation.MemberEnd(); ++it) {
            if (it->value.IsObject() && it->value.HasMember("reference_id")) {
                std::set<std::string> visited_copy = visited;
                Value resolved = resolveValue(it->value, visited_copy);
                it->value.CopyFrom(resolved, *allocator);
            }
        }

        visited.erase(anim_id);
        return animation;
    }

    std::unique_ptr<BaseAnimation> createAnimation(const Value& anim_obj) {
        std::string type = anim_obj["type"].GetString();
        double duration = anim_obj["duration"].GetDouble();

        auto get_double = [&](const char* key, double def) {
            return anim_obj.HasMember(key) && anim_obj[key].IsDouble() ? anim_obj[key].GetDouble() : def;
        };

        auto get_int = [&](const char* key, int def) {
            return anim_obj.HasMember(key) && anim_obj[key].IsInt() ? anim_obj[key].GetInt() : def;
        };

        auto get_bool = [&](const char* key, bool def) {
            return anim_obj.HasMember(key) && anim_obj[key].IsBool() ? anim_obj[key].GetBool() : def;
        };

        auto get_string_opt = [&](const char* key) -> std::optional<std::string> {
            if (anim_obj.HasMember(key) && anim_obj[key].IsString()) {
                return std::string(anim_obj[key].GetString());
            }
            return std::nullopt;
        };

        auto get_double_opt = [&](const char* key) -> std::optional<double> {
            if (anim_obj.HasMember(key) && anim_obj[key].IsDouble()) {
                return anim_obj[key].GetDouble();
            }
            return std::nullopt;
        };

        double delay = get_double("delay", 0.0);
        bool loop = get_bool("loop", false);
        bool lock_input = get_bool("lock_input", false);

        if (type == "fade") {
            return std::make_unique<FadeAnimation>(
                duration,
                get_double("initial_opacity", 1.0),
                loop,
                lock_input,
                get_double("final_opacity", 0.0),
                delay,
                get_string_opt("ease_in"),
                get_string_opt("ease_out"),
                get_double_opt("reverse_delay")
            );
        } else if (type == "move") {
            return std::make_unique<MoveAnimation>(
                duration,
                get_int("total_distance", 0),
                loop,
                lock_input,
                get_int("start_position", 0),
                delay,
                get_double_opt("reverse_delay"),
                get_string_opt("ease_in"),
                get_string_opt("ease_out")
            );
        } else if (type == "texture_change") {
            std::vector<std::tuple<double, double, int>> textures;
            if (anim_obj.HasMember("textures") && anim_obj["textures"].IsArray()) {
                const Value& tex_array = anim_obj["textures"];
                for (SizeType i = 0; i < tex_array.Size(); i++) {
                    if (tex_array[i].IsArray() && tex_array[i].Size() == 3) {
                        double start = tex_array[i][0].GetDouble();
                        double end = tex_array[i][1].GetDouble();
                        int index = tex_array[i][2].GetInt();
                        textures.emplace_back(start, end, index);
                    }
                }
            }
            return std::make_unique<TextureChangeAnimation>(
                duration, textures, loop, lock_input, delay
            );
        } else if (type == "text_stretch") {
            return std::make_unique<TextStretchAnimation>(
                duration, delay, loop, lock_input
            );
        } else if (type == "texture_resize") {
            return std::make_unique<TextureResizeAnimation>(
                duration,
                get_double("initial_size", 1.0),
                loop,
                lock_input,
                get_double("final_size", 0.0),
                delay,
                get_double_opt("reverse_delay"),
                get_string_opt("ease_in"),
                get_string_opt("ease_out")
            );
        } else {
            throw std::runtime_error("Unknown animation type: " + type);
        }
    }

public:
    std::map<std::string, std::unique_ptr<BaseAnimation>> parse_animations(const Value& animation_json) {
        if (!animation_json.IsArray()) {
            throw std::runtime_error("Animation JSON must be an array");
        }

        Document temp_doc;
        allocator = &temp_doc.GetAllocator();
        raw_anims.clear();

        // First pass: collect all animations
        for (SizeType i = 0; i < animation_json.Size(); i++) {
            const Value& item = animation_json[i];

            if (!item.HasMember("id")) {
                throw std::runtime_error("Animation requires id");
            }
            if (!item.HasMember("type")) {
                throw std::runtime_error("Animation requires type");
            }
            std::string id = std::to_string(item["id"].GetInt());
            Value item_copy;
            item_copy.CopyFrom(item, *allocator);
            raw_anims[id] = std::move(item_copy);
        }

        std::map<std::string, std::unique_ptr<BaseAnimation>> anim_dict;

        for (auto& [id, _] : raw_anims) {
            std::set<std::string> visited;
            Value absolute_anim = findRefs(id, visited);

            auto anim = createAnimation(absolute_anim);
            anim_dict[id] = std::move(anim);
        }

        return anim_dict;
    }

    std::map<std::string, std::unique_ptr<BaseAnimation>> parseAnimationsFromString(const std::string& json_str) {
        Document doc;
        doc.Parse(json_str.c_str());

        if (doc.HasParseError()) {
            throw std::runtime_error(
                std::string("JSON parse error: ") +
                GetParseError_En(doc.GetParseError()) +
                " at offset " + std::to_string(doc.GetErrorOffset())
            );
        }

        return parse_animations(doc);
    }
};
