#include "sd_recorder.h"
#include "app_config.h"
#include "board_config.h"
#include <FS.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>

namespace {
File indexFile; // Writer task only.

unsigned sessionNumber(const char* name) {
    // Accept "cap_0012" or "/cap_0012".
    if (*name == '/') ++name;
    if (strncmp(name, "cap_", 4) != 0) return 0;
    char* end = nullptr;
    const unsigned long n = strtoul(name + 4, &end, 10);
    return (end && *end == '\0' && n < 100000) ? static_cast<unsigned>(n) : 0;
}
}

bool SdRecorder::begin() {
    if (!board::sdEnabled) return false;
    mutex_ = xSemaphoreCreateMutex();
    queue_ = xQueueCreate(config::sdQueueDepth, sizeof(Frame));
    if (!mutex_ || !queue_) { Serial.println("SD: queue/mutex allocation failed"); return false; }
    if (!SD_MMC.setPins(board::sdClk, board::sdCmd, board::sdData0)) {
        Serial.println("SD: invalid pins");
        strlcpy(status_.error, "invalid_pins", sizeof(status_.error));
        return false;
    }
    // 1-bit mode, do not format on failure, default (20 MHz) bus clock.
    if (!SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 3)) {
        Serial.println("SD: no card or mount failed (card must be FAT32; exFAT is not supported)");
        strlcpy(status_.error, "no_card", sizeof(status_.error));
        return false;
    }
    File root = SD_MMC.open("/");
    for (File entry = root.openNextFile(); entry; entry = root.openNextFile()) {
        if (entry.isDirectory()) {
            const unsigned n = sessionNumber(entry.name());
            if (n >= nextSession_) nextSession_ = n + 1;
        }
        entry.close();
    }
    root.close();
    status_.mounted = true;
    refreshUsage();
    Serial.printf("SD: mounted | %llu MB | used %llu MB | next session cap_%04u\n",
        status_.totalBytes >> 20, status_.usedBytes >> 20, nextSession_);
    // Core 0 next to the camera task; never competes with the motor task on core 1.
    if (xTaskCreatePinnedToCore(taskEntry, "sd", 6144, this, 1, nullptr, 0) != pdPASS) {
        Serial.println("SD: writer task allocation failed");
        status_.mounted = false;
        return false;
    }
    return true;
}

void SdRecorder::refreshUsage() {
    status_.totalBytes = SD_MMC.totalBytes();
    status_.usedBytes = SD_MMC.usedBytes();
}

bool SdRecorder::start(uint32_t intervalMs) {
    if (!mutex_ || (intervalMs != 0 && intervalMs < config::sdMinIntervalMs)) return false;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(200)) != pdTRUE) return false;
    bool ok = status_.mounted && !recording_.load();
    if (ok && status_.totalBytes - status_.usedBytes < config::sdMinFreeBytes) {
        strlcpy(status_.error, "card_full", sizeof(status_.error));
        ok = false;
    }
    char folder[sizeof(status_.folder)];
    if (ok) {
        snprintf(folder, sizeof(folder), "/cap_%04u", nextSession_);
        ok = SD_MMC.mkdir(folder);
        if (!ok) strlcpy(status_.error, "mkdir_failed", sizeof(status_.error));
    }
    if (ok) {
        strlcpy(status_.folder, folder, sizeof(status_.folder));
        status_.error[0] = '\0';
        status_.saved = status_.dropped = status_.failures = 0;
        status_.intervalMs = intervalMs;
        intervalMs_.store(intervalMs);
        session_.store(nextSession_++);
        lastOfferAt_ = millis() - intervalMs;
        recording_.store(true);
        status_.recording = true;
        if (intervalMs) Serial.printf("SD: recording to %s every %lu ms\n", folder, static_cast<unsigned long>(intervalMs));
        else Serial.printf("SD: manual shots go to %s\n", folder);
    }
    xSemaphoreGive(mutex_);
    return ok;
}

void SdRecorder::stop() {
    if (!mutex_ || !recording_.exchange(false)) return;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(200)) == pdTRUE) {
        status_.recording = false;
        xSemaphoreGive(mutex_);
    }
    // Ask the writer to close index.csv. If the queue is full the file is still
    // flushed after every line, so nothing is lost.
    shotPending_.store(false);
    const Frame close{nullptr, 0, millis(), session_.load(), false};
    xQueueSend(queue_, &close, pdMS_TO_TICKS(50));
    Serial.println("SD: recording stopped");
}

bool SdRecorder::setInterval(uint32_t intervalMs) {
    if (!mutex_ || intervalMs < config::sdMinIntervalMs || !recording_.load()) return false;
    intervalMs_.store(intervalMs);
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(200)) == pdTRUE) {
        status_.intervalMs = intervalMs;
        xSemaphoreGive(mutex_);
    }
    return true;
}

bool SdRecorder::requestShot() {
    if (!recording_.load() && !start(0)) return false;
    shotPending_.store(true);
    return true;
}

void SdRecorder::offer(const uint8_t* jpeg, size_t length) {
    if (!recording_.load() || !jpeg || !length) return;
    const uint32_t now = millis();
    const bool manual = shotPending_.exchange(false);
    const uint32_t interval = intervalMs_.load();
    if (!manual && (interval == 0 || now - lastOfferAt_ < interval)) return;
    if (!manual) lastOfferAt_ = now;
    auto* copy = static_cast<uint8_t*>(heap_caps_malloc(length, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    const Frame frame{copy, length, now, session_.load(), manual};
    if (copy) memcpy(copy, jpeg, length);
    if (!copy || xQueueSend(queue_, &frame, 0) != pdTRUE) {
        free(copy);
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(5)) == pdTRUE) {
            ++status_.dropped;
            xSemaphoreGive(mutex_);
        }
    }
}

SdStatus SdRecorder::status() {
    SdStatus copy;
    if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
        copy = status_;
        xSemaphoreGive(mutex_);
    } else if (!mutex_) {
        copy = status_; // begin() failed before the mutex existed; status_ is static.
    }
    return copy;
}

void SdRecorder::finishSession() {
    if (indexFile) indexFile.close();
    openSession_ = 0;
}

void SdRecorder::fail(const char* reason) {
    recording_.store(false);
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        status_.recording = false;
        strlcpy(status_.error, reason, sizeof(status_.error));
        xSemaphoreGive(mutex_);
    }
    finishSession();
    Serial.printf("SD: recording stopped (%s)\n", reason);
}

void SdRecorder::taskEntry(void* arg) { static_cast<SdRecorder*>(arg)->task(); }

void SdRecorder::task() {
    Frame frame;
    char folder[sizeof(status_.folder)] = "";
    for (;;) {
        if (xQueueReceive(queue_, &frame, portMAX_DELAY) != pdTRUE) continue;
        if (!frame.data) {
            if (frame.session == openSession_) finishSession();
            continue;
        }
        if (frame.session != openSession_) {
            // Open a session only while it is the active recording; frames left in the
            // queue from a failed or replaced session are discarded.
            if (!recording_.load() || frame.session != session_.load()) { free(frame.data); continue; }
            finishSession();
            snprintf(folder, sizeof(folder), "/cap_%04u", frame.session);
            char path[40];
            snprintf(path, sizeof(path), "%s/index.csv", folder);
            indexFile = SD_MMC.open(path, FILE_WRITE);
            if (!indexFile) { free(frame.data); fail("open_failed"); continue; }
            indexFile.println("file,ms_since_boot,bytes,trigger");
            openSession_ = frame.session;
            fileIndex_ = 0;
            consecutiveFailures_ = 0;
        }
        char name[16], path[40];
        snprintf(name, sizeof(name), "%05lu.jpg", static_cast<unsigned long>(fileIndex_));
        snprintf(path, sizeof(path), "%s/%s", folder, name);
        File out = SD_MMC.open(path, FILE_WRITE);
        const bool ok = out && out.write(frame.data, frame.length) == frame.length;
        if (out) out.close();
        free(frame.data);
        if (ok) {
            ++fileIndex_;
            consecutiveFailures_ = 0;
            indexFile.printf("%s,%lu,%u,%s\n", name, static_cast<unsigned long>(frame.at),
                static_cast<unsigned>(frame.length), frame.manual ? "manual" : "auto");
            indexFile.flush();
        }
        const bool checkSpace = ok && fileIndex_ % 25 == 0;
        uint64_t total = 0, used = 0;
        if (checkSpace) { total = SD_MMC.totalBytes(); used = SD_MMC.usedBytes(); }
        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            if (ok) ++status_.saved; else ++status_.failures;
            if (checkSpace) { status_.totalBytes = total; status_.usedBytes = used; }
            xSemaphoreGive(mutex_);
        }
        if (!ok && ++consecutiveFailures_ >= 3) fail("write_failed");
        else if (checkSpace && total - used < config::sdMinFreeBytes) fail("card_full");
    }
}
