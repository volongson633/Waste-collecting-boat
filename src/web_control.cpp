#include "web_control.h"
#include "app_config.h"
#include "board_config.h"
#include "vision_service.h"
#include "web_page.h"
#include "camera_page.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_system.h>

bool WebControl::begin() {
    Serial.println("Wi-Fi: starting access point...");
    if (!WiFi.mode(strlen(WIFI_SSID) ? WIFI_AP_STA : WIFI_AP)) {
        Serial.println("Wi-Fi: WIFI_AP mode failed");
        return false;
    }
    WiFi.setSleep(false);
    if (!WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0))) {
        Serial.println("Wi-Fi: AP IP configuration failed");
        return false;
    }
    if (!WiFi.softAP(BOAT_AP_SSID, BOAT_AP_PASSWORD, 1, false, 2)) {
        Serial.println("Wi-Fi: softAP start failed");
        return false;
    }
    Serial.printf("Wi-Fi: AP active | SSID=%s | IP=%s\n", BOAT_AP_SSID, WiFi.softAPIP().toString().c_str());
    if (strlen(WIFI_SSID)) WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    char key[33];
    snprintf(key, sizeof(key), "%08lx%08lx%08lx%08lx", esp_random(), esp_random(), esp_random(), esp_random());
    controlKey_ = key;
    const char* headers[] = {"X-Boat-Key", "Content-Type", "X-Motor-Control"};
    server_.collectHeaders(headers, 3);
    server_.on("/", HTTP_GET, [this]() {
        runtime_.operatorPresent();
        server_.sendHeader("Cache-Control", "no-store");
        server_.sendHeader("X-Frame-Options", "DENY");
        server_.send_P(200, "text/html; charset=utf-8", CAMERA_PAGE);
    });
    server_.on("/dashboard", HTTP_GET, [this]() {
        runtime_.operatorPresent();
        server_.sendHeader("Cache-Control", "no-store");
        server_.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
    });
    server_.on("/motor", HTTP_POST, [this]() { handlePhoneMotor(); });
    cameraRoutes();
    sdRoutes();
    server_.on("/api/status", HTTP_GET, [this]() { runtime_.operatorPresent(); sendStatus(); });
    server_.on("/api/command", HTTP_POST, [this]() { handleCommand(); });
    server_.on("/api/led", HTTP_POST, [this]() { handleLights(); });
    server_.onNotFound([this]() { server_.send(404, "application/json", "{\"error\":\"not_found\"}"); });
    server_.begin();
    Serial.printf("Wi-Fi: %s | http://192.168.4.1 | stream: :81/stream | snapshot: :82/capture\n", BOAT_AP_SSID);
    return true;
}
void WebControl::sendStatus() {
    const auto s = runtime_.status();
    StaticJsonDocument<2048> doc;
    doc["state"] = boat::name(s.state); doc["reason"] = boat::name(s.reason);
    doc["epoch"] = s.epoch; doc["sequence"] = s.sequence; doc["key"] = controlKey_;
    doc["version"] = s.version;
    doc["outputsEnabled"] = config::outputsEnabled;
    doc["batteryRequired"] = config::batteryRequired;
    doc["hardwareReady"] = s.hardwareReady;
    doc["readyToArm"] = s.readyToArm;
    doc["idlePatrol"] = s.idlePatrol;
    doc["detector"] = vision::backendName();
    doc["cameraReady"] = s.camera.ready;
    doc["detectorReady"] = s.camera.detectorReady;
    doc["processingMs"] = s.camera.processingMs;
    doc["captureFailures"] = s.camera.captureFailures;
    doc["uptimeMs"] = millis();
    doc["heap"] = ESP.getFreeHeap(); doc["psram"] = ESP.getFreePsram();
    doc["collectionCycles"] = s.collectionCycles;
    doc["ultrasonicCycle"] = boat::name(s.ultrasonicCycle);
    doc["batteryV"] = s.sensors.batteryV;
    doc["batteryValid"] = s.sensors.batteryValid;
    doc["batteryWarning"] = s.sensors.batteryWarning;
    doc["intakeAvailable"] = board::intakeSensor >= 0;
    doc["intake"] = s.sensors.intake;
    doc["binFull"] = s.sensors.binFull;
    doc["obstacleNear"] = s.sensors.obstacleNear;
    if (s.sensors.binDistanceCm > 0) doc["binDistanceCm"] = s.sensors.binDistanceCm;
    if (s.sensors.obstacleDistanceCm > 0) doc["obstacleDistanceCm"] = s.sensors.obstacleDistanceCm;
    doc["currentAvailable"] = board::currentAdc >= 0;
    doc["currentA"] = s.sensors.currentA;
    doc["conveyorPwm"] = board::conveyorEnable >= 0;
    JsonObject lights = doc.createNestedObject("lights");
    lights["redAvailable"] = board::redLed >= 0;
    lights["rgbAvailable"] = board::rgbAvailable;
    lights["red"] = s.lights.red;
    lights["rgb"] = boat::name(s.lights.rgb);
    lights["revision"] = s.lights.revision;
    JsonObject motor = doc.createNestedObject("motor");
    motor["left"] = s.applied.left; motor["right"] = s.applied.right; motor["conveyor"] = s.applied.conveyor;
    JsonObject detection = doc.createNestedObject("detection");
    detection["found"] = s.camera.detection.found;
    detection["healthy"] = s.camera.detection.healthy;
    detection["frame"] = s.camera.detection.frame;
    detection["ageMs"] = millis() - s.camera.detection.capturedAt;
    detection["x"] = s.camera.detection.x; detection["y"] = s.camera.detection.y;
    detection["confidence"] = s.camera.detection.confidence;
    String json;
    serializeJson(doc, json);
    server_.sendHeader("Cache-Control", "no-store");
    server_.send(200, "application/json", json);
}
bool WebControl::acceptJsonRequest() {
    // Non-simple JSON + same-origin readable key prevents cross-site form commands.
    if (server_.header("X-Boat-Key") != controlKey_ ||
        !server_.header("Content-Type").startsWith("application/json")) {
        server_.send(403, "application/json", "{\"error\":\"forbidden\"}"); return false;
    }
    if (server_.arg("plain").length() > 512) {
        server_.send(413, "application/json", "{\"error\":\"body_too_large\"}"); return false;
    }
    return true;
}
void WebControl::handleLights() {
    if (!acceptJsonRequest()) return;
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, server_.arg("plain")) || !doc["epoch"].is<uint32_t>() ||
        !doc["revision"].is<uint32_t>() || !doc["red"].is<bool>() || !doc["rgb"].is<const char*>()) {
        server_.send(400, "application/json", "{\"error\":\"invalid_led_command\"}"); return;
    }
    const String rgb = doc["rgb"].as<const char*>();
    boat::RgbColor color = boat::RgbColor::Off;
    if (rgb == "red") color = boat::RgbColor::Red;
    else if (rgb == "yellow") color = boat::RgbColor::Yellow;
    else if (rgb == "blue") color = boat::RgbColor::Blue;
    else if (rgb == "green") color = boat::RgbColor::Green;
    else if (rgb == "white") color = boat::RgbColor::White;
    else if (rgb != "off") {
        server_.send(400, "application/json", "{\"error\":\"invalid_led_color\"}"); return;
    }
    if (!runtime_.setLights(doc["epoch"], doc["revision"], doc["red"], color)) {
        server_.send(409, "application/json", "{\"error\":\"led_state_changed_or_pin_disabled\"}"); return;
    }
    sendStatus();
}
void WebControl::handleCommand() {
    if (!acceptJsonRequest()) return;
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, server_.arg("plain")) || !doc["action"].is<const char*>()) {
        server_.send(400, "application/json", "{\"error\":\"invalid_json\"}"); return;
    }
    const String action = doc["action"].as<const char*>();
    boat::Command c;
    bool valid = true;
    if (action == "stop") c.action = boat::Action::Stop;
    else if (action == "manual") c.action = boat::Action::Manual;
    else if (action == "auto") c.action = boat::Action::Auto;
    else if (action == "drive") c.action = boat::Action::Drive;
    else if (action == "heartbeat") c.action = boat::Action::Heartbeat;
    else if (action == "clear") c.action = boat::Action::ClearFault;
    else valid = false;
    if (c.action != boat::Action::Stop && !doc["epoch"].is<uint32_t>()) valid = false;
    c.epoch = doc["epoch"] | uint32_t(0);
    if (c.action == boat::Action::Drive || c.action == boat::Action::Heartbeat) {
        if (!doc["sequence"].is<uint32_t>()) valid = false;
        c.sequence = doc["sequence"] | uint32_t(0);
    }
    if (c.action == boat::Action::Drive) {
        if (!doc["left"].is<int>() || !doc["right"].is<int>() || !doc["conveyor"].is<int>()) valid = false;
        c.left = doc["left"] | 0; c.right = doc["right"] | 0; c.conveyor = doc["conveyor"] | 0;
    }
    if (!valid) { server_.send(400, "application/json", "{\"error\":\"invalid_command\"}"); return; }
    if (!runtime_.command(c)) {
        server_.send(409, "application/json", "{\"error\":\"rejected_check_state_epoch_battery_camera\"}"); return;
    }
    sendStatus();
}
