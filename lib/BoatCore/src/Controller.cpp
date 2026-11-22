#include "boat/Controller.h"
#include <cmath>

namespace boat {
namespace {
int clamp(int value, int limit) { return value < -limit ? -limit : value > limit ? limit : value; }
uint32_t elapsed(uint32_t now, uint32_t since) { return now - since; }
}
Controller::Controller(const Settings& settings, uint32_t bootEpoch)
    : cfg_(settings), epoch_(bootEpoch ? bootEpoch : 1) {}

void Controller::resetTracking(uint32_t now) {
    confirmations_ = 0;
    lastFrame_ = 0;
    searchSince_ = now;
    targetSince_ = now;
}
void Controller::stop(Reason reason) {
    state_ = State::Stop;
    reason_ = reason;
    manual_ = Output();
    confirmations_ = 0;
    if (ultrasonicCycle_ == UltrasonicCycle::Running) ultrasonicCycle_ = UltrasonicCycle::Interrupted;
    ++epoch_;
    sequence_ = 0;
}
void Controller::forceFault(Reason reason) {
    stop(reason);
    state_ = State::Fault;
}
bool Controller::ready(const Inputs& in) const {
    return in.hardwareReady && !in.jam && !in.binFull && !in.obstacleNear && (!cfg_.requireBattery ||
        (in.batteryValid && !in.batteryLow && in.batteryRecovered));
}
bool Controller::visionFresh(const Detection& d, uint32_t now) const {
    return d.healthy && d.frame != 0 && elapsed(now, d.capturedAt) <= cfg_.visionTimeoutMs;
}
bool Controller::command(const Command& c, uint32_t now, const Inputs& in) {
    if (c.action == Action::Stop) {
        // A stop must not clear a latched fault.
        if (state_ != State::Fault) stop(Reason::UserStop);
        return true;
    }
    if (c.epoch != epoch_) return false;
    if (c.action == Action::ClearFault) {
        if (state_ != State::Fault || !ready(in)) return false;
        stop(Reason::None);
        return true;
    }
    if (c.action == Action::Manual || c.action == Action::Auto) {
        if (state_ != State::Stop || !ready(in)) return false;
        if (c.action == Action::Auto && (!in.cameraReady || !in.detectorReady ||
            !visionFresh(in.detection, now))) return false;
        ++epoch_;
        sequence_ = 0;
        lastContact_ = stateSince_ = now;
        resetTracking(now);
        manual_ = Output();
        reason_ = Reason::None;
        ultrasonicCycle_ = UltrasonicCycle::Idle;
        state_ = c.action == Action::Manual ? State::Manual : State::Search;
        return true;
    }
    if (!active() || c.sequence <= sequence_) return false;
    // A late HTTP command cannot revive an expired control session.
    if (elapsed(now, lastContact_) >= cfg_.linkTimeoutMs) {
        stop(Reason::LinkTimeout);
        return false;
    }
    if (c.action == Action::Drive) {
        if (state_ != State::Manual || c.left < -255 || c.left > 255 ||
            c.right < -255 || c.right > 255 || c.conveyor < -255 || c.conveyor > 255) return false;
        manual_.left = clamp(c.left, cfg_.maxDrive);
        manual_.right = clamp(c.right, cfg_.maxDrive);
        manual_.conveyor = c.conveyor;
    } else if (c.action != Action::Heartbeat || state_ == State::Manual) {
        // Manual leases are renewed by a full Drive command, never a heartbeat.
        return false;
    }
    sequence_ = c.sequence;
    lastContact_ = now;
    return true;
}
Output Controller::tick(uint32_t now, const Inputs& in) {
    Output out;
    if (!active()) return out;
    if (!in.hardwareReady) { forceFault(Reason::Hardware); return out; }
    if (cfg_.requireBattery && !in.batteryValid) { forceFault(Reason::BatteryInvalid); return out; }
    if (cfg_.requireBattery && in.batteryLow) { forceFault(Reason::BatteryLow); return out; }
    if (in.jam) { forceFault(Reason::Jam); return out; }
    if (in.binFull) { forceFault(Reason::BinFull); return out; }
    if (elapsed(now, lastContact_) >= cfg_.linkTimeoutMs) { stop(Reason::LinkTimeout); return out; }
    if (automatic()) {
        if (!in.cameraReady) { forceFault(Reason::CameraUnavailable); return out; }
        if (!in.detectorReady) { forceFault(Reason::VisionUnavailable); return out; }
        if (!visionFresh(in.detection, now)) { stop(Reason::VisionStale); return out; }
    }
    if (in.obstacleNear && ultrasonicCycle_ == UltrasonicCycle::Idle) {
        ultrasonicCycle_ = UltrasonicCycle::Running;
        ultrasonicCollectSince_ = now;
    }
    if (ultrasonicCycle_ == UltrasonicCycle::Running) {
        if (elapsed(now, ultrasonicCollectSince_) >= cfg_.ultrasonicCollectMs) {
            ultrasonicCycle_ = UltrasonicCycle::Finished;
            forceFault(Reason::Obstacle);
            return out;
        }
        out.conveyor = cfg_.conveyorSpeed;
        return out;
    }
    if (state_ == State::Manual) {
        out = manual_;
    } else {
        if (state_ == State::Collect) {
            if (elapsed(now, stateSince_) >= cfg_.collectMs) {
                // Counts attempts, NOT confirmed objects collected. Require explicit restart.
                ++collectionCycles_;
                stop(Reason::None);
                return out;
            }
            out.left = out.right = cfg_.collectionSpeed;
            out.conveyor = cfg_.conveyorSpeed;
        } else {
            const Detection& d = in.detection;
            const bool target = d.found && std::isfinite(d.x) && std::isfinite(d.y) &&
                std::isfinite(d.confidence) && d.x >= 0 && d.x <= 1 && d.y >= 0 && d.y <= 1 &&
                d.confidence >= cfg_.minConfidence && d.confidence <= 1;
            if (!target) {
                confirmations_ = 0;
                if (state_ != State::Search) {
                    state_ = State::Search;
                    searchSince_ = now;
                    // Do not move on the frame that loses an acquired target.
                    return out;
                }
                if (elapsed(now, searchSince_) >= cfg_.searchTimeoutMs) {
                    stop(Reason::SearchTimeout); return out;
                }
                out.left = cfg_.searchSpeed;
                out.right = -cfg_.searchSpeed;
            } else {
                if (lastFrame_ != d.frame) {
                    if (confirmations_ && (std::fabs(d.x - previousX_) > cfg_.trackingRadius ||
                        std::fabs(d.y - previousY_) > cfg_.trackingRadius)) confirmations_ = 0;
                    if (confirmations_ < cfg_.confirmFrames) ++confirmations_;
                    previousX_ = d.x;
                    previousY_ = d.y;
                    lastFrame_ = d.frame;
                }
                if (confirmations_ < cfg_.confirmFrames) {
                    if (state_ != State::Search) { state_ = State::Search; searchSince_ = now; }
                    if (elapsed(now, searchSince_) >= cfg_.searchTimeoutMs) stop(Reason::SearchTimeout);
                    return out;
                }
                if (state_ == State::Search) targetSince_ = now;
                if (elapsed(now, targetSince_) >= cfg_.approachTimeoutMs) {
                    stop(Reason::ApproachTimeout); return out;
                }
                const float error = d.x - 0.5f;
                const bool centered = std::fabs(error) <= cfg_.centerDeadband;
                if (centered && (d.y >= cfg_.collectY || in.intake)) {
                    state_ = State::Collect;
                    stateSince_ = now;
                    out.left = out.right = cfg_.collectionSpeed;
                    out.conveyor = cfg_.conveyorSpeed;
                } else if (centered) {
                    state_ = State::Approach;
                    out.left = out.right = cfg_.approachSpeed;
                } else {
                    state_ = State::Align;
                    const int turn = error > 0 ? cfg_.turnCorrection : -cfg_.turnCorrection;
                    out.left = cfg_.approachSpeed + turn;
                    out.right = cfg_.approachSpeed - turn;
                }
            }
        }
    }
    out.left = clamp(out.left, cfg_.maxDrive);
    out.right = clamp(out.right, cfg_.maxDrive);
    return out;
}
const char* name(State s) {
    switch (s) {
    case State::Stop: return "STOP"; case State::Manual: return "MANUAL";
    case State::Search: return "SEARCH"; case State::Align: return "ALIGN";
    case State::Approach: return "APPROACH"; case State::Collect: return "COLLECT";
    case State::Fault: return "FAULT";
    }
    return "UNKNOWN";
}
const char* name(Reason r) {
    switch (r) {
    case Reason::None: return "none"; case Reason::UserStop: return "user_stop";
    case Reason::LinkTimeout: return "link_timeout"; case Reason::BatteryInvalid: return "battery_invalid";
    case Reason::BatteryLow: return "battery_low"; case Reason::CameraUnavailable: return "camera_unavailable";
    case Reason::VisionUnavailable: return "vision_unavailable"; case Reason::VisionStale: return "vision_stale";
    case Reason::SearchTimeout: return "search_timeout"; case Reason::ApproachTimeout: return "approach_timeout";
    case Reason::BinFull: return "bin_full"; case Reason::Obstacle: return "obstacle";
    case Reason::Jam: return "jam";
    case Reason::Hardware: return "hardware"; case Reason::InvalidCommand: return "invalid_command";
    }
    return "unknown";
}
const char* name(UltrasonicCycle cycle) {
    switch (cycle) {
    case UltrasonicCycle::Idle: return "idle";
    case UltrasonicCycle::Running: return "running";
    case UltrasonicCycle::Finished: return "finished";
    case UltrasonicCycle::Interrupted: return "interrupted";
    }
    return "unknown";
}
} // namespace boat
