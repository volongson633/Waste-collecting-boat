#if defined(BOAT_COLLECTOR) && BOAT_COLLECTOR
// Entry point of the data-collection firmware (pio run -e data-collect).
#include <Arduino.h>
#include "camera_service.h"
#include "collector_app.h"
#include "led_control.h"
#include "runtime.h"
#include "sd_recorder.h"

CameraService camera;
Runtime runtime(camera);
SdRecorder sd;
LedControl leds;
CollectorApp app(runtime, sd);

void setup() {
    Serial.begin(115200);
    Serial.printf("\nGOOUUU boat | DATA COLLECT | outputs=%s | PSRAM=%u bytes\n",
        config::outputsEnabled ? "LIVE" : "SIMULATED", ESP.getPsramSize());
    // Motors are held stopped by the control task before anything slow starts.
    // The idle patrol of the main firmware is deliberately NOT enabled here.
    if (!runtime.begin()) Serial.println("Control initialization failed; motors unavailable");
    leds.begin();
    if (!app.beginWifi()) Serial.println("Wi-Fi initialization failed");
    if (!sd.begin()) Serial.println("SD: recording unavailable");
    camera.attachRecorder(&sd);
    if (!camera.begin()) Serial.println("Camera unavailable; inspect ribbon, power and pinout");
    app.beginServer();
}

void loop() {
    app.poll();
    const auto status = runtime.status();
    leds.apply(status.lights);
    logMotorOutput(status.applied);
    delay(2);
}
#endif
