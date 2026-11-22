#pragma once
#include "boat/Controller.h"
#include <Arduino.h>
#include <esp_http_server.h>
#include <atomic>

struct CameraStatus {
    bool ready = false;
    bool detectorReady = false;
    uint32_t processingMs = 0;
    uint32_t captureFailures = 0;
    boat::Detection detection;
};
class CameraService {
public:
    bool begin();
    CameraStatus status();
private:
    static void taskEntry(void* arg);
    void task();
    static esp_err_t snapshotHandler(httpd_req_t* request);
    static esp_err_t streamHandler(httpd_req_t* request);
    SemaphoreHandle_t mutex_ = nullptr;
    httpd_handle_t http_ = nullptr;
    httpd_handle_t streamHttp_ = nullptr;
    uint8_t* jpeg_ = nullptr;
    uint8_t* bgr_ = nullptr;
    size_t jpegLength_ = 0;
    uint32_t jpegAt_ = 0;
    CameraStatus status_;
    std::atomic<bool> initialized_{false};
};
