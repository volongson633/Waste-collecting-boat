#pragma once

#ifndef BOAT_RGB4_ENABLED
#define BOAT_RGB4_ENABLED 0
#endif
#ifndef BOAT_RGB4_COMMON_ANODE
#define BOAT_RGB4_COMMON_ANODE 0
#endif

// GOOUUU ESP32-S3-CAM pinout supplied by the user:
// https://forum.arduino.cc/t/chinese-esp32-s3-cam/1318290
// Local reference: doc/reference/goouuu-pinout.jpeg.
namespace board {
constexpr int cameraPwdn = -1;
constexpr int cameraReset = -1;
constexpr int cameraXclk = 15;
constexpr int cameraSiod = 4;
constexpr int cameraSioc = 5;
constexpr int cameraD0 = 11;  // Y2
constexpr int cameraD1 = 9;   // Y3
constexpr int cameraD2 = 8;   // Y4
constexpr int cameraD3 = 10;  // Y5
constexpr int cameraD4 = 12;  // Y6
constexpr int cameraD5 = 18;  // Y7
constexpr int cameraD6 = 17;  // Y8
constexpr int cameraD7 = 16;  // Y9
constexpr int cameraVsync = 6;
constexpr int cameraHref = 7;
constexpr int cameraPclk = 13;

// Drive bridge (L298N #1): ENA/ENB jumpers fitted, speed is PWM on the IN pins.
// Fit 10 kOhm pull-downs on IN1..IN4: with EN tied high, a floating IN moves a motor.
constexpr int leftEnable = -1;  // ENA jumper.
constexpr int leftIn1 = 2;      // PWM. Also drives the board's indicator LED.
constexpr int leftIn2 = 14;     // PWM.
constexpr int rightEnable = -1; // ENB jumper.
constexpr int rightIn1 = 21;    // PWM (IN3).
constexpr int rightIn2 = 47;    // PWM (IN4).

// On-board microSD slot, SD_MMC 1-bit mode. These traces are fixed on the board.
constexpr bool sdEnabled = true;
constexpr int sdClk = 39;
constexpr int sdCmd = 38;
constexpr int sdData0 = 40;
constexpr int conveyorIn1 = 41;
constexpr int conveyorIn2 = 42;
constexpr int conveyorEnable = -1; // ENA jumper, full speed. GPIO40 now belongs to the SD slot.
constexpr int redLed = -1; // No free GPIO for an external red LED.
constexpr int rgbLed = 48; // Built-in WS2812; do not connect a discrete LED here.
constexpr bool rgb4Enabled = BOAT_RGB4_ENABLED != 0;
constexpr bool rgb4CommonAnode = BOAT_RGB4_COMMON_ANODE != 0;
constexpr int rgb4Red = rgb4Enabled ? 1 : -1;
constexpr int rgb4Green = rgb4Enabled ? 40 : -1;
constexpr int rgb4Blue = rgb4Enabled ? 20 : -1; // Native USB D+; leave that USB port unplugged.
static_assert(!(rgb4Enabled && sdEnabled), "The four-pin RGB LED needs GPIO40, which the SD slot uses");
constexpr bool rgbAvailable = rgbLed >= 0 || rgb4Enabled;
constexpr bool invertLeft = false;
constexpr bool invertRight = false;
constexpr bool invertConveyor = false;

constexpr int batteryAdc = -1; // GPIO1 is used by the ultrasonic echo.
constexpr int intakeSensor = -1; // Optional 3.3V digital input; validate free pin first.
constexpr bool intakeActiveLow = true;
constexpr int currentAdc = -1;   // Optional ADC1 input; requires wiring reassignment.

// Assign only after checking which pins are exposed and free on the board.
// Bin-full HC-SR04 looking down into the bin. TRIG on GPIO20 (native USB D+, OTG port
// unused); ECHO on GPIO46 through the 1k/2k divider, whose 2k to GND also holds this
// strapping pin low at boot as required.
constexpr int binTrig = rgb4Enabled ? -1 : 20;
constexpr int binEcho = rgb4Enabled ? -1 : 46;
// TRIG uses GPIO19 (native USB D-): keep the OTG USB port unplugged; flash over the TTL port.
constexpr int obstacleTrig = rgb4Enabled ? -1 : 19; // Yellow TRIG wire.
constexpr int obstacleEcho = rgb4Enabled ? -1 : 1; // Green ECHO wire through a 5 V to 3.3 V divider.
static_assert((binTrig < 0) == (binEcho < 0), "Set both bin ultrasonic pins");
static_assert((obstacleTrig < 0) == (obstacleEcho < 0), "Set both obstacle ultrasonic pins");

constexpr bool isUsable(int pin) {
    return (pin >= 0 && pin <= 21) || (pin >= 38 && pin <= 48);
}
constexpr bool reserved(int pin) {
    // GPIO19/20 are the native USB pins and GPIO46 a strapping pin; each is allowed
    // only for its documented use. GPIO0 (BOOT) and GPIO45 (flash voltage) never.
    return pin == 0 || pin == 3 || (pin == 19 && obstacleTrig != 19) ||
           (pin == 20 && !rgb4Enabled && binTrig != 20) || pin == 43 ||
           pin == 44 || pin == 45 || (pin == 46 && binEcho != 46) || pin == 48;
}
constexpr int assigned[] = {
    cameraPwdn, cameraReset, cameraXclk, cameraSiod, cameraSioc,
    cameraD0, cameraD1, cameraD2, cameraD3, cameraD4, cameraD5, cameraD6,
    cameraD7, cameraVsync, cameraHref, cameraPclk, leftEnable, leftIn1,
    leftIn2, rightEnable, rightIn1, rightIn2, conveyorIn1, conveyorIn2,
    conveyorEnable, batteryAdc, intakeSensor, currentAdc, redLed, rgb4Red, rgb4Green, rgb4Blue,
    binTrig, binEcho, obstacleTrig, obstacleEcho,
    sdEnabled ? sdClk : -1, sdEnabled ? sdCmd : -1, sdEnabled ? sdData0 : -1
};
template<unsigned N>
constexpr bool distinctFromLater(const int (&pins)[N], unsigned i, unsigned j) {
    return j >= N || ((pins[j] < 0 || pins[i] != pins[j]) && distinctFromLater(pins, i, j + 1));
}
template<unsigned N>
constexpr bool unique(const int (&pins)[N], unsigned i = 0) {
    return i >= N || ((pins[i] < 0 || distinctFromLater(pins, i, i + 1)) && unique(pins, i + 1));
}
template<unsigned N>
constexpr bool allowed(const int (&pins)[N], unsigned i = 0) {
    return i >= N ? true :
        (pins[i] == -1 || (isUsable(pins[i]) && !reserved(pins[i]))) && allowed(pins, i + 1);
}
static_assert(unique(assigned), "GPIO collision: camera/motors/sensors share a pin");
static_assert(allowed(assigned), "Invalid/reserved GPIO: check USB, UART, straps and PSRAM");
static_assert(rgbLed == -1 || rgbLed == 48, "Built-in RGB LED must use GPIO48 (or -1 to disable)");
static_assert(batteryAdc == -1 || (batteryAdc >= 1 && batteryAdc <= 10), "Battery must use ADC1");
static_assert(currentAdc == -1 || (currentAdc >= 1 && currentAdc <= 10), "Current must use ADC1");
} // namespace board
