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

constexpr int leftEnable = 21;
constexpr int leftIn1 = 2;   // Also drives the board's indicator LED.
constexpr int leftIn2 = 14;
constexpr int rightEnable = 38; // SD CMD: do not initialize SD.
constexpr int rightIn1 = 39;    // SD CLK: do not insert/use an SD card.
constexpr int rightIn2 = 47;
constexpr int conveyorIn1 = 41;
constexpr int conveyorIn2 = 42;
constexpr int conveyorEnable = -1; // Original wiring: ENA jumper, full speed.
// Optional PWM: set conveyorEnable=40 AND redLed=-1; remove red LED and ENA jumper.
constexpr int redLed = -1; // GPIO40 is used by the ultrasonic trigger.
constexpr int rgbLed = 48; // Built-in WS2812; do not connect a discrete LED here.
constexpr bool rgb4Enabled = BOAT_RGB4_ENABLED != 0;
constexpr bool rgb4CommonAnode = BOAT_RGB4_COMMON_ANODE != 0;
constexpr int rgb4Red = rgb4Enabled ? 1 : -1;
constexpr int rgb4Green = rgb4Enabled ? 40 : -1;
constexpr int rgb4Blue = rgb4Enabled ? 20 : -1; // Native USB D+; leave that USB port unplugged.
constexpr bool rgbAvailable = rgbLed >= 0 || rgb4Enabled;
constexpr bool invertLeft = false;
constexpr bool invertRight = false;
constexpr bool invertConveyor = false;

constexpr int batteryAdc = -1; // GPIO1 is used by the ultrasonic echo.
constexpr int intakeSensor = -1; // Optional 3.3V digital input; validate free pin first.
constexpr bool intakeActiveLow = true;
constexpr int currentAdc = -1;   // Optional ADC1 input; requires wiring reassignment.

// Assign only after checking which pins are exposed and free on the board.
constexpr int binTrig = -1;
constexpr int binEcho = -1;
constexpr int obstacleTrig = rgb4Enabled ? -1 : 40; // Yellow TRIG wire.
constexpr int obstacleEcho = rgb4Enabled ? -1 : 1; // Green ECHO wire through a 5 V to 3.3 V divider.
static_assert((binTrig < 0) == (binEcho < 0), "Set both bin ultrasonic pins");
static_assert((obstacleTrig < 0) == (obstacleEcho < 0), "Set both obstacle ultrasonic pins");

constexpr bool isUsable(int pin) {
    return (pin >= 0 && pin <= 21) || (pin >= 38 && pin <= 48);
}
constexpr bool reserved(int pin) {
    return pin == 0 || pin == 3 || pin == 19 || (pin == 20 && !rgb4Enabled) || pin == 43 ||
           pin == 44 || pin == 45 || pin == 46 || pin == 48;
}
constexpr int assigned[] = {
    cameraPwdn, cameraReset, cameraXclk, cameraSiod, cameraSioc,
    cameraD0, cameraD1, cameraD2, cameraD3, cameraD4, cameraD5, cameraD6,
    cameraD7, cameraVsync, cameraHref, cameraPclk, leftEnable, leftIn1,
    leftIn2, rightEnable, rightIn1, rightIn2, conveyorIn1, conveyorIn2,
    conveyorEnable, batteryAdc, intakeSensor, currentAdc, redLed, rgb4Red, rgb4Green, rgb4Blue,
    binTrig, binEcho, obstacleTrig, obstacleEcho
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
