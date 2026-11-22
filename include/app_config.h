#pragma once
#include <stdint.h>
#include "wifi_config.h"
#include "board_config.h"

#ifndef BOAT_OUTPUTS_ENABLED
#define BOAT_OUTPUTS_ENABLED 0
#endif
#ifndef BOAT_VISION_BACKEND
#define BOAT_VISION_BACKEND 0
#endif
#ifndef BOAT_BATTERY_REQUIRED
#define BOAT_BATTERY_REQUIRED BOAT_OUTPUTS_ENABLED
#endif
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef BOAT_AP_SSID
#define BOAT_AP_SSID AP_SSID
#endif
#ifndef BOAT_AP_PASSWORD
#define BOAT_AP_PASSWORD AP_PASSWORD
#endif

namespace config {
constexpr bool idlePatrolEnabled = true;
constexpr uint32_t idlePatrolWaitMs = 10000;
constexpr uint32_t idlePatrolStepMs = 3000;
constexpr int idlePatrolSpeed = 80;
constexpr bool outputsEnabled = BOAT_OUTPUTS_ENABLED != 0;
constexpr int visionBackend = BOAT_VISION_BACKEND; // 0: none, 1: red demo, 2: Edge Impulse.
constexpr uint32_t controlPeriodMs = 20;
constexpr uint32_t cameraPeriodMs = visionBackend == 0 ? 50 : 200;
constexpr uint32_t sensorPeriodMs = 100;
// Starting thresholds in cm. Calibrate with the installed sensors.
constexpr unsigned binFullCm = 8;
constexpr unsigned binClearCm = 12;
constexpr unsigned obstacleNearCm = 45;
constexpr unsigned obstacleClearCm = 60;
constexpr uint32_t ultrasonicTimeoutUs = 4500; // About 77 cm of one-way range.
constexpr uint32_t conveyorRevolutionMs = 3000; // Calibrated time for one belt loop.
constexpr uint32_t intakeDebounceMs = 80;
constexpr unsigned cameraWidth = 320;
constexpr unsigned cameraHeight = 240;
constexpr bool cameraMirror = true;
constexpr bool cameraFlip = true;
constexpr unsigned cameraJpegQuality = 20;
constexpr unsigned snapshotCapacity = 512 * 1024;

// Camera uses LEDC channel 0 / timer 0. Each drive motor uses two channels, one per IN pin
// (channels 2-5, timers 1/2); the optional conveyor PWM uses channel 6 (timer 3).
constexpr unsigned leftPwmChannel = 2;
constexpr unsigned leftPwmChannelB = 3;
constexpr unsigned rightPwmChannel = 4;
constexpr unsigned rightPwmChannelB = 5;
constexpr unsigned conveyorPwmChannel = 6;
constexpr unsigned pwmHz = 1000;
constexpr unsigned pwmBits = 8;
constexpr int pwmRampStep = 12; // Every 20 ms. Stop/fault always bypass the ramp.
constexpr uint32_t reverseDeadTimeMs = 80;

constexpr bool batteryRequired = BOAT_BATTERY_REQUIRED != 0 && board::batteryAdc >= 0;
constexpr float dividerTopOhms = 22000.0f;
constexpr float dividerBottomOhms = 4700.0f;
constexpr float batteryCalibration = 1.0f; // Adjust against a multimeter.
// Provisional 3S Li-ion settings; match actual cells/BMS before live use.
constexpr float batteryWarningV = 10.8f;
constexpr float batteryStopV = 10.2f;
constexpr float batteryRecoveryV = 10.8f;
constexpr float batteryMinimumValidV = 6.0f;
constexpr float batteryMaximumValidV = 13.0f;
constexpr uint32_t lowBatteryHoldMs = 1500;

// Only used with board::currentAdc >= 0. Calibrate the actual 3.3V-compatible sensor.
constexpr float currentZeroMv = 1650.0f;
constexpr float currentMvPerAmp = 100.0f;
constexpr float conveyorJamAmps = 0.0f; // Set a measured limit >0 when sensor is enabled.
constexpr uint32_t jamStartupIgnoreMs = 600;
constexpr uint32_t jamHoldMs = 400;
constexpr const char* targetLabel = "rac"; // Must match the exported EI label exactly.

// SD recording (board::sdEnabled). Frames are copied to a queue and written by a
// separate task, so a slow card never delays the camera or detector.
constexpr uint32_t sdDefaultIntervalMs = 1000;
constexpr uint32_t sdMinIntervalMs = 200;
constexpr unsigned sdQueueDepth = 4;     // Frames waiting for the card; extra frames are dropped.
constexpr uint64_t sdMinFreeBytes = 8ULL * 1024 * 1024; // Stop recording below this.

// Data-collection firmware (env data-collect). Auto mode drives a fixed pattern
// (straight, then a pivot turn alternating left/right) while saving frames to SD.
// It only runs while the phone page keeps sending heartbeats.
constexpr uint32_t collectorLinkTimeoutMs = 1000; // No heartbeat for this long: stop everything.
constexpr uint32_t collectorCommandMs = 100;      // Drive command period in auto mode.
constexpr uint32_t collectorStraightMs = 4000;
constexpr uint32_t collectorTurnMs = 1500;
constexpr unsigned collectorTurnPercent = 70;     // Pivot speed as % of the auto speed.
constexpr unsigned collectorDefaultSpeedPercent = 50;
}
static_assert(config::visionBackend >= 0 && config::visionBackend <= 2, "Unknown detector");
static_assert(sizeof(BOAT_AP_PASSWORD) >= 9, "Wi-Fi password needs at least 8 characters");
