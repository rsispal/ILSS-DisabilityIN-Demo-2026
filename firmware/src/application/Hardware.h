#pragma once

#include "driver/gpio.h"

// Temporary bring-up: cycle RGB strip R→G→B forever at boot (blocks app start).
// #define ILSS_TEMP_RGB_RGB_FLASH_BOOT 1

// Temporary bring-up: side-button matrix test (blocks app start). See
// docs/BUTTON_BRINGUP_HANDOFF.md and ButtonBringupTest.h for knobs.
// #define ILSS_TEMP_BUTTON_BRINGUP_TEST 1
// Optional overrides (defaults in ButtonBringupTest.h):
// #define ILSS_BTN_TEST_ACTIVE_HIGH 0
// #define ILSS_BTN_TEST_PULL 1          // 0=none 1=up 2=down
// #define ILSS_BTN_TEST_SWAP_PINS 0
// #define ILSS_BTN_TEST_USE_IOT_BUTTON 1
// #define ILSS_BTN_TEST_PIN_HUNT 1

// #define HARDWAREPROTOTYPE__USING_ESPRESSIF_PCB_1_0_LANYARD_HARDWARE 1
#define HARDWAREPROTOTYPE__USING_DISABILITYIN_PCB 1



#ifdef HARDWAREPROTOTYPE__USING_DISABILITYIN_PCB
    /**
     * Hardware Pin Mapping - Application Scoped
     * 
     * This file defines GPIO pin mappings for physical hardware connections.
     * Based on Zephyr devicetree overlay: boards/xiao_esp32s3.overlay
     * 
     * Seeed Studio XIAO ESP32-S3 pin assignments:
     * - Buzzer: GPIO2 (LEDC_CH0)
     * - Left Button: GPIO8 (xiao_d 9 / D9)
     * - Right Button: GPIO7 (xiao_d 8 / D8) — confirmed via PIN_HUNT (not D10/GPIO9)
     * - I2C SDA: GPIO5 (I2C0_SDA)
     * - I2C SCL: GPIO6 (I2C0_SCL)
     * 
     * This follows a strict interface pattern for reusability across different
     * hardware configurations and applications.
     */

    // I2C Bus Configuration (Seeed Studio XIAO ESP32-S3)
    #define HARDWARE_I2C_NUM           I2C_NUM_0
    #define HARDWARE_I2C_SDA_PIN       GPIO_NUM_5   // GPIO5 for SDA (I2C0_SDA_GPIO5)
    #define HARDWARE_I2C_SCL_PIN       GPIO_NUM_6   // GPIO6 for SCL (I2C0_SCL_GPIO6)
    #define HARDWARE_I2C_CLOCK_SPEED   400000       // 400kHz (I2C_BITRATE_STANDARD)

    // DRV2605L Haptic Driver
    #define HARDWARE_DRV2605_I2C_ADDR  0x5A         // I2C address (ADDR pin determines 0x5A or 0x5C)

    // ATECC608B CryptoAuthentication (default 7-bit addr)
    #define HARDWARE_ATECC608B_I2C_ADDR 0x60

    // Buzzer (PWM via LEDC)
    #define HARDWARE_BUZZER_PIN        GPIO_NUM_2  // GPIO2 (LEDC_CH0_GPIO2)

    // Side Buttons
    #define HARDWARE_BUTTON_LEFT_PIN   GPIO_NUM_8  // GPIO8 (xiao_d 9 / D9)
    #define HARDWARE_BUTTON_RIGHT_PIN  GPIO_NUM_7  // GPIO7 (xiao_d 8 / D8) — PIN_HUNT 2026-07-20
    #define HARDWARE_BUTTON_ACTIVE_HIGH 0 // Active low (pressed = 0); schematic to GND
    // Schematic has external 1k pull-ups; enable matching internal pull-up.
    #define HARDWARE_BUTTON_PULLUP 1

    // LED Strip (WS2813 via RMT)
    #define HARDWARE_LED_STRIP_PIN     GPIO_NUM_1  // GPIO1 (D0)
    #define HARDWARE_LED_STRIP_LENGTH 10

    // BLE metadata Model characteristic (read by web twin)
    #define HARDWARE_MODEL_NAME        "ILSS-LANYARD-HW1_0-DISABILITYIN"
#endif

#ifdef HARDWAREPROTOTYPE__USING_ESPRESSIF_BREAKOUT_LANYARD_HARDWARE
    /**
     * Hardware Pin Mapping - Application Scoped
     * 
     * This file defines GPIO pin mappings for physical hardware connections.
     * Based on Zephyr devicetree overlay: boards/xiao_esp32s3.overlay
     * 
     * Seeed Studio XIAO ESP32-S3 pin assignments:
     * - Buzzer: GPIO2 (LEDC_CH0)
     * - Left Button: GPIO8 (xiao_d 9 / D9)
     * - Right Button: GPIO9 (xiao_d 10 / D10)
     * - I2C SDA: GPIO5 (I2C0_SDA)
     * - I2C SCL: GPIO6 (I2C0_SCL)
     * 
     * This follows a strict interface pattern for reusability across different
     * hardware configurations and applications.
     */

    // I2C Bus Configuration (Seeed Studio XIAO ESP32-S3)
    #define HARDWARE_I2C_NUM           I2C_NUM_0
    #define HARDWARE_I2C_SDA_PIN       GPIO_NUM_5   // GPIO5 for SDA (I2C0_SDA_GPIO5)
    #define HARDWARE_I2C_SCL_PIN       GPIO_NUM_6   // GPIO6 for SCL (I2C0_SCL_GPIO6)
    #define HARDWARE_I2C_CLOCK_SPEED   400000       // 400kHz (I2C_BITRATE_STANDARD)

    // DRV2605L Haptic Driver
    #define HARDWARE_DRV2605_I2C_ADDR  0x5A         // I2C address (ADDR pin determines 0x5A or 0x5C)

    // ATECC608B CryptoAuthentication (default 7-bit addr)
    #define HARDWARE_ATECC608B_I2C_ADDR 0x60

    // Buzzer (PWM via LEDC)
    #define HARDWARE_BUZZER_PIN        GPIO_NUM_2  // GPIO2 (LEDC_CH0_GPIO2)

    // Side Buttons
    #define HARDWARE_BUTTON_LEFT_PIN   GPIO_NUM_8  // GPIO8 (xiao_d 9 / D9)
    #define HARDWARE_BUTTON_RIGHT_PIN  GPIO_NUM_9  // GPIO9 (xiao_d 10 / D10)
    #define HARDWARE_BUTTON_ACTIVE_HIGH 0 // Active low (pressed = 0)
    #define HARDWARE_BUTTON_PULLUP 0 // Pulldown enabled (matches active low with external pulldown, or no pull if external resistors used)

    // LED Strip (WS2813 via RMT)
    #define HARDWARE_LED_STRIP_PIN     GPIO_NUM_1  // GPIO1 (D0)
    #define HARDWARE_LED_STRIP_LENGTH 10

    // BLE metadata Model characteristic (read by web twin)
    #define HARDWARE_MODEL_NAME        "ILSS-Lanyard-Breakout"
#endif
