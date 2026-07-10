#include "SideButtons.h"
#include "../../application/Hardware.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Initialize singleton
SideButtons* SideButtons::instance = nullptr;

SideButtons::SideButtons(State* state, gpio_num_t left_pin, gpio_num_t right_pin)
    : state(state), m_initialized(false),
      left_pin_(left_pin), right_pin_(right_pin),
      left_button_handle(nullptr), right_button_handle(nullptr),
      left_button_pressed(false), right_button_pressed(false),
      both_hold_detected(false),
      both_hold_timer(nullptr),
      event_callback(nullptr)
{
    logger.setLogLevel(LogLevel::DEBUG);
    instance = this;
}

SideButtons::~SideButtons()
{
    // Clear callback to prevent it from being called during shutdown
    event_callback = nullptr;
    
    // Stop and delete both hold timer
    if (both_hold_timer) {
        esp_timer_stop(both_hold_timer);
        esp_timer_delete(both_hold_timer);
        both_hold_timer = nullptr;
    }
    
    // Delete button handles
    if (left_button_handle) {
        iot_button_delete(left_button_handle);
        left_button_handle = nullptr;
    }
    if (right_button_handle) {
        iot_button_delete(right_button_handle);
        right_button_handle = nullptr;
    }
    
    instance = nullptr;
}

uint32_t SideButtons::getHoldThresholdMs() const
{
    // Use State's configurable value if available, otherwise use default
    if (state) {
        return static_cast<uint32_t>(state->getPersonalAlertButtonTriggerDelayMs());
    }
    return HOLD_THRESHOLD_MS_DEFAULT;
}

bool SideButtons::begin()
{
    uint32_t hold_threshold = getHoldThresholdMs();
    logger.LOGI(TAG, "Initializing side buttons using espressif/button component (Left: GPIO%d, Right: GPIO%d)...", 
                left_pin_, right_pin_);
    logger.LOGI(TAG, "Button configuration: ACTIVE_HIGH=%d, PULLUP=%d", 
                HARDWARE_BUTTON_ACTIVE_HIGH, HARDWARE_BUTTON_PULLUP);
    logger.LOGI(TAG, "Hold threshold: %lu ms (from %s)", 
                hold_threshold, state ? "State config" : "default");

    // Configure button parameters
    button_config_t button_cfg = {};
    button_cfg.type = BUTTON_TYPE_GPIO;
    button_cfg.long_press_time = hold_threshold;
    button_cfg.short_press_time = PRESS_THRESHOLD_MS;
    button_cfg.gpio_button_config.gpio_num = left_pin_;
    
    // Use hardware configuration for active level and pullup/pulldown
    // The iot_button component automatically sets pullup/pulldown based on active_level:
    // - active_level = 0 (active low) → pullup enabled automatically
    // - active_level = 1 (active high) → pulldown enabled automatically
    button_cfg.gpio_button_config.active_level = HARDWARE_BUTTON_ACTIVE_HIGH;
    
    // Validate that HARDWARE_BUTTON_PULLUP matches expected behavior
    // PULLUP=1 expects pullup (active_level=0) ✓
    // PULLUP=0 expects pulldown (active_level=1) ✓
    // If there's a mismatch, we disable pull and let external hardware handle it
    bool expects_pullup = (HARDWARE_BUTTON_PULLUP == 1);
    bool auto_pullup = (HARDWARE_BUTTON_ACTIVE_HIGH == 0);
    
    if (expects_pullup != auto_pullup) {
        // Mismatch: disable internal pull and rely on external pull resistors
        logger.LOGW(TAG, "Button pull configuration mismatch: ACTIVE_HIGH=%d, PULLUP=%d. Disabling internal pull.", 
                    HARDWARE_BUTTON_ACTIVE_HIGH, HARDWARE_BUTTON_PULLUP);
        button_cfg.gpio_button_config.disable_pull = true;
    } else {
        // Configuration matches automatic behavior - use it
        button_cfg.gpio_button_config.disable_pull = false;
    }

    // Create left button
    left_button_handle = iot_button_create(&button_cfg);
    if (!left_button_handle) {
        logger.LOGE(TAG, "Failed to create left button handle");
        return false;
    }
    
    // Register callback for left button
    iot_button_register_cb(left_button_handle, BUTTON_PRESS_DOWN, left_button_cb, this);
    iot_button_register_cb(left_button_handle, BUTTON_PRESS_UP, left_button_cb, this);
    iot_button_register_cb(left_button_handle, BUTTON_SINGLE_CLICK, left_button_cb, this);
    iot_button_register_cb(left_button_handle, BUTTON_LONG_PRESS_START, left_button_cb, this);
    iot_button_register_cb(left_button_handle, BUTTON_LONG_PRESS_HOLD, left_button_cb, this);
    iot_button_register_cb(left_button_handle, BUTTON_LONG_PRESS_UP, left_button_cb, this);

    // Configure right button
    button_cfg.gpio_button_config.gpio_num = right_pin_;
    
    // Create right button
    right_button_handle = iot_button_create(&button_cfg);
    if (!right_button_handle) {
        logger.LOGE(TAG, "Failed to create right button handle");
        iot_button_delete(left_button_handle);
        left_button_handle = nullptr;
        return false;
    }
    
    // Register callback for right button
    iot_button_register_cb(right_button_handle, BUTTON_PRESS_DOWN, right_button_cb, this);
    iot_button_register_cb(right_button_handle, BUTTON_PRESS_UP, right_button_cb, this);
    iot_button_register_cb(right_button_handle, BUTTON_SINGLE_CLICK, right_button_cb, this);
    iot_button_register_cb(right_button_handle, BUTTON_LONG_PRESS_START, right_button_cb, this);
    iot_button_register_cb(right_button_handle, BUTTON_LONG_PRESS_HOLD, right_button_cb, this);
    iot_button_register_cb(right_button_handle, BUTTON_LONG_PRESS_UP, right_button_cb, this);

    // Create timer for both hold detection
    esp_timer_create_args_t timer_args = {
        .callback = both_hold_timer_callback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "both_hold_timer",
        .skip_unhandled_events = false
    };
    esp_err_t err = esp_timer_create(&timer_args, &both_hold_timer);
    if (err != ESP_OK) {
        logger.LOGE(TAG, "Failed to create both hold timer: %s", esp_err_to_name(err));
        return false;
    }

    m_initialized = true;
    logger.LOGI(TAG, "Side buttons initialized successfully");
    return true;
}

bool SideButtons::isReady() const
{
    return m_initialized && left_button_handle != nullptr && right_button_handle != nullptr;
}

void SideButtons::setEventCallback(button_event_callback_t callback)
{
    event_callback = callback;
}

bool SideButtons::isLeftPressed() const
{
    return left_button_pressed;
}

bool SideButtons::isRightPressed() const
{
    return right_button_pressed;
}

void SideButtons::left_button_cb(void* button_handle, void* usr_data)
{
    if (!button_handle || !usr_data) {
        ESP_LOGE("SideButtons", "Button callback called with null pointers");
        return;
    }
    SideButtons* self = static_cast<SideButtons*>(usr_data);
    if (!self) {
        ESP_LOGE("SideButtons", "Failed to cast button callback usr_data");
        return;
    }
    // Get the event type from the button handle
    button_event_t event = iot_button_get_event(static_cast<button_handle_t>(button_handle));
    // Only log non-periodic events to reduce log spam
    if (event != BUTTON_LONG_PRESS_HOLD) {
        ESP_LOGI("SideButtons", "Left button event: %d (%s)", event, iot_button_get_event_str(event));
    }
    self->handle_button_event(self->left_pin_, event);
}

void SideButtons::right_button_cb(void* button_handle, void* usr_data)
{
    if (!button_handle || !usr_data) {
        ESP_LOGE("SideButtons", "Button callback called with null pointers");
        return;
    }
    SideButtons* self = static_cast<SideButtons*>(usr_data);
    if (!self) {
        ESP_LOGE("SideButtons", "Failed to cast button callback usr_data");
        return;
    }
    // Get the event type from the button handle
    button_event_t event = iot_button_get_event(static_cast<button_handle_t>(button_handle));
    // Only log non-periodic events to reduce log spam
    if (event != BUTTON_LONG_PRESS_HOLD) {
        ESP_LOGI("SideButtons", "Right button event: %d (%s)", event, iot_button_get_event_str(event));
    }
    self->handle_button_event(self->right_pin_, event);
}

void SideButtons::handle_button_event(gpio_num_t pin, button_event_t event)
{
    // Debug: Log that callback was received (but skip periodic LONG_PRESS_HOLD to reduce spam)
    if (event != BUTTON_LONG_PRESS_HOLD) {
        logger.LOGD(TAG, "Button event received: pin=%d, event=%d", pin, event);
    }
    
    bool is_left = (pin == left_pin_);
    
    switch (event) {
    case BUTTON_PRESS_DOWN:
        if (is_left) {
            left_button_pressed = true;
            logger.LOGI(TAG, "Left button PRESS_DOWN");
            trigger_event(ButtonEvent::LEFT_PRESS_DOWN);
        } else {
            right_button_pressed = true;
            logger.LOGI(TAG, "Right button PRESS_DOWN");
            trigger_event(ButtonEvent::RIGHT_PRESS_DOWN);
        }
        check_both_hold();
        break;
        
    case BUTTON_PRESS_UP:
        if (is_left) {
            left_button_pressed = false;
            logger.LOGI(TAG, "Left button PRESS_UP");
            trigger_event(ButtonEvent::LEFT_PRESS_UP);
        } else {
            right_button_pressed = false;
            logger.LOGI(TAG, "Right button PRESS_UP");
            trigger_event(ButtonEvent::RIGHT_PRESS_UP);
        }
        // Stop both hold timer if either button is released
        if (both_hold_timer) {
            esp_timer_stop(both_hold_timer);
        }
        // Reset both hold detection flag
        both_hold_detected = false;
        // Check if we need to trigger BOTH_HOLD_UP
        if (!left_button_pressed && !right_button_pressed && both_hold_detected) {
            // Both buttons were held and now both are released
            trigger_event(ButtonEvent::BOTH_HOLD_UP);
            both_hold_detected = false;
        }
        break;
        
    case BUTTON_SINGLE_CLICK:
        if (is_left) {
            logger.LOGI(TAG, "Left button PRESS (single click)");
            trigger_event(ButtonEvent::LEFT_PRESS);
        } else {
            logger.LOGI(TAG, "Right button PRESS (single click)");
            trigger_event(ButtonEvent::RIGHT_PRESS);
        }
        break;
        
    case BUTTON_LONG_PRESS_START:
        // Check if both buttons are pressed before triggering individual hold
        // If both are pressed, we're in "both hold" mode - suppress individual holds
        if (left_button_pressed && right_button_pressed) {
            // Both buttons are held - don't trigger individual hold events
            // The BOTH_HOLD will be triggered by the timer callback
            logger.LOGD(TAG, "Both buttons held - suppressing individual %s hold event", is_left ? "left" : "right");
            // Ensure the both hold timer is running (but don't trigger BOTH_HOLD_DOWN again)
            if (both_hold_timer && !esp_timer_is_active(both_hold_timer)) {
                logger.LOGD(TAG, "Starting both hold timer");
                esp_timer_start_once(both_hold_timer, getHoldThresholdMs() * 1000);
            }
        } else {
            // Only one button is held - trigger individual hold event
            // But first check if the other button might be in the process of being held
            if (is_left && right_button_pressed) {
                // Left hold started, but right is also pressed - wait for right's LONG_PRESS_START
                logger.LOGD(TAG, "Left hold started but right is also pressed - waiting for both hold");
                if (both_hold_timer && !esp_timer_is_active(both_hold_timer)) {
                    esp_timer_start_once(both_hold_timer, getHoldThresholdMs() * 1000);
                }
            } else if (!is_left && left_button_pressed) {
                // Right hold started, but left is also pressed - wait for left's LONG_PRESS_START
                logger.LOGD(TAG, "Right hold started but left is also pressed - waiting for both hold");
                if (both_hold_timer && !esp_timer_is_active(both_hold_timer)) {
                    esp_timer_start_once(both_hold_timer, getHoldThresholdMs() * 1000);
                }
            } else {
                // Only one button is held - trigger individual hold event
                if (is_left) {
                    logger.LOGI(TAG, "Left button HOLD started");
                    trigger_event(ButtonEvent::LEFT_HOLD);
                } else {
                    logger.LOGI(TAG, "Right button HOLD started");
                    trigger_event(ButtonEvent::RIGHT_HOLD);
                }
            }
        }
        break;
        
    case BUTTON_LONG_PRESS_HOLD:
        // Periodic event during long press
        // If both buttons are held, suppress these events (we're waiting for BOTH_HOLD from timer)
        if (left_button_pressed && right_button_pressed) {
            // Both buttons are held - suppress individual LONG_PRESS_HOLD events
            // The BOTH_HOLD will be triggered by the timer callback
            // Just ensure timer is running (silently, no logging)
            if (both_hold_timer && !esp_timer_is_active(both_hold_timer)) {
                esp_timer_start_once(both_hold_timer, getHoldThresholdMs() * 1000);
            }
            // Don't process further - suppress this event (exit early from switch)
            break;
        }
        // Only one button held - this is normal, but we don't need to do anything
        // (the individual hold was already triggered in LONG_PRESS_START)
        break;
        
    case BUTTON_LONG_PRESS_UP:
        if (is_left) {
            logger.LOGD(TAG, "Left button HOLD released");
            left_button_pressed = false;
        } else {
            logger.LOGD(TAG, "Right button HOLD released");
            right_button_pressed = false;
        }
        // Stop both hold timer if either button is released
        if (both_hold_timer) {
            esp_timer_stop(both_hold_timer);
        }
        // Check if we need to trigger BOTH_HOLD_UP
        if (!left_button_pressed && !right_button_pressed && both_hold_detected) {
            trigger_event(ButtonEvent::BOTH_HOLD_UP);
            both_hold_detected = false;
        }
        break;
        
    default:
        logger.LOGD(TAG, "Unhandled button event: %d", event);
        break;
    }
}

void SideButtons::check_both_hold()
{
    // Check if both buttons are pressed
    if (left_button_pressed && right_button_pressed) {
        // Both buttons are pressed - trigger BOTH_HOLD_DOWN only once
        if (!both_hold_detected) {
            logger.LOGI(TAG, "Both buttons pressed - BOTH_HOLD_DOWN");
            trigger_event(ButtonEvent::BOTH_HOLD_DOWN);
            both_hold_detected = true;
        }
        
        // Start timer for combined hold (after threshold)
        if (both_hold_timer && !esp_timer_is_active(both_hold_timer)) {
            logger.LOGD(TAG, "Both buttons pressed - starting combined hold timer");
            esp_timer_start_once(both_hold_timer, getHoldThresholdMs() * 1000);  // Convert to microseconds
        }
    } else {
        // Not both pressed - stop timer and reset flag
        if (both_hold_timer) {
            esp_timer_stop(both_hold_timer);
        }
        both_hold_detected = false;
    }
}

void SideButtons::both_hold_timer_callback(void* arg)
{
    SideButtons* self = static_cast<SideButtons*>(arg);
    if (self && self->left_button_pressed && self->right_button_pressed && self->both_hold_detected) {
        self->logger.LOGI(self->TAG, "Both buttons HOLD detected - triggering BOTH_HOLD");
        // Do not beep here: Buzzer::beep()/play() used to call stop() which
        // clearPending() + stopPatternTimer(), killing the personal/fire siren
        // that BOTH_HOLD queues. Alert audio is the feedback.
        self->trigger_event(ButtonEvent::BOTH_HOLD);
    }
}

void SideButtons::trigger_event(ButtonEvent event)
{
    const char* event_name = "";
    switch (event) {
        case ButtonEvent::LEFT_PRESS_DOWN: event_name = "LEFT_PRESS_DOWN"; break;
        case ButtonEvent::LEFT_PRESS_UP: event_name = "LEFT_PRESS_UP"; break;
        case ButtonEvent::RIGHT_PRESS_DOWN: event_name = "RIGHT_PRESS_DOWN"; break;
        case ButtonEvent::RIGHT_PRESS_UP: event_name = "RIGHT_PRESS_UP"; break;
        case ButtonEvent::LEFT_PRESS: event_name = "LEFT_PRESS"; break;
        case ButtonEvent::RIGHT_PRESS: event_name = "RIGHT_PRESS"; break;
        case ButtonEvent::LEFT_HOLD: event_name = "LEFT_HOLD"; break;
        case ButtonEvent::RIGHT_HOLD: event_name = "RIGHT_HOLD"; break;
        case ButtonEvent::BOTH_HOLD_DOWN: event_name = "BOTH_HOLD_DOWN"; break;
        case ButtonEvent::BOTH_HOLD: event_name = "BOTH_HOLD"; break;
        case ButtonEvent::BOTH_HOLD_UP: event_name = "BOTH_HOLD_UP"; break;
    }
    logger.LOGI(TAG, "Triggering event: %s (callback=%s)", event_name, event_callback ? "set" : "null");
    if (event_callback) {
        event_callback(event);
        logger.LOGD(TAG, "Event callback completed for: %s", event_name);
    } else {
        logger.LOGW(TAG, "No event callback registered for: %s", event_name);
    }
}
