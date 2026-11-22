#pragma once
#include <Arduino.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

struct SdStatus {
    bool mounted = false;
    bool recording = false;
    uint32_t intervalMs = 0;
    uint32_t saved = 0;     // Frames written in the current/last session.
    uint32_t dropped = 0;   // Frames skipped because the card was slower than the camera.
    uint32_t failures = 0;  // Write errors in the current/last session.
    uint64_t totalBytes = 0;
    uint64_t usedBytes = 0;
    char folder[16] = "";   // e.g. "/cap_0003"
    char error[32] = "";
};

// Records JPEG frames from the camera to the on-board microSD slot (SD_MMC 1-bit).
// The camera task only copies a frame into a queue; a separate task does all card I/O.
class SdRecorder {
public:
    bool begin();
    // intervalMs == 0 opens a session for manual shots only (no timed capture).
    bool start(uint32_t intervalMs);
    void stop();
    // Change the timed-capture interval of the running session.
    bool setInterval(uint32_t intervalMs);
    // Save the next camera frame; opens a manual-only session if none is running.
    bool requestShot();
    // Camera task only. Takes at most one frame per interval, plus requested shots.
    void offer(const uint8_t* jpeg, size_t length);
    SdStatus status();
private:
    // data == nullptr asks the writer to close the session's files.
    struct Frame { uint8_t* data; size_t length; uint32_t at; unsigned session; bool manual; };
    static void taskEntry(void* arg);
    void task();
    void finishSession();
    void fail(const char* reason);
    void refreshUsage();
    SemaphoreHandle_t mutex_ = nullptr;
    QueueHandle_t queue_ = nullptr;
    SdStatus status_;
    std::atomic<bool> recording_{false};
    std::atomic<uint32_t> intervalMs_{0};
    std::atomic<uint32_t> session_{0};
    std::atomic<bool> shotPending_{false};
    uint32_t lastOfferAt_ = 0;   // Camera task only.
    unsigned nextSession_ = 1;
    unsigned openSession_ = 0;   // Writer task only: session whose index.csv is open.
    uint32_t fileIndex_ = 0;     // Writer task only.
    unsigned consecutiveFailures_ = 0;
};
