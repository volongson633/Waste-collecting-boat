#pragma once
#include <stdint.h>

namespace boat {
enum class State { Stop, Manual, Search, Align, Approach, Collect, Fault };
enum class Reason { None, UserStop, LinkTimeout, BatteryInvalid, BatteryLow,
    CameraUnavailable, VisionUnavailable, VisionStale, SearchTimeout,
    ApproachTimeout, BinFull, Obstacle, Jam, Hardware, InvalidCommand };
enum class UltrasonicCycle { Idle, Running, Finished, Interrupted };
enum class Action { Stop, Manual, Auto, Drive, Heartbeat, ClearFault };
struct Command {
    Action action = Action::Stop;
    uint32_t epoch = 0;
    uint32_t sequence = 0;
    int left = 0;
    int right = 0;
    int conveyor = 0;
};
struct Detection {
    uint32_t frame = 0;
    uint32_t capturedAt = 0;
    bool healthy = false;
    bool found = false;
    float x = 0.5f;
    float y = 0.5f;
    float confidence = 0.0f;
};
struct Inputs {
    bool hardwareReady = true;
    bool batteryValid = false;
    bool batteryLow = false;
    bool batteryRecovered = false;
    bool cameraReady = false;
    bool detectorReady = false;
    bool intake = false;
    bool jam = false;
    bool binFull = false;
    bool obstacleNear = false;
    Detection detection;
};
struct Output { int left = 0; int right = 0; int conveyor = 0; };
struct Settings {
    bool requireBattery = true;
    uint32_t linkTimeoutMs = 1000;
    uint32_t visionTimeoutMs = 1500;
    uint32_t searchTimeoutMs = 10000;
    uint32_t approachTimeoutMs = 15000;
    uint32_t collectMs = 3500;
    uint32_t ultrasonicCollectMs = 3000;
    unsigned confirmFrames = 3;
    float minConfidence = 0.7f;
    float centerDeadband = 0.12f;
    float collectY = 0.80f;
    float trackingRadius = 0.30f;
    int searchSpeed = 65;
    int approachSpeed = 95;
    int turnCorrection = 40;
    int collectionSpeed = 55;
    int conveyorSpeed = 180;
    int maxDrive = 180;
};

class Controller {
public:
    explicit Controller(const Settings& settings = Settings(), uint32_t bootEpoch = 1);
    bool command(const Command& command, uint32_t now, const Inputs& inputs);
    Output tick(uint32_t now, const Inputs& inputs);
    State state() const { return state_; }
    Reason reason() const { return reason_; }
    uint32_t epoch() const { return epoch_; }
    uint32_t sequence() const { return sequence_; }
    UltrasonicCycle ultrasonicCycle() const { return ultrasonicCycle_; }
    unsigned collectionCycles() const { return collectionCycles_; }
    bool active() const { return state_ != State::Stop && state_ != State::Fault; }
    bool automatic() const { return active() && state_ != State::Manual; }
    void forceFault(Reason reason);
private:
    Settings cfg_;
    State state_ = State::Stop;
    Reason reason_ = Reason::None;
    uint32_t epoch_;
    uint32_t sequence_ = 0;
    uint32_t lastContact_ = 0;
    uint32_t stateSince_ = 0;
    uint32_t searchSince_ = 0;
    uint32_t targetSince_ = 0;
    uint32_t lastFrame_ = 0;
    unsigned confirmations_ = 0;
    unsigned collectionCycles_ = 0;
    float previousX_ = 0.5f;
    float previousY_ = 0.5f;
    Output manual_;
    UltrasonicCycle ultrasonicCycle_ = UltrasonicCycle::Idle;
    uint32_t ultrasonicCollectSince_ = 0;
    bool ready(const Inputs&) const;
    bool visionFresh(const Detection&, uint32_t now) const;
    void stop(Reason reason);
    void resetTracking(uint32_t now);
};
const char* name(State state);
const char* name(Reason reason);
const char* name(UltrasonicCycle cycle);
} // namespace boat
