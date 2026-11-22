#pragma once
#include <stdint.h>

namespace boat {
enum class RgbColor { Off, Red, Yellow, Blue, Green, White };
struct LightState {
    bool red = false;
    RgbColor rgb = RgbColor::Off;
    uint32_t revision = 0;
};
// Independent from motor commands: changing a light must not renew a drive lease.
class Lights {
public:
    const LightState& state() const { return state_; }
    bool set(uint32_t expectedRevision, bool red, RgbColor rgb) {
        if (expectedRevision != state_.revision || rgb < RgbColor::Off || rgb > RgbColor::White) return false;
        state_.red = red;
        state_.rgb = rgb;
        ++state_.revision;
        return true;
    }
    void off() {
        state_.red = false;
        state_.rgb = RgbColor::Off;
        ++state_.revision; // Also invalidate pending requests, even if already off.
    }
private:
    LightState state_;
};
inline const char* name(RgbColor color) {
    switch (color) {
    case RgbColor::Off: return "off"; case RgbColor::Red: return "red";
    case RgbColor::Yellow: return "yellow"; case RgbColor::Blue: return "blue";
    case RgbColor::Green: return "green"; case RgbColor::White: return "white";
    }
    return "off";
}
}
