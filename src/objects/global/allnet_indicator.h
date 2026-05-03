#pragma once

class AllNetIcon {
private:
    bool online;
public:
    AllNetIcon();

    void update(double current_ms);

    void draw(float x = 0, float y = 0);
};
