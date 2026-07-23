#include "../../application/Hardware.h"
#include "ButtonBringupTest.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if ILSS_BTN_TEST_USE_IOT_BUTTON
#include "iot_button.h"
#endif

static const char* TAG = "BtnBringup";

static uint32_t now_ms()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

static const char* pull_name(int pull)
{
    switch (pull) {
        case 1: return "PULLUP";
        case 2: return "PULLDOWN";
        default: return "NONE";
    }
}

static void configure_pin(gpio_num_t pin)
{
    gpio_config_t io = {};
    io.intr_type = GPIO_INTR_DISABLE;
    io.mode = GPIO_MODE_INPUT;
    io.pin_bit_mask = (1ULL << pin);
#if ILSS_BTN_TEST_PULL == 1
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
#elif ILSS_BTN_TEST_PULL == 2
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_ENABLE;
#else
    io.pull_up_en = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
#endif
    ESP_ERROR_CHECK(gpio_config(&io));
}

#if ILSS_BTN_TEST_USE_IOT_BUTTON
static void iot_cb(void* button_handle, void* usr_data)
{
    const char* name = static_cast<const char*>(usr_data);
    button_event_t ev = iot_button_get_event(static_cast<button_handle_t>(button_handle));
    ESP_LOGI(TAG, "iot_button %s: %d (%s) t=%lu",
             name, static_cast<int>(ev), iot_button_get_event_str(ev),
             static_cast<unsigned long>(now_ms()));
}

static button_handle_t make_iot_button(gpio_num_t pin, const char* name)
{
    button_config_t cfg = {};
    cfg.type = BUTTON_TYPE_GPIO;
    cfg.long_press_time = ILSS_BTN_TEST_LONG_MS;
    cfg.short_press_time = ILSS_BTN_TEST_SHORT_MS;
    cfg.gpio_button_config.gpio_num = pin;
    cfg.gpio_button_config.active_level = ILSS_BTN_TEST_ACTIVE_HIGH;
    const bool wants_pullup = (ILSS_BTN_TEST_PULL == 1);
    const bool auto_pullup = (ILSS_BTN_TEST_ACTIVE_HIGH == 0);
    if (ILSS_BTN_TEST_PULL == 0 || wants_pullup != auto_pullup) {
        cfg.gpio_button_config.disable_pull = true;
    } else {
        cfg.gpio_button_config.disable_pull = false;
    }

    button_handle_t h = iot_button_create(&cfg);
    if (!h) {
        ESP_LOGE(TAG, "iot_button_create failed for %s GPIO%d", name, pin);
        return nullptr;
    }
    iot_button_register_cb(h, BUTTON_PRESS_DOWN, iot_cb, const_cast<char*>(name));
    iot_button_register_cb(h, BUTTON_PRESS_UP, iot_cb, const_cast<char*>(name));
    iot_button_register_cb(h, BUTTON_SINGLE_CLICK, iot_cb, const_cast<char*>(name));
    iot_button_register_cb(h, BUTTON_LONG_PRESS_START, iot_cb, const_cast<char*>(name));
    iot_button_register_cb(h, BUTTON_LONG_PRESS_UP, iot_cb, const_cast<char*>(name));
    ESP_LOGI(TAG, "iot_button ready: %s GPIO%d active_high=%d disable_pull=%d",
             name, pin, ILSS_BTN_TEST_ACTIVE_HIGH,
             cfg.gpio_button_config.disable_pull ? 1 : 0);
    return h;
}
#endif

#if ILSS_BTN_TEST_PIN_HUNT
// XIAO ESP32-S3 digital pins commonly available as GPIOs (avoid USB 19/20, flash, etc.)
static const gpio_num_t kHuntPins[] = {
    GPIO_NUM_1,  // D0
    GPIO_NUM_2,  // D1 (buzzer on this board — expect activity if driven)
    GPIO_NUM_3,  // D2
    GPIO_NUM_4,  // D3
    GPIO_NUM_5,  // D4 / I2C SDA
    GPIO_NUM_6,  // D5 / I2C SCL
    GPIO_NUM_7,  // D8
    GPIO_NUM_8,  // D9  mapped left
    GPIO_NUM_9,  // D10 mapped right
    GPIO_NUM_43, // D6
    GPIO_NUM_44, // D7
};
static constexpr size_t kHuntCount = sizeof(kHuntPins) / sizeof(kHuntPins[0]);
#endif

void ilss_button_bringup_test_run()
{
#if ILSS_BTN_TEST_SWAP_PINS
    const gpio_num_t left_pin = HARDWARE_BUTTON_RIGHT_PIN;
    const gpio_num_t right_pin = HARDWARE_BUTTON_LEFT_PIN;
#else
    const gpio_num_t left_pin = HARDWARE_BUTTON_LEFT_PIN;
    const gpio_num_t right_pin = HARDWARE_BUTTON_RIGHT_PIN;
#endif

    ESP_LOGW(TAG, "========== BUTTON BRING-UP TEST (blocks app) ==========");
    ESP_LOGW(TAG, "ACTIVE_HIGH=%d  PULL=%s(%d)  SWAP=%d  IOT_BUTTON=%d  PIN_HUNT=%d",
             ILSS_BTN_TEST_ACTIVE_HIGH, pull_name(ILSS_BTN_TEST_PULL), ILSS_BTN_TEST_PULL,
             ILSS_BTN_TEST_SWAP_PINS, ILSS_BTN_TEST_USE_IOT_BUTTON, ILSS_BTN_TEST_PIN_HUNT);
    ESP_LOGW(TAG, "Mapped: left=GPIO%d right=GPIO%d  long=%dms short=%dms",
             left_pin, right_pin, ILSS_BTN_TEST_LONG_MS, ILSS_BTN_TEST_SHORT_MS);
    ESP_LOGW(TAG, "Expect idle raw=1 if active-low + pull-up.");
    ESP_LOGW(TAG, "Press PHYSICAL right side now — if R never changes, wiring != GPIO9.");
#if ILSS_BTN_TEST_PIN_HUNT
    ESP_LOGW(TAG, "PIN_HUNT on: any hunted GPIO going active-low logs HUNT gpio=N");
#endif
    ESP_LOGW(TAG, "========================================================");

    configure_pin(left_pin);
    configure_pin(right_pin);

#if ILSS_BTN_TEST_PIN_HUNT
    int hunt_last[kHuntCount];
    for (size_t i = 0; i < kHuntCount; i++) {
        configure_pin(kHuntPins[i]);
        hunt_last[i] = gpio_get_level(kHuntPins[i]);
    }
#endif

#if ILSS_BTN_TEST_USE_IOT_BUTTON
    button_handle_t left_h = make_iot_button(left_pin, "LEFT");
    button_handle_t right_h = make_iot_button(right_pin, "RIGHT");
    (void)left_h;
    (void)right_h;
#endif

    int last_l = -1;
    int last_r = -1;
    uint32_t last_dump = 0;

    for (;;) {
        const int l = gpio_get_level(left_pin);
        const int r = gpio_get_level(right_pin);
        const uint32_t t = now_ms();

        const bool l_pressed = ILSS_BTN_TEST_ACTIVE_HIGH ? (l == 1) : (l == 0);
        const bool r_pressed = ILSS_BTN_TEST_ACTIVE_HIGH ? (r == 1) : (r == 0);

        if (l != last_l || r != last_r) {
            ESP_LOGI(TAG, "EDGE t=%lu raw L=%d R=%d  pressed L=%d R=%d%s",
                     static_cast<unsigned long>(t), l, r,
                     l_pressed ? 1 : 0, r_pressed ? 1 : 0,
                     (l_pressed && r_pressed) ? "  [BOTH]" : "");
            last_l = l;
            last_r = r;
        }

#if ILSS_BTN_TEST_PIN_HUNT
        for (size_t i = 0; i < kHuntCount; i++) {
            const int v = gpio_get_level(kHuntPins[i]);
            if (v == hunt_last[i]) {
                continue;
            }
            const bool active = ILSS_BTN_TEST_ACTIVE_HIGH ? (v == 1) : (v == 0);
            ESP_LOGW(TAG, "HUNT t=%lu gpio=%d raw=%d -> %d  active=%d%s",
                     static_cast<unsigned long>(t), static_cast<int>(kHuntPins[i]),
                     hunt_last[i], v, active ? 1 : 0,
                     (kHuntPins[i] == left_pin) ? " (mapped LEFT)" :
                     (kHuntPins[i] == right_pin) ? " (mapped RIGHT)" : "");
            hunt_last[i] = v;
        }
#endif

        if (t - last_dump >= ILSS_BTN_TEST_POLL_MS) {
            last_dump = t;
            ESP_LOGI(TAG, "POLL t=%lu raw L=%d R=%d  pressed L=%d R=%d",
                     static_cast<unsigned long>(t), l, r,
                     l_pressed ? 1 : 0, r_pressed ? 1 : 0);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
