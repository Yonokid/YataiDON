#pragma once

#include "../../libs/text.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

inline std::string exam_threshold_text(const TextureWrapper& tex,
                                       const std::string& type,
                                       const std::string& range,
                                       int value,
                                       const std::string& lang) {
    const bool gauge = (type == "gauge");
    const bool less  = (range == "less");
    const char* key = gauge ? (less ? "dan_exam_text_gauge_less" : "dan_exam_text_gauge")
                    : less  ? "dan_exam_text_less"
                            : "dan_exam_text_more";
    const char* jp  = gauge ? (less ? "%s \xEF\xBC\x85\xE6\x9C\xAA\xE6\xBA\x80"   // "%s ％未満" (invented)
                                    : "%s \xEF\xBC\x85\xE4\xBB\xA5\xE4\xB8\x8A")  // "%s ％以上"
                    : less  ? "%s \xE6\x9C\xAA\xE6\xBA\x80"                       // "%s 未満"
                            : "%s \xE4\xBB\xA5\xE4\xB8\x8A";                      // "%s 以上"
    std::string fmt = tex.skin_text(key, lang, tex.skin_text(key, "ja", jp));
    const std::string num = std::to_string(value);
    // Only "%s" is substituted; "%%" is an escaped percent (the en rows use it).
    std::string out;
    for (size_t i = 0; i < fmt.size(); i++) {
        if (fmt[i] == '%' && i + 1 < fmt.size() && fmt[i + 1] == 's') { out += num; i++; }
        else if (fmt[i] == '%' && i + 1 < fmt.size() && fmt[i + 1] == '%') { out += '%'; i++; }
        else out += fmt[i];
    }
    return out;
}

class ExamCaptionCache {
public:
    OutlinedText* get(const std::string& s, int size, float outline,
                      ray::Color fill = ray::WHITE, ray::Color ol = ray::BLACK) {
        if (s.empty() || size <= 0) return nullptr;
        const std::string k = s + "\x1f" + std::to_string(size) + "\x1f"
                            + std::to_string((int)(outline * 100));
        auto it = items.find(k);
        if (it == items.end())
            it = items.emplace(k, std::make_unique<OutlinedText>(
                     s, size, fill, ol, false, outline)).first;
        return it->second.get();
    }

    void clear() { items.clear(); }

    static float pad_for(float outline, float screen_scale) {
        return (float)((int)(outline * screen_scale) + 2);
    }

private:
    std::map<std::string, std::unique_ptr<OutlinedText>> items;
};
