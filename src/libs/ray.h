#pragma once
#include <string>

namespace ray {
    extern "C" {
        #include <raylib.h>
    }
}

inline ray::Shader load_shader(const char* vs_path, const char* fs_path) {
#ifdef PLATFORM_ANDROID
    auto repath = [](const char* p) -> std::string {
        std::string s(p);
        const std::string prefix = "shader/";
        if (s.substr(0, prefix.size()) == prefix)
            s.insert(prefix.size(), "es/");
        return s;
    };
    std::string vs = repath(vs_path);
    std::string fs = repath(fs_path);
    return ray::LoadShader(vs.c_str(), fs.c_str());
#else
    return ray::LoadShader(vs_path, fs_path);
#endif
}
