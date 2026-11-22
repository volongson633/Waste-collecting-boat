#pragma once
#include <stdint.h>

namespace boat {
// Pure logic, shared by the firmware and host tests.
class MotorRamp {
public:
    MotorRamp(int step = 12, uint32_t deadTimeMs = 80) : step_(step), deadTime_(deadTimeMs) {}
    int update(int target, uint32_t now) {
        if (target == 0) { stop(now); return 0; }
        const int wantedSign = target > 0 ? 1 : -1;
        if (value_ != 0 && wantedSign != sign_) {
            value_ = towards(value_, 0);
            if (value_ == 0) zeroSince_ = now;
            return value_;
        }
        if (value_ == 0 && sign_ != 0 && wantedSign != sign_ && now - zeroSince_ < deadTime_) return 0;
        sign_ = wantedSign;
        value_ = towards(value_, target);
        return value_;
    }
    void stop(uint32_t now) {
        if (value_ != 0) zeroSince_ = now;
        value_ = 0;
    }
    int value() const { return value_; }
private:
    int towards(int from, int to) const {
        return from < to ? (to - from > step_ ? from + step_ : to) :
            (from - to > step_ ? from - step_ : to);
    }
    int step_;
    uint32_t deadTime_;
    int value_ = 0;
    int sign_ = 0;
    uint32_t zeroSince_ = 0;
};
}
