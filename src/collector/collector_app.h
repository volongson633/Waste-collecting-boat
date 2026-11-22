#pragma once
#if defined(BOAT_COLLECTOR) && BOAT_COLLECTOR
#include "app_config.h"
#include "runtime.h"
#include "sd_recorder.h"
#include <WebServer.h>

// Data-collection firmware: camera stream, manual driving with manual shots, and an
// auto mode that drives a fixed pattern while saving frames to the SD card at a set
// interval. All web handlers and the auto pattern run in loop(), one thread.
class CollectorApp {
public:
    enum class Mode { Idle, Manual, Auto };
    CollectorApp(Runtime& runtime, SdRecorder& sd) : runtime_(runtime), sd_(sd), server_(80) {}
    bool beginWifi();
    void beginServer();
    void poll();
private:
    bool authorized();
    bool arm(String& error);
    void stopAll(const char* reason);
    void tickAuto(uint32_t now);
    void handleState();
    void handleManual();
    void handleAuto();
    void handleShot();
    void handleCamera();
    void handleClear();
    bool readUnsigned(const char* name, unsigned long min, unsigned long max, unsigned long& value);
    Runtime& runtime_;
    SdRecorder& sd_;
    WebServer server_;
    Mode mode_ = Mode::Idle;
    bool owned_ = false;          // We armed the controller and hold its session.
    uint32_t epoch_ = 0;
    uint32_t lastPageAt_ = 0;
    uint32_t autoSince_ = 0;
    uint32_t lastSendAt_ = 0;
    unsigned speedPercent_ = config::collectorDefaultSpeedPercent;
    uint32_t intervalMs_ = config::sdDefaultIntervalMs;
    bool turning_ = false;
    String stopReason_;
};
#endif
