#include "motor_control.h"
#include "board_config.h"
#include <Arduino.h>
#include <driver/gpio.h>

namespace {
bool setupMotor(int enable, int in1, int in2, unsigned channel) {
    digitalWrite(in1, LOW); digitalWrite(in2, LOW);
    pinMode(in1, OUTPUT); pinMode(in2, OUTPUT);
    if (enable < 0) return true;
    digitalWrite(enable, LOW); pinMode(enable, OUTPUT);
    if (ledcSetup(channel, config::pwmHz, config::pwmBits) == 0) return false;
    ledcWrite(channel, 0);
    ledcAttachPin(enable, channel);
    return true;
}
void writeMotor(int value, int enable, int in1, int in2, unsigned channel, bool inverted) {
    if (enable >= 0) ledcWrite(channel, 0);
    if (inverted) value = -value;
    digitalWrite(in1, value > 0 ? HIGH : LOW);
    digitalWrite(in2, value < 0 ? HIGH : LOW);
    if (enable >= 0) ledcWrite(channel, abs(value));
}
bool setupConveyor() {
    if (board::conveyorEnable >= 0) {
        return setupMotor(board::conveyorEnable, board::conveyorIn1,
            board::conveyorIn2, config::conveyorPwmChannel);
    }
    bool ready = true;
    const int pins[] = {board::conveyorIn1, board::conveyorIn2};
    for (int pin : pins) {
        const auto gpio = static_cast<gpio_num_t>(pin);
        const auto resetResult = gpio_reset_pin(gpio);
        const auto stopResult = gpio_set_level(gpio, 0);
        const auto modeResult = gpio_set_direction(gpio, GPIO_MODE_INPUT_OUTPUT);
        const auto holdResult = gpio_hold_dis(gpio);
        ready = (holdResult == ESP_OK || holdResult == ESP_ERR_NOT_SUPPORTED) && resetResult == ESP_OK &&
            modeResult == ESP_OK && stopResult == ESP_OK && ready;
    }
    return ready;
}
void writeConveyor(int value) {
    if (board::conveyorEnable >= 0) {
        writeMotor(value, board::conveyorEnable, board::conveyorIn1,
            board::conveyorIn2, config::conveyorPwmChannel, board::invertConveyor);
        return;
    }
    if (board::invertConveyor) value = -value;
    const auto in1 = static_cast<gpio_num_t>(board::conveyorIn1);
    const auto in2 = static_cast<gpio_num_t>(board::conveyorIn2);
    // Lower the inactive input first, including when changing direction.
    if (value > 0) {
        gpio_set_level(in2, 0);
        gpio_set_level(in1, 1);
    } else {
        gpio_set_level(in1, 0);
        gpio_set_level(in2, value < 0 ? 1 : 0);
    }
}
}
bool MotorControl::begin() {
    if (!config::outputsEnabled) { initialized_ = true; return true; }
    // Never short-circuit: configure every bridge to a stopped state even if one fails.
    bool leftOk = setupMotor(board::leftEnable, board::leftIn1, board::leftIn2, config::leftPwmChannel);
    bool rightOk = setupMotor(board::rightEnable, board::rightIn1, board::rightIn2, config::rightPwmChannel);
    bool conveyorOk = setupConveyor();
    initialized_ = leftOk && rightOk && conveyorOk;
    return initialized_;
}
boat::Output MotorControl::apply(const boat::Output& desired, uint32_t now) {
    boat::Output out;
    if (!initialized_) return out;
    out.left = left_.update(desired.left, now);
    out.right = right_.update(desired.right, now);
    out.conveyor = conveyor_.update(desired.conveyor, now);
    if (board::conveyorEnable < 0) {
        // With an EN jumper, preserve a stopped interval before reversing.
        if (out.conveyor > 0) out.conveyor = 255;
        else if (out.conveyor < 0) out.conveyor = -255;
    }
    if (config::outputsEnabled) {
        writeMotor(out.left, board::leftEnable, board::leftIn1, board::leftIn2, config::leftPwmChannel, board::invertLeft);
        writeMotor(out.right, board::rightEnable, board::rightIn1, board::rightIn2, config::rightPwmChannel, board::invertRight);
        writeConveyor(out.conveyor);
    }
    return out;
}
void MotorControl::stop(uint32_t now) { apply(boat::Output(), now); }

// Called from the web/main loop, outside the time-critical motor task.
void logMotorOutput(const boat::Output& motor) {
    static int previousDirection = 0;
    static int previousConveyor = 0;
    int direction = 0;
    if (motor.left > 0 && motor.right > 0) direction = 1;
    else if (motor.left < 0 && motor.right < 0) direction = 2;
    else if (motor.left <= 0 && motor.right >= 0 && (motor.left || motor.right)) direction = 3;
    else if (motor.left >= 0 && motor.right <= 0 && (motor.left || motor.right)) direction = 4;
    const char* prefix = "[MOTOR]";
    if (direction != previousDirection) {
        const char* messages[] = {
            "Đã dừng lại", "Đã phát lệnh tiến tới", "Đã phát lệnh lùi",
            "Đã phát lệnh sang trái", "Đã phát lệnh sang phải"
        };
        Serial.printf("%s %s\n", prefix, messages[direction]);
        previousDirection = direction;
    }
    const int conveyor = (motor.conveyor > 0) - (motor.conveyor < 0);
    if (conveyor != previousConveyor) {
        Serial.printf("[CONVEYOR] applied=%d | GPIO%d=%d GPIO%d=%d | EN=%d | live=%d\n",
            motor.conveyor,
            board::conveyorIn1, config::outputsEnabled ? digitalRead(board::conveyorIn1) : -1,
            board::conveyorIn2, config::outputsEnabled ? digitalRead(board::conveyorIn2) : -1,
            board::conveyorEnable, config::outputsEnabled);
        Serial.printf("%s %s\n", prefix,
            conveyor ? "Đã bật băng tải" : "Đã tắt băng tải");
        previousConveyor = conveyor;
    }
}
