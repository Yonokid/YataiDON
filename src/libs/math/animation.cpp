#include "animation.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <spdlog/spdlog.h>

#include "rapidjson/error/en.h"

using std::runtime_error;

namespace {
    std::atomic<int> input_lock_count{0};

    void release_input_lock() {
        int prev = input_lock_count.fetch_sub(1, std::memory_order_relaxed);
        if (prev <= 0) {
            input_lock_count.store(0, std::memory_order_relaxed);
        }
    }
} // namespace

bool is_input_locked() {
    return input_lock_count.load(std::memory_order_relaxed) > 0;
}

void reset_input_lock() {
    input_lock_count.store(0, std::memory_order_relaxed);
}

BaseAnimation::BaseAnimation(double duration, double delay, bool loop, bool lock_input)
    : duration(duration), delay(delay), delay_saved(delay), start_ms(get_current_ms()), loop(loop),
      lock_input(lock_input) {
    if (loop) {
        is_started = true;
        restart();
    }
}

BaseAnimation::~BaseAnimation() {
    if (lock_input && !unlocked) {
        release_input_lock();
    }
}

double BaseAnimation::easeIn(double progress, EaseType ease_type) {
    switch (ease_type) {
    case EaseType::Quadratic:
        return progress * progress;
    case EaseType::Cubic:
        return progress * progress * progress;
    case EaseType::Exponential:
        return progress == 0 ? 0 : std::pow(2, 10 * (progress - 1));
    }
    return progress;
}

double BaseAnimation::easeOut(double progress, EaseType ease_type) {
    switch (ease_type) {
    case EaseType::Quadratic:
        return progress * (2 - progress);
    case EaseType::Cubic:
        return 1 - std::pow(1 - progress, 3);
    case EaseType::Exponential:
        return progress == 1 ? 1 : 1 - std::pow(2, -10 * progress);
    }
    return progress;
}

double BaseAnimation::applyEasing(double progress, const std::optional<EaseType>& ease_in_opt,
                                  const std::optional<EaseType>& ease_out_opt) {
    if (ease_in_opt.has_value()) {
        return easeIn(progress, ease_in_opt.value());
    }

    if (ease_out_opt.has_value()) {
        return easeOut(progress, ease_out_opt.value());
    }
    return progress;
}

void BaseAnimation::update(double current_time_ms) {
    if (lock_input && is_finished && !unlocked) {
        unlocked = true;
        release_input_lock();
    }
    if (loop && is_finished) {
        restart();
    }
}

void BaseAnimation::restart() {
    start_ms = get_current_ms();
    is_finished = false;
    is_reversing = false;
    delay = delay_saved;
    if (is_started) {
        if (lock_input && unlocked) {
            input_lock_count++;
        }
        unlocked = false;
    }
}

void BaseAnimation::start() {
    is_started = true;
    restart();
}

void BaseAnimation::unlock_input() {
    if (lock_input && !unlocked) {
        unlocked = true;
        release_input_lock();
    }
}

void BaseAnimation::pause() {
    if (!is_started) {
        return;
    }
    is_started = false;
    paused_at_ms = get_current_ms();
    if (lock_input && !unlocked) {
        unlocked = true;
        release_input_lock();
    }
}

void BaseAnimation::unpause() {
    if (is_started) {
        return;
    }
    is_started = true;
    start_ms += get_current_ms() - paused_at_ms;
    if (lock_input && unlocked) {
        unlocked = false;
        input_lock_count++;
    }
}

void BaseAnimation::reset() {
    restart();
    pause();
}

FadeAnimation::FadeAnimation(double duration, double initial_opacity, bool loop, bool lock_input, double final_opacity,
                             double delay, std::optional<EaseType> ease_in, std::optional<EaseType> ease_out,
                             std::optional<double> reverse_delay)
    : BaseAnimation(duration, delay, loop, lock_input), initial_opacity(initial_opacity), final_opacity(final_opacity),
      initial_opacity_saved(initial_opacity), final_opacity_saved(final_opacity), ease_in(ease_in), ease_out(ease_out),
      reverse_delay(reverse_delay), reverse_delay_saved(reverse_delay) {
    attribute = initial_opacity;
}

void FadeAnimation::restart() {
    BaseAnimation::restart();
    reverse_delay = reverse_delay_saved;
    initial_opacity = initial_opacity_saved;
    final_opacity = final_opacity_saved;
    attribute = initial_opacity;
}

void FadeAnimation::update(double current_time_ms) {
    if (!is_started) {
        return;
    }
    BaseAnimation::update(current_time_ms);
    if (is_finished) {
        return;
    }

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

std::unique_ptr<BaseAnimation> FadeAnimation::copy() const {
    return std::make_unique<FadeAnimation>(duration, initial_opacity_saved, loop, lock_input, final_opacity_saved,
                                           delay_saved, ease_in, ease_out, reverse_delay_saved);
}

MoveAnimation::MoveAnimation(double duration, int total_distance, bool loop, bool lock_input, int start_position,
                             double delay, std::optional<double> reverse_delay, std::optional<EaseType> ease_in,
                             std::optional<EaseType> ease_out, std::vector<Waypoint> waypoints)
    : BaseAnimation(duration, delay, loop, lock_input), total_distance(total_distance), start_position(start_position),
      total_distance_saved(total_distance), start_position_saved(start_position), ease_in(ease_in), ease_out(ease_out),
      reverse_delay(reverse_delay), reverse_delay_saved(reverse_delay), waypoints(std::move(waypoints)) {
    std::ranges::stable_sort(this->waypoints, {}, &Waypoint::at);
    attribute = start_position;
}

void MoveAnimation::restart() {
    BaseAnimation::restart();
    reverse_delay = reverse_delay_saved;
    total_distance = total_distance_saved;
    start_position = start_position_saved;
    attribute = start_position;
}

void MoveAnimation::update(double current_time_ms) {
    if (!is_started) {
        return;
    }
    BaseAnimation::update(current_time_ms);
    if (is_finished) {
        return;
    }

    double elapsed_time = current_time_ms - start_ms;

    if (elapsed_time < delay) {
        attribute = start_position;
    } else if (elapsed_time >= delay + duration) {
        attribute = start_position + total_distance;
        if (reverse_delay.has_value() && !waypoints.empty()) {
            // Reversing would need the waypoint list (and each segment's
            // easing direction) mirrored, which isn't implemented -- just
            // negating total_distance/start_position here would replay the
            // same forward waypoint sequence from the new position instead
            // of retracing it backward. Finish rather than play a wrong path.
            spdlog::warn(
                "MoveAnimation: reverse_delay combined with waypoints is not supported; finishing without reversing");
            is_finished = true;
        } else if (reverse_delay.has_value()) {
            start_ms = current_time_ms;
            delay = reverse_delay.value();
            start_position = start_position + total_distance;
            total_distance = -total_distance;
            reverse_delay = std::nullopt;
        } else {
            is_finished = true;
        }
    } else if (!waypoints.empty()) {
        double t = elapsed_time - delay;
        double prev_at = 0.0;
        int prev_val = 0;
        size_t i = 0;
        for (; i < waypoints.size() && t >= waypoints[i].at; i++) {
            prev_at = waypoints[i].at;
            prev_val = waypoints[i].value;
        }
        bool is_last_segment = (i == waypoints.size());
        double next_at = is_last_segment ? duration : waypoints[i].at;
        int next_val = is_last_segment ? total_distance : waypoints[i].value;

        double seg_progress = (next_at > prev_at) ? (t - prev_at) / (next_at - prev_at) : 1.0;
        seg_progress = is_last_segment ? applyEasing(seg_progress, ease_in, ease_out)
                                       : applyEasing(seg_progress, waypoints[i].ease_in, waypoints[i].ease_out);
        attribute = start_position + prev_val + (next_val - prev_val) * seg_progress;
    } else {
        double progress = applyEasing((elapsed_time - delay) / duration, ease_in, ease_out);
        attribute = start_position + (total_distance * progress);
    }
}

std::unique_ptr<BaseAnimation> MoveAnimation::copy() const {
    return std::make_unique<MoveAnimation>(duration, total_distance_saved, loop, lock_input, start_position_saved,
                                           delay_saved, reverse_delay_saved, ease_in, ease_out, waypoints);
}

TextureChangeAnimation::TextureChangeAnimation(double duration,
                                               const std::vector<std::tuple<double, double, int>>& keyframes, bool loop,
                                               bool lock_input, double delay)
    : BaseAnimation(duration, delay, loop, lock_input) {
    for (const auto& [start, end, index] : keyframes) {
        this->textures.push_back({start, end, index});
    }
    if (!this->textures.empty()) {
        attribute = this->textures[0].index;
    }
}

void TextureChangeAnimation::reset() {
    BaseAnimation::reset();
    if (!textures.empty()) {
        attribute = textures[0].index;
    }
}

void TextureChangeAnimation::update(double current_time_ms) {
    if (!is_started) {
        return;
    }
    BaseAnimation::update(current_time_ms);
    if (is_finished) {
        return;
    }

    double elapsed_time = current_time_ms - start_ms;
    if (elapsed_time < delay) {
        return;
    }

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

std::unique_ptr<BaseAnimation> TextureChangeAnimation::copy() const {
    std::vector<std::tuple<double, double, int>> tex_tuples;
    tex_tuples.reserve(textures.size());
    for (const auto& frame : textures) {
        tex_tuples.emplace_back(frame.start, frame.end, frame.index);
    }
    return std::make_unique<TextureChangeAnimation>(duration, tex_tuples, loop, lock_input, delay_saved);
}

TextStretchAnimation::TextStretchAnimation(double duration, double delay, bool loop, bool lock_input)
    : BaseAnimation(duration, delay, loop, lock_input) {}

void TextStretchAnimation::update(double current_time_ms) {
    if (!is_started) {
        return;
    }
    BaseAnimation::update(current_time_ms);
    if (is_finished) {
        return;
    }

    double elapsed_time = current_time_ms - start_ms;
    if (elapsed_time < delay) {
        return;
    }

    double animation_time = elapsed_time - delay;
    if (animation_time <= duration) {
        attribute = 2 + 5 * (static_cast<int>(animation_time) / 25.0f);
    } else if (animation_time <= duration + 116) {
        int frame_time = static_cast<int>((animation_time - duration) / 16.57);
        attribute = 2 + 10 - (2 * (frame_time + 1));
    } else {
        attribute = 0;
        is_finished = true;
    }
}

std::unique_ptr<BaseAnimation> TextStretchAnimation::copy() const {
    return std::make_unique<TextStretchAnimation>(duration, delay_saved, loop, lock_input);
}

TextureResizeAnimation::TextureResizeAnimation(double duration, double initial_size, bool loop, bool lock_input,
                                               double final_size, double delay, std::optional<double> reverse_delay,
                                               std::optional<EaseType> ease_in, std::optional<EaseType> ease_out)
    : BaseAnimation(duration, delay, loop, lock_input), initial_size(initial_size), final_size(final_size),
      initial_size_saved(initial_size), final_size_saved(final_size), ease_in(ease_in), ease_out(ease_out),
      reverse_delay(reverse_delay), reverse_delay_saved(reverse_delay) {
    attribute = initial_size;
}

void TextureResizeAnimation::restart() {
    BaseAnimation::restart();
    reverse_delay = reverse_delay_saved;
    initial_size = initial_size_saved;
    final_size = final_size_saved;
    attribute = initial_size;
}

void TextureResizeAnimation::update(double current_time_ms) {
    if (!is_started) {
        return;
    }
    BaseAnimation::update(current_time_ms);
    if (is_finished) {
        return;
    }

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

std::unique_ptr<BaseAnimation> TextureResizeAnimation::copy() const {
    return std::make_unique<TextureResizeAnimation>(duration, initial_size_saved, loop, lock_input, final_size_saved,
                                                    delay_saved, reverse_delay_saved, ease_in, ease_out);
}

Value AnimationParser::resolveValue(const Value& ref_obj, std::set<int>& visited) {
    if (!ref_obj.HasMember("property")) {
        throw std::runtime_error("Reference requires 'property' field");
    }

    int ref_id;
    if (!ref_obj.HasMember("reference_id")) {
        throw std::runtime_error("Reference requires 'reference_id' field");
    }
    if (ref_obj["reference_id"].IsString()) {
        try {
            ref_id = std::stoi(ref_obj["reference_id"].GetString());
        } catch (const std::exception&) {
            throw std::runtime_error(std::string("Invalid reference_id string: ") +
                                     ref_obj["reference_id"].GetString());
        }
    } else if (ref_obj["reference_id"].IsInt()) {
        ref_id = ref_obj["reference_id"].GetInt();
    } else {
        throw std::runtime_error("reference_id must be string or int");
    }
    if (!ref_obj["property"].IsString()) {
        throw std::runtime_error("Reference 'property' must be a string");
    }
    std::string ref_property = ref_obj["property"].GetString();

    if (!raw_anims.contains(ref_id)) {
        throw std::runtime_error("Referenced animation " + std::to_string(ref_id) + " not found");
    }

    Value resolved_anim = findRefs(ref_id, visited);

    if (!resolved_anim.HasMember(ref_property.c_str())) {
        throw std::runtime_error("Property '" + ref_property + "' not found in animation " + std::to_string(ref_id));
    }

    Value base_value;
    base_value.CopyFrom(resolved_anim[ref_property.c_str()], *allocator);

    if (ref_obj.HasMember("init_val")) {
        const Value& init_val_obj = ref_obj["init_val"];

        if (init_val_obj.IsObject() && init_val_obj.HasMember("reference_id")) {
            Value init_val = resolveValue(init_val_obj, visited);

            // Handle numeric addition with type coercion
            double base_num = base_value.IsDouble()
                                  ? base_value.GetDouble()
                                  : (base_value.IsInt() ? static_cast<double>(base_value.GetInt()) : 0.0);
            double init_num = init_val.IsDouble() ? init_val.GetDouble()
                                                  : (init_val.IsInt() ? static_cast<double>(init_val.GetInt()) : 0.0);

            if (base_value.IsDouble() || init_val.IsDouble()) {
                base_value.SetDouble(base_num + init_num);
            } else {
                base_value.SetInt(static_cast<int>(base_num + init_num));
            }
        } else {
            // Handle numeric addition with type coercion
            double base_num = base_value.IsDouble()
                                  ? base_value.GetDouble()
                                  : (base_value.IsInt() ? static_cast<double>(base_value.GetInt()) : 0.0);
            double init_num = init_val_obj.IsDouble()
                                  ? init_val_obj.GetDouble()
                                  : (init_val_obj.IsInt() ? static_cast<double>(init_val_obj.GetInt()) : 0.0);

            if (base_value.IsDouble() || init_val_obj.IsDouble()) {
                base_value.SetDouble(base_num + init_num);
            } else {
                base_value.SetInt(static_cast<int>(base_num + init_num));
            }
        }
    }

    return base_value;
}

Value AnimationParser::findRefs(int anim_id, std::set<int>& visited) {
    if (visited.contains(anim_id)) {
        throw runtime_error("Circular reference detected involving animation " + std::to_string(anim_id));
    }

    visited.insert(anim_id);

    auto raw_it = raw_anims.find(anim_id);
    if (raw_it == raw_anims.end()) {
        throw runtime_error("Animation " + std::to_string(anim_id) + " not found");
    }
    Value animation;
    animation.CopyFrom(raw_it->second, *allocator);

    for (auto it = animation.MemberBegin(); it != animation.MemberEnd(); ++it) {
        if (it->value.IsObject() && it->value.HasMember("reference_id")) {
            std::set<int> visited_copy = visited;
            Value resolved = resolveValue(it->value, visited_copy);
            it->value.CopyFrom(resolved, *allocator);
        }
    }

    visited.erase(anim_id);
    return animation;
}

std::unique_ptr<BaseAnimation> AnimationParser::createAnimation(const Value& anim_obj) {
    if (!anim_obj.HasMember("type") || !anim_obj["type"].IsString()) {
        throw std::runtime_error("Animation requires a string 'type'");
    }
    std::string type = anim_obj["type"].GetString();
    double duration = 0.0;
    if (anim_obj.HasMember("duration")) {
        if (!anim_obj["duration"].IsNumber()) {
            throw std::runtime_error("Animation 'duration' must be numeric");
        }
        duration = anim_obj["duration"].IsDouble() ? anim_obj["duration"].GetDouble()
                                                   : static_cast<double>(anim_obj["duration"].GetInt());
        if (duration <= 0.0) {
            throw std::runtime_error("Animation of type '" + type + "' requires a positive duration");
        }
    } else {
        throw std::runtime_error("Animation of type '" + type + "' requires duration");
    }

    auto get_double = [&](const char* key, double def) {
        if (anim_obj.HasMember(key)) {
            if (anim_obj[key].IsDouble()) {
                return anim_obj[key].GetDouble();
            }

            if (anim_obj[key].IsInt()) {
                return static_cast<double>(anim_obj[key].GetInt());
            }
        }
        return def;
    };

    auto get_int = [&](const char* key, int def) {
        return anim_obj.HasMember(key) && anim_obj[key].IsInt() ? anim_obj[key].GetInt() : def;
    };

    auto get_bool = [&](const char* key, bool def) {
        return anim_obj.HasMember(key) && anim_obj[key].IsBool() ? anim_obj[key].GetBool() : def;
    };

    auto get_double_opt = [&](const char* key) -> std::optional<double> {
        if (anim_obj.HasMember(key)) {
            if (anim_obj[key].IsDouble()) {
                return anim_obj[key].GetDouble();
            }

            if (anim_obj[key].IsInt()) {
                return static_cast<double>(anim_obj[key].GetInt());
            }
        }
        return std::nullopt;
    };

    auto get_ease_opt_from = [&](const Value& obj, const char* key) -> std::optional<EaseType> {
        if (!obj.HasMember(key) || !obj[key].IsString()) {
            return std::nullopt;
        }
        std::string str = obj[key].GetString();
        if (str == "quadratic") {
            return EaseType::Quadratic;
        }
        if (str == "cubic") {
            return EaseType::Cubic;
        }
        if (str == "exponential") {
            return EaseType::Exponential;
        }
        throw std::runtime_error("Unknown ease type: " + str);
    };

    auto get_ease_opt = [&](const char* key) -> std::optional<EaseType> { return get_ease_opt_from(anim_obj, key); };

    auto get_waypoints = [&] -> std::vector<MoveAnimation::Waypoint> {
        std::vector<MoveAnimation::Waypoint> result;
        if (!anim_obj.HasMember("waypoints") || !anim_obj["waypoints"].IsArray()) {
            return result;
        }
        for (const auto& wp : anim_obj["waypoints"].GetArray()) {
            if (!wp.IsObject() || !wp.HasMember("at") || !wp.HasMember("value") || !wp["at"].IsNumber() ||
                !wp["value"].IsNumber()) {
                throw std::runtime_error("Each 'waypoints' entry requires numeric 'at' and 'value'");
            }
            double at = wp["at"].IsDouble() ? wp["at"].GetDouble() : static_cast<double>(wp["at"].GetInt());
            int value = wp["value"].IsInt() ? wp["value"].GetInt() : static_cast<int>(wp["value"].GetDouble());
            result.push_back({at, value, get_ease_opt_from(wp, "ease_in"), get_ease_opt_from(wp, "ease_out")});
        }
        return result;
    };

    double delay = get_double("delay", 0.0);
    bool loop = get_bool("loop", false);
    bool lock_input = get_bool("lock_input", false);

    if (type == "fade") {
        return std::make_unique<FadeAnimation>(duration, get_double("initial_opacity", 1.0), loop, lock_input,
                                               get_double("final_opacity", 0.0), delay, get_ease_opt("ease_in"),
                                               get_ease_opt("ease_out"), get_double_opt("reverse_delay"));
    }

    if (type == "move") {
        return std::make_unique<MoveAnimation>(duration, get_int("total_distance", 0), loop, lock_input,
                                               get_int("start_position", 0), delay, get_double_opt("reverse_delay"),
                                               get_ease_opt("ease_in"), get_ease_opt("ease_out"), get_waypoints());
    }

    if (type == "texture_change") {
        std::vector<std::tuple<double, double, int>> textures;
        if (anim_obj.HasMember("textures") && anim_obj["textures"].IsArray()) {
            const Value& tex_array = anim_obj["textures"];
            for (SizeType i = 0; i < tex_array.Size(); i++) {
                if (tex_array[i].IsArray() && tex_array[i].Size() == 3 && tex_array[i][0].IsNumber() &&
                    tex_array[i][1].IsNumber() && tex_array[i][2].IsInt()) {
                    double start = tex_array[i][0].GetDouble();
                    double end = tex_array[i][1].GetDouble();
                    int index = tex_array[i][2].GetInt();
                    textures.emplace_back(start, end, index);
                }
            }
        }
        return std::make_unique<TextureChangeAnimation>(duration, textures, loop, lock_input, delay);
    }

    if (type == "text_stretch") {
        return std::make_unique<TextStretchAnimation>(duration, delay, loop, lock_input);
    }

    if (type == "texture_resize") {
        return std::make_unique<TextureResizeAnimation>(
            duration, get_double("initial_size", 1.0), loop, lock_input, get_double("final_size", 0.0), delay,
            get_double_opt("reverse_delay"), get_ease_opt("ease_in"), get_ease_opt("ease_out"));
    }

    throw std::runtime_error("Unknown animation type: " + type);
}

std::unordered_map<int, std::unique_ptr<BaseAnimation>> AnimationParser::parse_animations(const Value& animation_json) {
    if (!animation_json.IsArray()) {
        throw std::runtime_error("Animation JSON must be an array");
    }

    Document temp_doc;
    allocator = &temp_doc.GetAllocator();
    raw_anims.clear();

    std::unordered_map<int, std::unique_ptr<BaseAnimation>> anim_dict;

    try {
        // First pass: collect all animations
        for (SizeType i = 0; i < animation_json.Size(); i++) {
            const Value& item = animation_json[i];

            if (!item.HasMember("id")) {
                throw std::runtime_error("Animation requires id");
            }
            if (!item.HasMember("type")) {
                throw std::runtime_error("Animation requires type");
            }
            if (!item["id"].IsInt()) {
                throw std::runtime_error("Animation 'id' must be an int");
            }
            int id = item["id"].GetInt();
            if (raw_anims.contains(id)) {
                throw std::runtime_error("Duplicate animation id: " + std::to_string(id));
            }
            Value item_copy;
            item_copy.CopyFrom(item, *allocator);
            raw_anims[id] = std::move(item_copy);
        }

        for (auto& [id, _] : raw_anims) {
            std::set<int> visited;
            Value absolute_anim = findRefs(id, visited);

            auto anim = createAnimation(absolute_anim);
            anim_dict[id] = std::move(anim);
        }
    } catch (...) {
        raw_anims.clear();
        allocator = nullptr;
        throw;
    }

    raw_anims.clear();
    allocator = nullptr;

    return anim_dict;
}

std::unordered_map<int, std::unique_ptr<BaseAnimation>>
AnimationParser::parseAnimationsFromString(const std::string& json_str) {
    Document doc;
    doc.Parse(json_str.c_str());

    if (doc.HasParseError()) {
        throw std::runtime_error(std::string("JSON parse error: ") + GetParseError_En(doc.GetParseError()) +
                                 " at offset " + std::to_string(doc.GetErrorOffset()));
    }

    return parse_animations(doc);
}
