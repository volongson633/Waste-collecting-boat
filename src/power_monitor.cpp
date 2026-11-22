#include "power_monitor.h"
#include "board_config.h"
#include "app_config.h"
#include <Arduino.h>
#include <cmath>

static_assert(board::currentAdc < 0 || config::conveyorJamAmps > 0, "Set a calibrated jam threshold");
namespace {
void setupUltrasonic(int trigger, int echo) {
    if (trigger < 0) return;
    pinMode(trigger, OUTPUT);
    digitalWrite(trigger, LOW);
    pinMode(echo, INPUT);
}

float readDistanceCm(int trigger, int echo) {
    digitalWrite(trigger, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigger, LOW);
    return pulseIn(echo, HIGH, config::ultrasonicTimeoutUs) * 0.01715f;
}

void confirmState(bool proposed, bool& state, unsigned& transitions, unsigned samples) {
    if (proposed == state) transitions = 0;
    else if (++transitions >= samples) {
        state = proposed;
        transitions = 0;
    }
}

float averageMv(int pin) {
    uint32_t sum = 0;
    for (int i = 0; i < 8; ++i) sum += analogReadMilliVolts(pin);
    return sum / 8.0f;
}
}
void PowerMonitor::begin() {
    setupUltrasonic(board::binTrig, board::binEcho);
    setupUltrasonic(board::obstacleTrig, board::obstacleEcho);
    analogReadResolution(12);
    if (board::batteryAdc >= 0) {
        pinMode(board::batteryAdc, INPUT);
        analogSetPinAttenuation(board::batteryAdc, ADC_11db);
    }
    if (board::currentAdc >= 0) {
        pinMode(board::currentAdc, INPUT);
        analogSetPinAttenuation(board::currentAdc, ADC_11db);
    }
    if (board::intakeSensor >= 0) pinMode(board::intakeSensor, board::intakeActiveLow ? INPUT_PULLUP : INPUT_PULLDOWN);
}
SensorStatus PowerMonitor::sample(uint32_t now, bool conveyorRunning) {
    if (board::batteryAdc >= 0) {
        const float mv = averageMv(board::batteryAdc);
        const float rawV = mv * (config::dividerTopOhms + config::dividerBottomOhms) /
            config::dividerBottomOhms * config::batteryCalibration / 1000.0f;
        status_.batteryV = samples_ ? status_.batteryV * 0.75f + rawV * 0.25f : rawV;
        if (samples_ < 5) ++samples_;
        status_.batteryValid = samples_ >= 5 && mv < 3000 && rawV >= config::batteryMinimumValidV &&
            rawV <= config::batteryMaximumValidV;
        status_.batteryWarning = status_.batteryValid && status_.batteryV < config::batteryWarningV;
        status_.batteryRecovered = status_.batteryValid && status_.batteryV >= config::batteryRecoveryV;
        if (status_.batteryValid && status_.batteryV <= config::batteryStopV) {
            if (!lowPending_) { lowPending_ = true; lowSince_ = now; }
            if (now - lowSince_ >= config::lowBatteryHoldMs) status_.batteryLow = true;
        } else {
            lowPending_ = false;
            if (status_.batteryRecovered || !status_.batteryValid) status_.batteryLow = false;
        }
    }
    if (board::intakeSensor >= 0) {
        const bool raw = digitalRead(board::intakeSensor) == (board::intakeActiveLow ? LOW : HIGH);
        if (raw != lastIntakeRaw_) { lastIntakeRaw_ = raw; intakeSince_ = now; }
        if (now - intakeSince_ >= config::intakeDebounceMs) status_.intake = raw;
    }
    if (conveyorRunning && !conveyorWasRunning_) conveyorSince_ = now;
    conveyorWasRunning_ = conveyorRunning;
    status_.jam = false;
    if (board::currentAdc >= 0) {
        status_.currentA = std::fabs(averageMv(board::currentAdc) - config::currentZeroMv) / config::currentMvPerAmp;
        const bool overloaded = conveyorRunning && now - conveyorSince_ >= config::jamStartupIgnoreMs &&
            status_.currentA >= config::conveyorJamAmps;
        if (overloaded) {
            if (!overCurrent_) { overCurrent_ = true; currentSince_ = now; }
            status_.jam = now - currentSince_ >= config::jamHoldMs;
        } else overCurrent_ = false;
    }
    // Pulse only one sensor per cycle to avoid overlapping echoes.
    if (board::binTrig >= 0 && (sampleBin_ || board::obstacleTrig < 0)) {
        const float cm = readDistanceCm(board::binTrig, board::binEcho);
        status_.binDistanceCm = cm;
        const bool full = cm == 0 ? status_.binFull :
            cm <= (status_.binFull ? config::binClearCm : config::binFullCm);
        confirmState(full, status_.binFull, binTransitions_, 3);
    } else if (board::obstacleTrig >= 0) {
        const float cm = readDistanceCm(board::obstacleTrig, board::obstacleEcho);
        status_.obstacleDistanceCm = cm;
        const bool near = cm > 0 && cm <=
            (status_.obstacleNear ? config::obstacleClearCm : config::obstacleNearCm);
        confirmState(near, status_.obstacleNear, obstacleTransitions_, near ? 1 : 3);
    }
    sampleBin_ = !sampleBin_;
    return status_;
}
