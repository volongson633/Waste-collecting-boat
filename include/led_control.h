#pragma once
#include "boat/Lights.h"

class LedControl {
public:
    void begin();
    void apply(const boat::LightState& state);
private:
    bool initialized_ = false;
    bool previousRed_ = false;
    boat::RgbColor previousRgb_ = boat::RgbColor::Off;
};
