#pragma once
#include <stdint.h>

struct SensorStatus {
    float batteryV = 0;
    float currentA = 0;
    bool batteryValid = false;
    bool batteryWarning = false;
    bool batteryLow = false;
    bool batteryRecovered = false;
    bool intake = false;
    bool jam = false;
    bool binFull = false;
    bool obstacleNear = false;
    float binDistanceCm = 0;
    float obstacleDistanceCm = 0;
};
class PowerMonitor {
public:
    void begin();
    SensorStatus sample(uint32_t now, bool conveyorRunning);
private:
    SensorStatus status_;
    bool sampleBin_ = true;
    unsigned binTransitions_ = 0;
    unsigned obstacleTransitions_ = 0;
    unsigned samples_ = 0;
    bool lowPending_ = false;
    bool overCurrent_ = false;
    bool conveyorWasRunning_ = false;
    bool lastIntakeRaw_ = false;
    uint32_t lowSince_ = 0;
    uint32_t currentSince_ = 0;
    uint32_t conveyorSince_ = 0;
    uint32_t intakeSince_ = 0;
};
