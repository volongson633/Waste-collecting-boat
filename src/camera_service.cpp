#include "camera_service.h"
#include "board_config.h"
#include "app_config.h"
#include "vision_service.h"
#include "sd_recorder.h"
#include <esp_camera.h>
#include <img_converters.h>
#include <esp_heap_caps.h>
#include <esp_err.h>

bool CameraService::begin() {
    Serial.printf("Camera: starting | PSRAM=%u | free PSRAM=%u\n",
        ESP.getPsramSize(), ESP.getFreePsram());
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_ || !psramFound()) { Serial.println("Camera: PSRAM/mutex unavailable"); return false; }
    jpeg_ = static_cast<uint8_t*>(heap_caps_malloc(config::snapshotCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (vision::available()) bgr_ = static_cast<uint8_t*>(heap_caps_malloc(
        config::cameraWidth * config::cameraHeight * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!jpeg_ || (vision::available() && !bgr_)) {
        Serial.println("Camera: JPEG/vision buffer allocation failed");
        return false;
    }
    camera_config_t c = {};
    c.ledc_channel = LEDC_CHANNEL_0; c.ledc_timer = LEDC_TIMER_0;
    c.pin_d0 = board::cameraD0; c.pin_d1 = board::cameraD1;
    c.pin_d2 = board::cameraD2; c.pin_d3 = board::cameraD3;
    c.pin_d4 = board::cameraD4; c.pin_d5 = board::cameraD5;
    c.pin_d6 = board::cameraD6; c.pin_d7 = board::cameraD7;
    c.pin_xclk = board::cameraXclk; c.pin_pclk = board::cameraPclk;
    c.pin_vsync = board::cameraVsync; c.pin_href = board::cameraHref;
    c.pin_sccb_sda = board::cameraSiod; c.pin_sccb_scl = board::cameraSioc;
    c.pin_pwdn = board::cameraPwdn; c.pin_reset = board::cameraReset;
    c.xclk_freq_hz = 20000000;
    c.pixel_format = PIXFORMAT_JPEG;
    // Allocate buffers for the largest selectable resolution before lowering
    // the default sensor output; later UXGA selection must not overflow VGA buffers.
    c.frame_size = vision::available() ? FRAMESIZE_QVGA : FRAMESIZE_UXGA;
    c.jpeg_quality = config::cameraJpegQuality; c.fb_count = 3;
    c.fb_location = CAMERA_FB_IN_PSRAM; c.grab_mode = CAMERA_GRAB_LATEST;
    const esp_err_t err = esp_camera_init(&c);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: %s (0x%x)\n", esp_err_to_name(err), err);
        return false;
    }
    sensor_t* sensor = esp_camera_sensor_get();
    if (!sensor) {
        Serial.println("Camera: sensor unavailable after initialization");
        esp_camera_deinit();
        return false;
    }
    Serial.printf("Camera: sensor PID=0x%04x\n", sensor->id.PID);
    if (!vision::available()) sensor->set_framesize(sensor, FRAMESIZE_VGA);
    sensor->set_hmirror(sensor, config::cameraMirror);
    sensor->set_vflip(sensor, config::cameraFlip);
    httpd_config_t httpConfig = HTTPD_DEFAULT_CONFIG();
    httpConfig.server_port = 82;
    httpConfig.ctrl_port = 32769;
    httpConfig.max_open_sockets = 3;
    httpConfig.lru_purge_enable = true;
    httpConfig.send_wait_timeout = 1;
    httpConfig.recv_wait_timeout = 1;
    if (httpd_start(&http_, &httpConfig) == ESP_OK) {
        httpd_uri_t uri = {};
        uri.uri = "/capture"; uri.method = HTTP_GET;
        uri.handler = snapshotHandler; uri.user_ctx = this;
        httpd_register_uri_handler(http_, &uri);
    } else Serial.println("Snapshot HTTP server unavailable");
    // Dedicated HTTP task: a continuous stream must never block control APIs
    // or snapshot downloads. Both readers copy the camera task's latest JPEG.
    httpd_config_t streamConfig = HTTPD_DEFAULT_CONFIG();
    streamConfig.server_port = 81;
    streamConfig.ctrl_port = 32770;
    streamConfig.max_open_sockets = 2;
    streamConfig.stack_size = 8192;
    streamConfig.send_wait_timeout = 1;
    streamConfig.recv_wait_timeout = 1;
    streamConfig.lru_purge_enable = true;
    if (httpd_start(&streamHttp_, &streamConfig) == ESP_OK) {
        httpd_uri_t uri = {};
        uri.uri = "/stream"; uri.method = HTTP_GET;
        uri.handler = streamHandler; uri.user_ctx = this;
        if (httpd_register_uri_handler(streamHttp_, &uri) != ESP_OK)
            Serial.println("Stream route unavailable");
        else Serial.println("Camera: stream listening on :81/stream");
    } else Serial.println("Stream HTTP server unavailable");
    status_.ready = true;
    status_.detectorReady = vision::available();
    if (xTaskCreatePinnedToCore(taskEntry, "camera", 16384, this, 1, nullptr, 0) != pdPASS) {
        Serial.println("Camera: capture task allocation failed");
        status_.ready = false;
        return false;
    }
    initialized_.store(true);
    return true;
}
CameraStatus CameraService::status() {
    CameraStatus result;
    if (initialized_.load() && xSemaphoreTake(mutex_, pdMS_TO_TICKS(5)) == pdTRUE) {
        result = status_;
        xSemaphoreGive(mutex_);
    }
    return result;
}

esp_err_t CameraService::streamHandler(httpd_req_t* req) {
    auto* self = static_cast<CameraService*>(req->user_ctx);
    auto* copy = static_cast<uint8_t*>(heap_caps_malloc(
        config::snapshotCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!copy) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No stream buffer");
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    uint32_t lastAt = 0;
    bool sentFrame = false;
    uint32_t lastFresh = millis();
    esp_err_t result = ESP_OK;
    while (result == ESP_OK) {
        size_t length = 0;
        if (xSemaphoreTake(self->mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (self->jpegLength_ && millis() - self->jpegAt_ <= 2000 &&
                (!sentFrame || self->jpegAt_ != lastAt)) {
                length = self->jpegLength_;
                memcpy(copy, self->jpeg_, length);
                lastAt = self->jpegAt_;
            }
            xSemaphoreGive(self->mutex_);
        }
        if (!length) {
            // Exit even when the sensor fails, so a dead client cannot own
            // the stream server forever without any socket writes.
            if (millis() - lastFresh > 3000) { result = ESP_FAIL; break; }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        lastFresh = millis();
        sentFrame = true;
        char header[96];
        const int size = snprintf(header, sizeof(header),
            "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
            static_cast<unsigned>(length));
        result = httpd_resp_send_chunk(req, header, size);
        if (result == ESP_OK)
            result = httpd_resp_send_chunk(req, reinterpret_cast<char*>(copy), length);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    free(copy);
    return result;
}
void CameraService::taskEntry(void* arg) { static_cast<CameraService*>(arg)->task(); }
void CameraService::task() {
    uint32_t frame = 0;
    for (;;) {
        const uint32_t started = millis();
        camera_fb_t* fb = esp_camera_fb_get();
        CameraStatus next;
        next.ready = fb != nullptr;
        next.detectorReady = vision::available();
        if (fb) {
            if (!frame) Serial.printf("Camera: first frame | %ux%u | %u bytes\n",
                fb->width, fb->height, static_cast<unsigned>(fb->len));
            next.detection.frame = ++frame;
            if (frame == 0) next.detection.frame = ++frame;
            next.detection.capturedAt = static_cast<uint32_t>(uint64_t(fb->timestamp.tv_sec) * 1000ULL + fb->timestamp.tv_usec / 1000);
            if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
                if (fb->format == PIXFORMAT_JPEG && fb->len <= config::snapshotCapacity) {
                    memcpy(jpeg_, fb->buf, fb->len); jpegLength_ = fb->len;
                    jpegAt_ = next.detection.capturedAt;
                }
                xSemaphoreGive(mutex_);
            }
            // Copies at most one frame per recording interval; card I/O runs elsewhere.
            if (recorder_ && fb->format == PIXFORMAT_JPEG) recorder_->offer(fb->buf, fb->len);
            if (vision::available() && fb->width == config::cameraWidth && fb->height == config::cameraHeight &&
                fmt2rgb888(fb->buf, fb->len, fb->format, bgr_)) {
                esp_camera_fb_return(fb); fb = nullptr;
                next.detection.healthy = vision::detect(bgr_, config::cameraWidth, config::cameraHeight, next.detection);
            }
            if (fb) esp_camera_fb_return(fb);
        }
        next.processingMs = millis() - started;
        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            next.captureFailures = status_.captureFailures + (next.ready ? 0 : 1);
            status_ = next;
            xSemaphoreGive(mutex_);
        }
        const uint32_t used = millis() - started;
        vTaskDelay(pdMS_TO_TICKS(used < config::cameraPeriodMs ? config::cameraPeriodMs - used : 10));
    }
}
esp_err_t CameraService::snapshotHandler(httpd_req_t* req) {
    auto* self = static_cast<CameraService*>(req->user_ctx);
    uint8_t* copy = static_cast<uint8_t*>(heap_caps_malloc(config::snapshotCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!copy) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No snapshot buffer");
    size_t length = 0;
    if (xSemaphoreTake(self->mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
        if (millis() - self->jpegAt_ <= 2000) {
            length = self->jpegLength_;
            memcpy(copy, self->jpeg_, length);
        }
        xSemaphoreGive(self->mutex_);
    }
    esp_err_t result;
    if (length) {
        httpd_resp_set_type(req, "image/jpeg");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        result = httpd_resp_send(req, reinterpret_cast<char*>(copy), length);
    } else {
        httpd_resp_set_status(req, "503 Service Unavailable");
        result = httpd_resp_sendstr(req, "No fresh frame");
    }
    free(copy);
    return result;
}
