#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../../libs/gfx/text.h"

class AccessCodePopup {
public:
    struct Entry {
        std::string label; // empty when only one seat is shown
        bool is_2p = false;
        std::string code;
    };

    explicit AccessCodePopup(const std::vector<Entry>& entries);
    ~AccessCodePopup();
    AccessCodePopup(const AccessCodePopup&) = delete;
    AccessCodePopup& operator=(const AccessCodePopup&) = delete;

    void draw();

private:
    struct Frame {
        TextureObject* top_left;
        TextureObject* top_right;
        TextureObject* bottom_left;
        TextureObject* bottom_right;
        TextureObject* top;
        TextureObject* bottom;
        TextureObject* left;
        TextureObject* right;
        TextureObject* center;
    };

    static Frame load_frame(bool is_2p);
    static void draw_frame(const Frame& frame, float x, float y, float w, float h);

    struct Card {
        Frame frame;
        Texture qr;
        std::unique_ptr<OutlinedText> label;
        std::unique_ptr<OutlinedText> code;
    };

    std::unique_ptr<OutlinedText> title;
    std::vector<Card> cards;
};
