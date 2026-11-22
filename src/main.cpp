#include <Arduino.h>
#include "camera_service.h"
#include "runtime.h"
#include "web_control.h"
#include "app_config.h"
#include "led_control.h"

CameraService camera;
Runtime runtime(camera);
WebControl web(runtime);
LedControl leds;

// Print only direction/on-off changes, not every PWM ramp step or heartbeat.
// Keep Serial outside the control task so terminal output cannot delay motors.
void logMotorState(const RuntimeStatus& status) {
    static bool previousPatrol = false;
    if (status.idlePatrol != previousPatrol) {
        Serial.println(status.idlePatrol ? "[AUTO] Không có người mở trang: bắt đầu tự chạy"
            : "[AUTO] Đã dừng tự chạy");
        previousPatrol = status.idlePatrol;
    }
    logMotorOutput(status.applied);
}

void setup() {
    Serial.begin(115200);
    Serial.printf("\nGOOUUU boat | outputs=%s | PSRAM=%u bytes\n",
        config::outputsEnabled ? "LIVE" : "SIMULATED", ESP.getPsramSize());
    // Establish motor stop/control task before slow camera or Wi-Fi startup.
    const bool controlReady = runtime.begin();
    leds.begin();
    if (!controlReady) Serial.println("Control initialization failed; motors unavailable");
    // Bring up the access point before camera initialization, which can stall
    // on a disconnected sensor. The control task already holds motors stopped.
    if (!web.begin()) {
        boat::Command stop;
        runtime.command(stop);
        Serial.println("Wi-Fi initialization failed");
    }
    if (!camera.begin()) Serial.println("Camera unavailable; inspect ribbon, power and pinout");
    if (controlReady) runtime.enableIdlePatrol();
}
void loop() {
    web.poll();
    const auto status = runtime.status();
    leds.apply(status.lights);
    logMotorState(status);
    delay(2);
}
