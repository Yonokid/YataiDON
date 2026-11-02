#pragma once

#include <algorithm>
#include <array>

namespace switch_controls {

    // Engine button numbers after applying the Switch mapping below.
    inline constexpr std::array left_ka{1, 4, 9, 10};
    inline constexpr std::array left_don{2, 3, 16};
    inline constexpr std::array right_ka{5, 7, 11, 12};
    inline constexpr std::array right_don{6, 8, 17};

    // The libnx SDL backend reports A/B/X/Y, sticks, shoulders, triggers,
    // Plus/Minus, then D-pad. Minus uses the engine's global back action.
    inline constexpr const char* mapping =
        "YataiDON Switch,a:b0,b:b1,x:b3,y:b2,dpup:b13,dpright:b14,dpdown:b15,dpleft:b12,"
        "leftshoulder:b6,rightshoulder:b7,lefttrigger:b8,righttrigger:b9,"
        "leftstick:b4,rightstick:b5,start:b10,touchpad:b11,"
        "leftx:a0,lefty:a1,rightx:a2,righty:a3,";

    template<class Layout>
    bool apply_defaults_if_unconfigured(Layout& layout) {
        const bool empty =
            layout.left_kat.empty() && layout.left_don.empty() && layout.right_don.empty() && layout.right_kat.empty();
        const bool desktop_defaults = std::ranges::equal(layout.left_kat, std::array{10}) &&
                                      std::ranges::equal(layout.left_don, std::array{16}) &&
                                      std::ranges::equal(layout.right_don, std::array{17}) &&
                                      std::ranges::equal(layout.right_kat, std::array{12});
        if (!empty && !desktop_defaults) {
            return false;
        }
        layout.left_kat.assign(left_ka.begin(), left_ka.end());
        layout.left_don.assign(left_don.begin(), left_don.end());
        layout.right_kat.assign(right_ka.begin(), right_ka.end());
        layout.right_don.assign(right_don.begin(), right_don.end());
        return true;
    }

    template<class Layout>
    void upgrade_layout(Layout& layout) {
        layout.left_kat.assign(left_ka.begin(), left_ka.end());
        layout.left_don.assign(left_don.begin(), left_don.end());
        layout.right_kat.assign(right_ka.begin(), right_ka.end());
        layout.right_don.assign(right_don.begin(), right_don.end());
    }

} // namespace switch_controls
