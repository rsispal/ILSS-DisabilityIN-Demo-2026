#pragma once

#include <functional>
#include "../../utils/Logger.h"
#include "../../state/State.h"
#include "../../lowlevel/LowLevel.h"
#include "iot_button.h"
#include "esp_timer.h"

/**
 * Button event types
 */
enum class ButtonEvent {
    LEFT_PRESS_DOWN,    // Left button pressed down
    LEFT_PRESS_UP,      // Left button released (after short press)
    RIGHT_PRESS_DOWN,   // Right button pressed down
    RIGHT_PRESS_UP,     // Right button released (after short press)
    LEFT_PRESS,         // Left button complete press (down + up) < 500ms
    RIGHT_PRESS,        // Right button complete press (down + up) < 500ms
    LEFT_HOLD,        // Left button held >= threshold (configurable via State)
    RIGHT_HOLD,       // Right button held >= threshold (configurable via State)
    BOTH_HOLD_DOWN,   // Both buttons pressed down simultaneously
    BOTH_HOLD,        // Both buttons held >= threshold (configurable via State)
    BOTH_HOLD_UP      // Both buttons released after hold
};

/**
 * Button callback function type
 */
typedef std::function<void(ButtonEvent)> button_event_callback_t;

/**
 * SideButtons - Button handling using espressif/button component
 * 
 * Features:
 * - Uses espressif/button component for robust button handling
 * - Support for press, hold, and simultaneous holds
 * - Automatic debouncing and event detection
 */
class SideButtons {
public:
    SideButtons(State* state, LowLevel* lowLevel, gpio_num_t left_pin = GPIO_NUM_8, gpio_num_t right_pin = GPIO_NUM_9);
    ~SideButtons();

    /**
     * Initialize button handling
     * @return true on success, false on failure
     */
    bool begin();

    /**
     * Register callback for button events
     * @param callback Function to call when button events occur
     */
    void setEventCallback(button_event_callback_t callback);

    /**
     * Get current button states
     */
    bool isLeftPressed() const;
    bool isRightPressed() const;
    
    /**
     * Check if SideButtons is ready
     */
    bool isReady() const;

private:
    static constexpr const char* TAG = "SideButtons";
    static constexpr uint32_t HOLD_THRESHOLD_MS_DEFAULT = 5000;  // 5 seconds (fallback if State not available)
    static constexpr uint32_t PRESS_THRESHOLD_MS = 500;   // 500ms
    
    /**
     * Get hold threshold from State configuration, or use default
     * @return Hold threshold in milliseconds
     */
    uint32_t getHoldThresholdMs() const;

    State* state;
    LowLevel* lowLevel;
    Logger logger;
    bool m_initialized;
    
    gpio_num_t left_pin_;
    gpio_num_t right_pin_;

    // Button driver handles
    button_handle_t left_button_handle;
    button_handle_t right_button_handle;

    // Button state tracking for combined hold detection
    volatile bool left_button_pressed;
    volatile bool right_button_pressed;
    volatile bool both_hold_detected;  // Track if both hold has been detected
    esp_timer_handle_t both_hold_timer;

    // Event callback
    button_event_callback_t event_callback;

    // Singleton instance for static callbacks
    static SideButtons* instance;

    // Button event callbacks (from espressif/button component)
    // Signature matches button_cb_t: (void *button_handle, void *usr_data)
    static void left_button_cb(void* button_handle, void* usr_data);
    static void right_button_cb(void* button_handle, void* usr_data);
    
    // Internal event processing
    void handle_button_event(gpio_num_t pin, button_event_t event);
    void trigger_event(ButtonEvent event);
    void check_both_hold();
    
    // Both hold timer callback
    static void both_hold_timer_callback(void* arg);
};
