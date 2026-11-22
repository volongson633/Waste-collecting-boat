#if defined(BOAT_COLLECTOR) && BOAT_COLLECTOR
#include "collector_app.h"
#include "board_config.h"
#include "collector_page.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_camera.h>

namespace {
const char* modeName(CollectorApp::Mode mode) {
    switch (mode) {
    case CollectorApp::Mode::Manual: return "manual";
    case CollectorApp::Mode::Auto: return "auto";
    default: return "idle";
    }
}
int scaled(unsigned percent) { return static_cast<int>((percent * boat::Settings().maxDrive + 50) / 100); }
}

bool CollectorApp::beginWifi() {
    Serial.println("Wi-Fi: starting access point...");
    if (!WiFi.mode(strlen(WIFI_SSID) ? WIFI_AP_STA : WIFI_AP)) return false;
    WiFi.setSleep(false);
    if (!WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0))) return false;
    if (!WiFi.softAP(BOAT_AP_SSID, BOAT_AP_PASSWORD, 1, false, 2)) return false;
    if (strlen(WIFI_SSID)) WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("Wi-Fi: AP %s | http://192.168.4.1 | stream :81/stream\n", BOAT_AP_SSID);
    return true;
}

void CollectorApp::beginServer() {
    const char* headers[] = {"X-Motor-Control"};
    server_.collectHeaders(headers, 1);
    server_.on("/", HTTP_GET, [this]() {
        server_.sendHeader("Cache-Control", "no-store");
        server_.send_P(200, "text/html; charset=utf-8", COLLECTOR_PAGE);
    });
    server_.on("/api/state", HTTP_GET, [this]() { handleState(); });
    server_.on("/api/manual", HTTP_POST, [this]() { handleManual(); });
    server_.on("/api/auto", HTTP_POST, [this]() { handleAuto(); });
    server_.on("/api/shot", HTTP_POST, [this]() { handleShot(); });
    server_.on("/api/camera", HTTP_POST, [this]() { handleCamera(); });
    server_.on("/api/clear", HTTP_POST, [this]() { handleClear(); });
    server_.on("/api/heartbeat", HTTP_POST, [this]() {
        if (!authorized()) return;
        lastPageAt_ = millis();
        server_.send(200, "text/plain", "OK");
    });
    server_.on("/api/stop", HTTP_POST, [this]() {
        // No header check: a stop must always get through (also sent on page close).
        stopAll("user_stop");
        server_.send(200, "text/plain", "OK");
    });
    server_.onNotFound([this]() { server_.send(404, "text/plain", "not_found"); });
    server_.begin();
}

void CollectorApp::poll() {
    server_.handleClient();
    const uint32_t now = millis();
    if (mode_ == Mode::Manual) {
        // The controller ends a manual lease 1 s after the last drive command.
        const auto st = runtime_.status();
        if (st.state != boat::State::Manual || st.epoch != epoch_) { mode_ = Mode::Idle; owned_ = false; }
    }
    tickAuto(now);
}

bool CollectorApp::authorized() {
    if (server_.header("X-Motor-Control") == "1") return true;
    server_.send(403, "text/plain", "missing_header");
    return false;
}

bool CollectorApp::readUnsigned(const char* name, unsigned long min, unsigned long max, unsigned long& value) {
    const String text = server_.arg(name);
    char* end = nullptr;
    const unsigned long n = strtoul(text.c_str(), &end, 10);
    if (!text.length() || *end || n < min || n > max) return false;
    value = n;
    return true;
}

bool CollectorApp::arm(String& error) {
    auto st = runtime_.status();
    if (owned_ && st.epoch == epoch_ && st.state == boat::State::Manual) return true;
    boat::Command command;
    command.action = boat::Action::Manual;
    command.epoch = st.epoch;
    if (!runtime_.command(command)) {
        st = runtime_.status();
        error = String("locked:") + boat::name(st.state) + ":" + boat::name(st.reason);
        return false;
    }
    epoch_ = runtime_.status().epoch;
    owned_ = true;
    return true;
}

void CollectorApp::stopAll(const char* reason) {
    runtime_.command(boat::Command());
    if (mode_ == Mode::Auto) sd_.stop();
    if (mode_ != Mode::Idle) Serial.printf("[COLLECT] %s -> idle (%s)\n", modeName(mode_), reason);
    mode_ = Mode::Idle;
    owned_ = false;
    turning_ = false;
    stopReason_ = reason;
}

void CollectorApp::tickAuto(uint32_t now) {
    if (mode_ != Mode::Auto) return;
    if (now - lastPageAt_ > config::collectorLinkTimeoutMs) { stopAll("link_lost"); return; }
    if (now - lastSendAt_ < config::collectorCommandMs) return;
    lastSendAt_ = now;
    const auto st = runtime_.status();
    // Obstacle, jam, bin full or any fault ends the run; the operator decides what next.
    if (st.state != boat::State::Manual || st.epoch != epoch_) {
        stopAll(st.reason == boat::Reason::None ? "controller_stopped" : boat::name(st.reason));
        return;
    }
    const SdStatus sd = sd_.status();
    if (!sd.recording) { stopAll(sd.error[0] ? sd.error : "sd_stopped"); return; }
    const uint32_t cycle = config::collectorStraightMs + config::collectorTurnMs;
    const uint32_t elapsed = now - autoSince_;
    turning_ = elapsed % cycle >= config::collectorStraightMs;
    const int speed = scaled(speedPercent_);
    boat::Command drive;
    drive.action = boat::Action::Drive;
    drive.epoch = epoch_;
    drive.sequence = st.sequence + 1;
    drive.left = drive.right = speed;
    if (turning_) {
        // Pivot, alternating left and right so the boat does not circle one way.
        const int turn = speed * static_cast<int>(config::collectorTurnPercent) / 100;
        const bool right = (elapsed / cycle) % 2 == 0;
        drive.left = right ? turn : -turn;
        drive.right = right ? -turn : turn;
    }
    if (!runtime_.command(drive)) stopAll("drive_rejected");
}

void CollectorApp::handleManual() {
    if (!authorized()) return;
    unsigned long dir, speed, belt;
    if (!readUnsigned("dir", 0, 4, dir) || !readUnsigned("speed", 0, 100, speed) ||
        !readUnsigned("belt", 0, 2, belt)) {
        server_.send(400, "text/plain", "expected dir=0..4 speed=0..100 belt=0..2"); return;
    }
    if (mode_ == Mode::Auto) { server_.send(409, "text/plain", "auto_running"); return; }
    lastPageAt_ = millis();
    speedPercent_ = speed;
    const auto st = runtime_.status();
    if (owned_ && (st.epoch != epoch_ || st.state != boat::State::Manual)) { owned_ = false; mode_ = Mode::Idle; }
    if (!owned_ && !dir && !belt) { server_.send(200, "text/plain", "OK"); return; }
    if (!config::outputsEnabled) { server_.send(409, "text/plain", "outputs_disabled"); return; }
    String error;
    if (!arm(error)) { server_.send(409, "text/plain", error); return; }
    mode_ = Mode::Manual;
    const int s = scaled(speed);
    boat::Command drive;
    drive.action = boat::Action::Drive;
    drive.epoch = epoch_;
    drive.sequence = runtime_.status().sequence + 1;
    if (dir == 1) drive.left = drive.right = s;
    else if (dir == 2) drive.left = drive.right = -s;
    else if (dir == 3) { drive.left = -s; drive.right = s; }
    else if (dir == 4) { drive.left = s; drive.right = -s; }
    drive.conveyor = belt == 1 ? 255 : belt == 2 ? -255 : 0;
    if (!runtime_.command(drive)) {
        owned_ = false; mode_ = Mode::Idle;
        server_.send(409, "text/plain", "session_expired"); return;
    }
    server_.send(200, "text/plain", "OK");
}

void CollectorApp::handleAuto() {
    if (!authorized()) return;
    const String action = server_.arg("action");
    if (action == "stop") {
        if (mode_ == Mode::Auto) stopAll("user_stop");
        server_.send(200, "text/plain", "OK"); return;
    }
    if (action != "start" && action != "set") { server_.send(400, "text/plain", "expected action=start|set|stop"); return; }
    unsigned long value;
    if (server_.hasArg("interval")) {
        if (!readUnsigned("interval", config::sdMinIntervalMs, 600000, value)) {
            server_.send(400, "text/plain", "interval 200..600000 ms"); return;
        }
        intervalMs_ = value;
        if (mode_ == Mode::Auto) sd_.setInterval(intervalMs_);
    }
    if (server_.hasArg("speed")) {
        if (!readUnsigned("speed", 0, 100, value)) { server_.send(400, "text/plain", "speed 0..100"); return; }
        speedPercent_ = value;
    }
    if (action == "set" || mode_ == Mode::Auto) { server_.send(200, "text/plain", "OK"); return; }
    if (!config::outputsEnabled) { server_.send(409, "text/plain", "outputs_disabled"); return; }
    if (!sd_.status().mounted) { server_.send(409, "text/plain", "no_card"); return; }
    if (owned_) { runtime_.command(boat::Command()); owned_ = false; mode_ = Mode::Idle; }
    String error;
    if (!arm(error)) { server_.send(409, "text/plain", error); return; }
    sd_.stop();
    if (!sd_.start(intervalMs_)) {
        runtime_.command(boat::Command());
        owned_ = false;
        const SdStatus sd = sd_.status();
        server_.send(409, "text/plain", sd.error[0] ? sd.error : "sd_start_failed"); return;
    }
    mode_ = Mode::Auto;
    autoSince_ = lastPageAt_ = millis();
    lastSendAt_ = 0;
    stopReason_ = "";
    Serial.printf("[COLLECT] auto start | speed %u%% | every %lu ms\n", speedPercent_,
        static_cast<unsigned long>(intervalMs_));
    tickAuto(millis());
    server_.send(200, "text/plain", "OK");
}

void CollectorApp::handleShot() {
    if (!authorized()) return;
    if (!sd_.requestShot()) {
        const SdStatus sd = sd_.status();
        server_.send(409, "text/plain", sd.error[0] ? sd.error : "no_card"); return;
    }
    server_.send(200, "text/plain", "OK");
}

void CollectorApp::handleCamera() {
    if (!authorized()) return;
    unsigned long size;
    if (!readUnsigned("framesize", FRAMESIZE_QVGA, FRAMESIZE_XGA, size) ||
        (size != FRAMESIZE_QVGA && size != FRAMESIZE_VGA && size != FRAMESIZE_SVGA && size != FRAMESIZE_XGA)) {
        server_.send(400, "text/plain", "framesize must be QVGA, VGA, SVGA or XGA"); return;
    }
    sensor_t* sensor = esp_camera_sensor_get();
    if (!sensor) { server_.send(503, "text/plain", "camera_unavailable"); return; }
    const bool ok = sensor->set_framesize(sensor, static_cast<framesize_t>(size)) == 0;
    server_.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "camera_rejected");
}

void CollectorApp::handleClear() {
    if (!authorized()) return;
    boat::Command clear;
    clear.action = boat::Action::ClearFault;
    clear.epoch = runtime_.status().epoch;
    const bool ok = runtime_.command(clear);
    if (ok) stopReason_ = "";
    server_.send(ok ? 200 : 409, "text/plain", ok ? "OK" : "fault_not_cleared");
}

void CollectorApp::handleState() {
    const auto st = runtime_.status();
    const SdStatus sd = sd_.status();
    StaticJsonDocument<1024> doc;
    doc["mode"] = modeName(mode_);
    doc["state"] = boat::name(st.state);
    doc["reason"] = boat::name(st.reason);
    doc["readyToArm"] = st.readyToArm;
    doc["outputsEnabled"] = config::outputsEnabled;
    doc["stopReason"] = stopReason_;
    doc["speed"] = speedPercent_;
    doc["intervalMs"] = intervalMs_;
    doc["phase"] = mode_ == Mode::Auto ? (turning_ ? "turn" : "straight") : "";
    doc["autoMs"] = mode_ == Mode::Auto ? millis() - autoSince_ : 0;
    doc["cameraReady"] = st.camera.ready;
    doc["obstacleCm"] = st.sensors.obstacleDistanceCm;
    doc["obstacleNear"] = st.sensors.obstacleNear;
    doc["binSensor"] = board::binTrig >= 0;
    doc["binCm"] = st.sensors.binDistanceCm;
    doc["binFull"] = st.sensors.binFull;
    doc["ultrasonicCycle"] = boat::name(st.ultrasonicCycle);
    sensor_t* sensor = esp_camera_sensor_get();
    doc["framesize"] = sensor ? static_cast<int>(sensor->status.framesize) : -1;
    JsonObject card = doc.createNestedObject("sd");
    card["mounted"] = sd.mounted; card["recording"] = sd.recording;
    card["folder"] = sd.folder; card["saved"] = sd.saved;
    card["dropped"] = sd.dropped; card["failures"] = sd.failures;
    card["usedMB"] = static_cast<uint32_t>(sd.usedBytes >> 20);
    card["totalMB"] = static_cast<uint32_t>(sd.totalBytes >> 20);
    card["error"] = sd.error;
    String json;
    serializeJson(doc, json);
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", json);
}
#endif
