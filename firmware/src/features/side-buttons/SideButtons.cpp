#include "SideButtons.h"
#include "../../application/Hardware.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

SideButtons* SideButtons::instance = nullptr;

SideButtons::SideButtons(State* state, gpio_num_t left_pin, gpio_num_t right_pin)
    : state(state), m_initialized(false),
      left_pin_(left_pin), right_pin_(right_pin),
      left_button_handle(nullptr), right_button_handle(nullptr),
      left_button_pressed(false), right_button_pressed(false),
      both_hold_detected(false),
      both_hold_timer(nullptr),
      left_cue_ms_(0),
      right_cue_ms_(0),
      event_callback(nullptr)
{
    logger.setLogLevel(LogLevel::DEBUG);
    instance = this;
}

SideButtons::~SideButtons()
{
    event_callback = nullptr;

    if (both_hold_timer) {
        esp_timer_stop(both_hold_timer);
        esp_timer_delete(both_hold_timer);
        both_hold_timer = nullptr;
    }

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
    if (state) {
        return static_cast<uint32_t>(state->getPersonalAlertButtonTriggerDelayMs());
    }
    return HOLD_THRESHOLD_MS_DEFAULT;
}

uint32_t SideButtons::nowMs()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

void SideButtons::emitSoloPress(bool is_left)
{
    const uint32_t now = nowMs();
    uint32_t* last = is_left ? &left_cue_ms_ : &right_cue_ms_;
    if (*last != 0 && (now - *last) < kPressCueCooldownMs) {
        return;
    }
    *last = now;

    if (is_left) {
        logger.LOGI(TAG, "Left button PRESS (down cue)");
        trigger_event(ButtonEvent::LEFT_PRESS);
    } else {
        logger.LOGI(TAG, "Right button PRESS (down cue)");
        trigger_event(ButtonEvent::RIGHT_PRESS);
    }
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

    button_config_t button_cfg = {};
    button_cfg.type = BUTTON_TYPE_GPIO;
    button_cfg.long_press_time = hold_threshold;
    button_cfg.short_press_time = PRESS_THRESHOLD_MS;
    button_cfg.gpio_button_config.gpio_num = left_pin_;
    button_cfg.gpio_button_config.active_level = HARDWARE_BUTTON_ACTIVE_HIGH;

    bool expects_pullup = (HARDWARE_BUTTON_PULLUP == 1);
    bool auto_pullup = (HARDWARE_BUTTON_ACTIVE_HIGH == 0);

    if (expects_pullup != auto_pullup) {
        logger.LOGW(TAG, "Button pull configuration mismatch: ACTIVE_HIGH=%d, PULLUP=%d. Disabling internal pull.",
                    HARDWARE_BUTTON_ACTIVE_HIGH, HARDWARE_BUTTON_PULLUP);
        button_cfg.gpio_button_config.disable_pull = true;
    } else {
        button_cfg.gpio_button_config.disable_pull = false;
    }

    left_button_handle = iot_button_create(&button_cfg);
    if (!left_button_handle) {
        logger.LOGE(TAG, "Failed to create left button handle");
        return false;
    }

    iot_button_register_cb(left_button_handle, BUTTON_PRESS_DOWN, left_button_cb, this);
    iot_button_register_cb(left_button_handle, BUTTON_PRESS_UP, left_button_cb, this);
    iot_button_register_cb(left_button_handle, BUTTON_SINGLE_CLICK, left_button_cb, this);
    iot_button_register_cb(left_button_handle, BUTTON_LONG_PRESS_START, left_button_cb, this);
    iot_button_register_cb(left_button_handle, BUTTON_LONG_PRESS_UP, left_button_cb, this);

    button_cfg.gpio_button_config.gpio_num = right_pin_;

    right_button_handle = iot_button_create(&button_cfg);
    if (!right_button_handle) {
        logger.LOGE(TAG, "Failed to create right button handle");
        iot_button_delete(left_button_handle);
        left_button_handle = nullptr;
        return false;
    }

    iot_button_register_cb(right_button_handle, BUTTON_PRESS_DOWN, right_button_cb, this);
    iot_button_register_cb(right_button_handle, BUTTON_PRESS_UP, right_button_cb, this);
    iot_button_register_cb(right_button_handle, BUTTON_SINGLE_CLICK, right_button_cb, this);
    iot_button_register_cb(right_button_handle, BUTTON_LONG_PRESS_START, right_button_cb, this);
    iot_button_register_cb(right_button_handle, BUTTON_LONG_PRESS_UP, right_button_cb, this);

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

    // Active-low + pull-up → expect 1 when released. 0 means stuck/active.
    logger.LOGI(TAG, "Idle GPIO levels: left=%d right=%d (expect 1,1 released)",
                gpio_get_level(left_pin_), gpio_get_level(right_pin_));

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
    button_event_t event = iot_button_get_event(static_cast<button_handle_t>(button_handle));
    if (event != BUTTON_PRESS_DOWN && event != BUTTON_PRESS_UP) {
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
    button_event_t event = iot_button_get_event(static_cast<button_handle_t>(button_handle));
    if (event != BUTTON_PRESS_DOWN && event != BUTTON_PRESS_UP) {
        ESP_LOGI("SideButtons", "Right button event: %d (%s)", event, iot_button_get_event_str(event));
    }
    self->handle_button_event(self->right_pin_, event);
}

void SideButtons::handle_button_event(gpio_num_t pin, button_event_t event)
{
    const bool is_left = (pin == left_pin_);

    switch (event) {
    case BUTTON_PRESS_DOWN:
        if (is_left) {
            left_button_pressed = true;
            logger.LOGD(TAG, "Left button PRESS_DOWN");
            trigger_event(ButtonEvent::LEFT_PRESS_DOWN);
        } else {
            right_button_pressed = true;
            logger.LOGD(TAG, "Right button PRESS_DOWN");
            trigger_event(ButtonEvent::RIGHT_PRESS_DOWN);
        }
        // Solo tap UI cue on DOWN — do not wait for SHORT click / PRESS_UP.
        if (!(left_button_pressed && right_button_pressed)) {
            emitSoloPress(is_left);
        }
        check_both_hold();
        break;

    case BUTTON_PRESS_UP: {
        const bool was_both = both_hold_detected;

        if (is_left) {
            left_button_pressed = false;
            logger.LOGD(TAG, "Left button PRESS_UP");
            trigger_event(ButtonEvent::LEFT_PRESS_UP);
        } else {
            right_button_pressed = false;
            logger.LOGD(TAG, "Right button PRESS_UP");
            trigger_event(ButtonEvent::RIGHT_PRESS_UP);
        }

        if (both_hold_timer) {
            esp_timer_stop(both_hold_timer);
        }
        if (!left_button_pressed && !right_button_pressed && was_both) {
            trigger_event(ButtonEvent::BOTH_HOLD_UP);
        }
        both_hold_detected = false;
        break;
    }

    case BUTTON_SINGLE_CLICK:
        // Backup if PRESS_DOWN cue was skipped (cooldown / both-hold).
        emitSoloPress(is_left);
        break;

    case BUTTON_LONG_PRESS_START:
        if (left_button_pressed && right_button_pressed) {
            logger.LOGD(TAG, "Both buttons held - suppressing individual %s hold event",
                        is_left ? "left" : "right");
            if (both_hold_timer && !esp_timer_is_active(both_hold_timer)) {
                esp_timer_start_once(both_hold_timer, getHoldThresholdMs() * 1000);
            }
        } else if (is_left && right_button_pressed) {
            if (both_hold_timer && !esp_timer_is_active(both_hold_timer)) {
                esp_timer_start_once(both_hold_timer, getHoldThresholdMs() * 1000);
            }
        } else if (!is_left && left_button_pressed) {
            if (both_hold_timer && !esp_timer_is_active(both_hold_timer)) {
                esp_timer_start_once(both_hold_timer, getHoldThresholdMs() * 1000);
            }
        } else if (is_left) {
            logger.LOGI(TAG, "Left button HOLD started");
            trigger_event(ButtonEvent::LEFT_HOLD);
        } else {
            logger.LOGI(TAG, "Right button HOLD started");
            trigger_event(ButtonEvent::RIGHT_HOLD);
        }
        break;

    case BUTTON_LONG_PRESS_UP:
        if (is_left) {
            left_button_pressed = false;
            logger.LOGD(TAG, "Left button HOLD released");
        } else {
            right_button_pressed = false;
            logger.LOGD(TAG, "Right button HOLD released");
        }
        if (both_hold_timer) {
            esp_timer_stop(both_hold_timer);
        }
        if (!left_button_pressed && !right_button_pressed && both_hold_detected) {
            trigger_event(ButtonEvent::BOTH_HOLD_UP);
        }
        both_hold_detected = false;
        break;

    default:
        break;
    }
}

void SideButtons::check_both_hold()
{
    if (left_button_pressed && right_button_pressed) {
        if (!both_hold_detected) {
            logger.LOGI(TAG, "Both buttons pressed - BOTH_HOLD_DOWN");
            trigger_event(ButtonEvent::BOTH_HOLD_DOWN);
            both_hold_detected = true;
        }

        if (both_hold_timer && !esp_timer_is_active(both_hold_timer)) {
            logger.LOGD(TAG, "Both buttons pressed - starting combined hold timer");
            esp_timer_start_once(both_hold_timer, getHoldThresholdMs() * 1000);
        }
    } else {
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
    const bool noisy = (event == ButtonEvent::LEFT_PRESS_DOWN ||
                        event == ButtonEvent::LEFT_PRESS_UP ||
                        event == ButtonEvent::RIGHT_PRESS_DOWN ||
                        event == ButtonEvent::RIGHT_PRESS_UP);
    if (!noisy) {
        logger.LOGI(TAG, "Triggering event: %s (callback=%s)", event_name, event_callback ? "set" : "null");
    }
    if (event_callback) {
        event_callback(event);
    }
}
