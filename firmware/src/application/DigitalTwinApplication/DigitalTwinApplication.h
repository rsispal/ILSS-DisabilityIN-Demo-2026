#pragma once

#include "../../utils/Logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

class LowLevel;
class State;
class RGBLED;
class Buzzer;
class SideButtons;
class IndicationController;
class BleTwin;

class DigitalTwinApplication {
    const char* TAG = "DigitalTwinApp";

public:
    DigitalTwinApplication(Logger* logger, LowLevel* lowLevel, State* state);
    ~DigitalTwinApplication();

    void begin();  // blocks in FreeRTOS event loop

private:
    Logger* logger_;
    LowLevel* lowLevel_;
    State* state_;

    RGBLED* rgbLed_ = nullptr;
    Buzzer* buzzer_ = nullptr;
    SideButtons* sideButtons_ = nullptr;
    IndicationController* indications_ = nullptr;
    BleTwin* bleTwin_ = nullptr;

    QueueHandle_t event_queue_ = nullptr;
    TaskHandle_t led_task_ = nullptr;
    TaskHandle_t buzzer_task_ = nullptr;

    enum class AppEventType : uint8_t {
        ButtonBothHold,
        ButtonSidePress,
        BleDisconnected,
        BleConnecting,
        BlePaired,
        Tick,
    };

    struct AppEvent {
        AppEventType type;
        uint8_t side = 0;  // BUTTON_SIDE_LEFT / RIGHT for ButtonSidePress
    };

    void initTasks();
    void eventLoop();
    void onBothHold();
    void onSidePress(uint8_t side);
    void onBleDisconnected();
    void onBleConnecting();
    void onBlePaired();

    static void ledTaskTrampoline(void* arg);
    static void buzzerTaskTrampoline(void* arg);
    void ledTask();
    void buzzerTask();
};
