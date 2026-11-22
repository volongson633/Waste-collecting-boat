#pragma once
#include "boat/Controller.h"
#include "boat/Lights.h"
#include "camera_service.h"
#include "power_monitor.h"
#include "motor_control.h"
#include <Arduino.h>

struct RuntimeStatus {
    boat::State state = boat::State::Stop;
    boat::Reason reason = boat::Reason::None;
    uint32_t epoch = 0;
    uint32_t sequence = 0;
    unsigned collectionCycles = 0;
    boat::UltrasonicCycle ultrasonicCycle = boat::UltrasonicCycle::Idle;
    bool hardwareReady = false;
    bool readyToArm = false;
    bool idlePatrol = false;
    boat::Output desired;
    boat::Output applied;
    SensorStatus sensors;
    CameraStatus camera;
    boat::LightState lights;
    uint32_t version = 0;
};
class Runtime {
public:
    explicit Runtime(CameraService& camera);
    bool begin();
    bool command(const boat::Command& command);
    bool setLights(uint32_t epoch, uint32_t revision, bool red, boat::RgbColor rgb);
    RuntimeStatus status();
    void operatorPresent();
    void enableIdlePatrol();
private:
    static void taskEntry(void* arg);
    void task();
    boat::Inputs inputs() const;
    void updateStatus();
    void tickIdlePatrol(uint32_t now);
    bool idleEnabled_ = false;
    uint32_t operatorAt_ = 0;
    uint32_t patrolAt_ = 0;
    CameraService& camera_;
    boat::Controller controller_;
    PowerMonitor power_;
    MotorControl motors_;
    boat::Lights lights_;
    RuntimeStatus status_;
    SemaphoreHandle_t mutex_ = nullptr;
};
