#include "runtime.h"
#include "app_config.h"
#include "board_config.h"
#include <esp_system.h>
#include <esp_task_wdt.h>

namespace {
boat::Settings settings() {
    boat::Settings s;
    s.requireBattery = config::batteryRequired;
    s.ultrasonicCollectMs = config::conveyorRevolutionMs;
    return s;
}
}
Runtime::Runtime(CameraService& camera) : camera_(camera), controller_(settings()) {}
bool Runtime::begin() {
    mutex_ = xSemaphoreCreateMutex();
    const bool motorReady = motors_.begin();
    if (!mutex_) { motors_.stop(millis()); return false; }
    power_.begin();
    controller_ = boat::Controller(settings(), esp_random());
    status_.hardwareReady = motorReady;
    if (!motorReady) controller_.forceFault(boat::Reason::Hardware);
    updateStatus();
    // The watchdog bounds a stalled control task. Pull-downs on enables/inputs
    // are still required to hold the bridge off during hardware reset.
    esp_task_wdt_init(3, true);
    if (xTaskCreatePinnedToCore(taskEntry, "control", 4096, this, 3, nullptr, 1) != pdPASS) {
        motors_.stop(millis());
        status_.hardwareReady = false;
        return false;
    }
    return true;
}
boat::Inputs Runtime::inputs() const {
    boat::Inputs in;
    in.hardwareReady = status_.hardwareReady;
    in.batteryValid = status_.sensors.batteryValid;
    in.batteryLow = status_.sensors.batteryLow;
    in.batteryRecovered = status_.sensors.batteryRecovered;
    in.intake = status_.sensors.intake;
    in.jam = status_.sensors.jam;
    in.binFull = status_.sensors.binFull;
    in.obstacleNear = status_.sensors.obstacleNear;
    in.cameraReady = status_.camera.ready;
    in.detectorReady = status_.camera.detectorReady;
    in.detection = status_.camera.detection;
    return in;
}
void Runtime::updateStatus() {
    if (status_.epoch != controller_.epoch()) lights_.off();
    status_.state = controller_.state();
    status_.reason = controller_.reason();
    status_.epoch = controller_.epoch();
    status_.sequence = controller_.sequence();
    status_.collectionCycles = controller_.collectionCycles();
    status_.ultrasonicCycle = controller_.ultrasonicCycle();
    status_.lights = lights_.state();
    ++status_.version;
    status_.readyToArm = status_.hardwareReady && !status_.sensors.jam &&
        !status_.sensors.binFull && !status_.sensors.obstacleNear &&
        (!config::batteryRequired || (status_.sensors.batteryValid && !status_.sensors.batteryLow && status_.sensors.batteryRecovered));
}
bool Runtime::command(const boat::Command& command) {
    if (!mutex_ || xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE) return false;
    const uint32_t now = millis();
    operatorAt_ = now;
    const bool result = controller_.command(command, now, inputs());
    if (command.action == boat::Action::Stop) lights_.off();
    if (result && (command.action == boat::Action::Drive || command.action == boat::Action::Stop)) {
        status_.desired = controller_.tick(now, inputs());
        status_.applied = motors_.apply(status_.desired, now);
    }
    updateStatus();
    xSemaphoreGive(mutex_);
    return result;
}
void Runtime::enableIdlePatrol() {
    if (!mutex_ || xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE) return;
    operatorAt_ = millis();
    idleEnabled_ = config::idlePatrolEnabled;
    xSemaphoreGive(mutex_);
}
void Runtime::operatorPresent() {
    if (!mutex_ || xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE) return;
    operatorAt_ = millis();
    if (status_.idlePatrol) {
        controller_.command(boat::Command(), operatorAt_, inputs());
        status_.idlePatrol = false;
        status_.desired = boat::Output();
        status_.applied = motors_.apply(status_.desired, operatorAt_);
        updateStatus();
    }
    xSemaphoreGive(mutex_);
}
void Runtime::tickIdlePatrol(uint32_t now) {
    if (!idleEnabled_) return;
    if (status_.idlePatrol && controller_.state() != boat::State::Manual)
        status_.idlePatrol = false;
    if (now - operatorAt_ < config::idlePatrolWaitMs) return;
    if (!status_.idlePatrol) {
        if (controller_.state() != boat::State::Stop) return;
        boat::Command arm;
        arm.action = boat::Action::Manual;
        arm.epoch = controller_.epoch();
        if (!controller_.command(arm, now, inputs())) return;
        status_.idlePatrol = true;
        patrolAt_ = now;
    }
    boat::Command drive;
    drive.action = boat::Action::Drive;
    drive.epoch = controller_.epoch();
    drive.sequence = controller_.sequence() + 1;
    const bool forward = ((now - patrolAt_) / config::idlePatrolStepMs) % 2 == 0;
    drive.left = drive.right = forward ? config::idlePatrolSpeed : -config::idlePatrolSpeed;
    if (!controller_.command(drive, now, inputs())) {
        controller_.command(boat::Command(), now, inputs());
        status_.idlePatrol = false;
        operatorAt_ = now;
    }
}
bool Runtime::setLights(uint32_t epoch, uint32_t revision, bool red, boat::RgbColor rgb) {
    if ((board::redLed < 0 && red) || (!board::rgbAvailable && rgb != boat::RgbColor::Off)) return false;
    if (!mutex_ || xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE) return false;
    const bool result = epoch == controller_.epoch() && lights_.set(revision, red, rgb);
    if (result) updateStatus();
    xSemaphoreGive(mutex_);
    return result;
}
RuntimeStatus Runtime::status() {
    RuntimeStatus copy;
    if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
        copy = status_;
        xSemaphoreGive(mutex_);
    }
    return copy;
}
void Runtime::taskEntry(void* arg) { static_cast<Runtime*>(arg)->task(); }
void Runtime::task() {
    esp_task_wdt_add(nullptr);
    TickType_t tick = xTaskGetTickCount();
    uint32_t lastSample = millis() - config::sensorPeriodMs;
    SensorStatus sensors;
    boat::Output applied;
    boat::State previousState = boat::State::Stop;
    for (;;) {
        uint32_t now = millis();
        if (now - lastSample >= config::sensorPeriodMs) {
            sensors = power_.sample(now, applied.conveyor != 0);
            lastSample = now;
        }
        const CameraStatus camera = camera_.status();
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(5)) == pdTRUE) {
            // A web command may have updated its timestamp while we sampled.
            now = millis();
            status_.sensors = sensors;
            status_.camera = camera;
            tickIdlePatrol(now);
            status_.desired = controller_.tick(now, inputs());
            applied = motors_.apply(status_.desired, now);
            status_.applied = applied;
            updateStatus();
            const boat::State currentState = status_.state;
            const boat::Reason reason = status_.reason;
            xSemaphoreGive(mutex_);
            if (currentState != previousState) {
                Serial.printf("[%lu] %s -> %s (%s)\n", now, boat::name(previousState), boat::name(currentState), boat::name(reason));
                previousState = currentState;
            }
        } else {
            // A congested UI may not hold the motors running.
            motors_.stop(now);
            applied = boat::Output();
        }
        esp_task_wdt_reset();
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(config::controlPeriodMs));
    }
}
