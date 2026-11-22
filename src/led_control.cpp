#include "led_control.h"
#include "board_config.h"
#include <Arduino.h>
#include <esp32-hal-rgb-led.h>

namespace {
uint8_t rgb4Level(bool on) {
    return on != board::rgb4CommonAnode ? HIGH : LOW;
}
}

void LedControl::begin() {
    if (board::redLed >= 0) {
        digitalWrite(board::redLed, LOW);
        pinMode(board::redLed, OUTPUT);
    }
    if (board::rgb4Enabled) {
        for (int pin : {board::rgb4Red, board::rgb4Green, board::rgb4Blue}) {
            digitalWrite(pin, rgb4Level(false));
            pinMode(pin, OUTPUT);
        }
    }
    apply(boat::LightState());
}
void LedControl::apply(const boat::LightState& state) {
    if (board::redLed >= 0 && (!initialized_ || state.red != previousRed_))
        digitalWrite(board::redLed, state.red ? HIGH : LOW);
    if (board::rgbAvailable && (!initialized_ || state.rgb != previousRgb_)) {
        constexpr uint8_t brightness = 24; // Gentle brightness, 0..255 per channel.
        uint8_t r = 0, g = 0, b = 0;
        switch (state.rgb) {
        case boat::RgbColor::Red: r = brightness; break;
        case boat::RgbColor::Yellow: r = g = brightness; break;
        case boat::RgbColor::Blue: b = brightness; break;
        case boat::RgbColor::Green: g = brightness; break;
        case boat::RgbColor::White: r = g = b = brightness; break;
        default: break;
        }
        if (board::rgb4Enabled) {
            digitalWrite(board::rgb4Red, rgb4Level(r != 0));
            digitalWrite(board::rgb4Green, rgb4Level(g != 0));
            digitalWrite(board::rgb4Blue, rgb4Level(b != 0));
        }
        if (board::rgbLed >= 0) {
            // RMT can block; loop() is outside the motor task and its mutex.
            neopixelWrite(board::rgbLed, r, g, b);
        }
    }
    previousRed_ = state.red;
    previousRgb_ = state.rgb;
    initialized_ = true;
}
