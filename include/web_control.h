#pragma once
#include "runtime.h"
#include <WebServer.h>

class WebControl {
public:
    explicit WebControl(Runtime& runtime) : runtime_(runtime), server_(80) {}
    bool begin();
    void poll() { server_.handleClient(); }
private:
    void sendStatus();
    void handleCommand();
    bool acceptJsonRequest();
    void handleLights();
    void handlePhoneMotor();
    void cameraRoutes();
    uint32_t phoneEpoch_ = 0;
    bool phoneOwned_ = false;
    Runtime& runtime_;
    WebServer server_;
    String controlKey_;
};
