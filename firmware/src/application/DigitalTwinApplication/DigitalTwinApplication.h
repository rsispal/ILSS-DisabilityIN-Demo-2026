#pragma once

#include "../../utils/Logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

class LowLevel;
class State;
class RGBLED;
class Buzzer;
class Haptics;
class SideButtons;
class IndicationController;
class BleTwin;

class DigitalTwinApplication {
    const char* TAG = "DigitalTwinApp";

public:
    /**
     * @param rgbLed Optional already-begun strip from boot (avoids double RMT on GPIO1).
     *               Ownership stays with the caller when non-null.
     */
    DigitalTwinApplication(Logger* logger, LowLevel* lowLevel, State* state,
                           RGBLED* rgbLed = nullptr);
    ~DigitalTwinApplication();

    void begin();  // blocks in FreeRTOS event loop

private:
    Logger* logger_;
    LowLevel* lowLevel_;
    State* state_;

    RGBLED* rgbLed_ = nullptr;
    bool owns_rgb_led_ = true;
    Buzzer* buzzer_ = nullptr;
    Haptics* haptics_ = nullptr;
    SideButtons* sideButtons_ = nullptr;
    IndicationController* indications_ = nullptr;
    BleTwin* bleTwin_ = nullptr;

    QueueHandle_t event_queue_ = nullptr;
    TaskHandle_t led_task_ = nullptr;
    TaskHandle_t buzzer_task_ = nullptr;
    /** Ignore repeat BOTH_HOLD within this window (ms) — app-level PA debounce. */
    TickType_t last_both_hold_tick_ = 0;

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
