#pragma once

class EntryOverlay {
private:
    bool online;
public:
    EntryOverlay();
    void update(double current_ms);
    void draw(float x = 0, float y = 0);
};
