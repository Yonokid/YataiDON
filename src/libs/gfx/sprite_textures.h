#pragma once

#include <algorithm>
#include <array>

class SpriteTextures {
    std::array<unsigned, 8> textures{};
    unsigned used = 0;

public:
    static constexpr unsigned capacity = 8;
    unsigned size() const { return used; }
    unsigned operator[](unsigned index) const { return textures[index]; }
    bool contains(unsigned texture) const {
        return std::find(textures.begin(), textures.begin() + used, texture) != textures.begin() + used;
    }
    void clear() { used = 0; }

    template<class Flush>
    unsigned add(unsigned texture, Flush flush) {
        for (unsigned i = 0; i < used; ++i) {
            if (textures[i] == texture) {
                return i;
            }
        }
        if (used == capacity) {
            flush();
            clear();
        }
        textures[used] = texture;
        return used++;
    }
};
