#pragma once

#include <functional>
#include "../../utils/Logger.h"
#include "../../state/State.h"
#include "iot_button.h"
#include "esp_timer.h"
#include "driver/gpio.h"

enum class ButtonEvent {
    LEFT_PRESS_DOWN,
    LEFT_PRESS_UP,
    RIGHT_PRESS_DOWN,
    RIGHT_PRESS_UP,
    LEFT_PRESS,
    RIGHT_PRESS,
    LEFT_HOLD,
    RIGHT_HOLD,
    BOTH_HOLD_DOWN,
    BOTH_HOLD,
    BOTH_HOLD_UP
};

typedef std::function<void(ButtonEvent)> button_event_callback_t;

/**
 * SideButtons — espressif/button wrapper.
 *
 * Solo UI cue (LEFT/RIGHT_PRESS) fires on PRESS_DOWN. The right GPIO on this
 * PCB often never completes a clean SHORT click (stuck-active → LONG_PRESS),
 * so waiting for PRESS_UP / SINGLE_CLICK misses real taps.
 */
class SideButtons {
public:
    SideButtons(State* state, gpio_num_t left_pin = GPIO_NUM_8, gpio_num_t right_pin = GPIO_NUM_7);
    ~SideButtons();

    bool begin();
    void setEventCallback(button_event_callback_t callback);
    bool isLeftPressed() const;
    bool isRightPressed() const;
    bool isReady() const;

private:
    static constexpr const char* TAG = "SideButtons";
    static constexpr uint32_t HOLD_THRESHOLD_MS_DEFAULT = 5000;
    static constexpr uint32_t PRESS_THRESHOLD_MS = 500;
    /** Min time between solo UI cues per side (EMI flood guard). */
    static constexpr uint32_t kPressCueCooldownMs = 250;

    uint32_t getHoldThresholdMs() const;
    static uint32_t nowMs();
    void emitSoloPress(bool is_left);

    State* state;
    Logger logger;
    bool m_initialized;

    gpio_num_t left_pin_;
    gpio_num_t right_pin_;

    button_handle_t left_button_handle;
    button_handle_t right_button_handle;

    volatile bool left_button_pressed;
    volatile bool right_button_pressed;
    volatile bool both_hold_detected;
    esp_timer_handle_t both_hold_timer;

    uint32_t left_cue_ms_;
    uint32_t right_cue_ms_;

    button_event_callback_t event_callback;
    static SideButtons* instance;

    static void left_button_cb(void* button_handle, void* usr_data);
    static void right_button_cb(void* button_handle, void* usr_data);

    void handle_button_event(gpio_num_t pin, button_event_t event);
    void trigger_event(ButtonEvent event);
    void check_both_hold();

    static void both_hold_timer_callback(void* arg);
};
