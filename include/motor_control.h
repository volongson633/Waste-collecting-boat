#pragma once
#include "boat/Controller.h"
#include "boat/Motion.h"
#include "app_config.h"

class MotorControl {
public:
    bool begin();
    boat::Output apply(const boat::Output& desired, uint32_t now);
    void stop(uint32_t now);
private:
    boat::MotorRamp left_{config::pwmRampStep, config::reverseDeadTimeMs};
    boat::MotorRamp right_{config::pwmRampStep, config::reverseDeadTimeMs};
    boat::MotorRamp conveyor_{config::pwmRampStep, config::reverseDeadTimeMs};
    bool initialized_ = false;
};

void logMotorOutput(const boat::Output& motor);
