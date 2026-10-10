#pragma once

#include <map>
#include <string>

enum class LocalizedTextFallback {
    NONE,
    ENGLISH,
    ENGLISH_OR_FIRST,
    REQUIRED_ENGLISH,
};

inline std::string localized_text(const std::map<std::string, std::string>& values,
                                  const std::string& language, LocalizedTextFallback fallback) {
    if (auto it = values.find(language); it != values.end()) {
        return it->second;
    }

    if (fallback == LocalizedTextFallback::NONE) return "";
    if (fallback == LocalizedTextFallback::REQUIRED_ENGLISH) return values.at("en");

    if (auto it = values.find("en"); it != values.end()) {
        return it->second;
    }

    if (fallback == LocalizedTextFallback::ENGLISH_OR_FIRST && !values.empty()) {
        return values.begin()->second;
    }

    return "";
}
