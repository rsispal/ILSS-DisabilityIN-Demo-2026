#pragma once

#include "driver/gpio.h"


// #define HARDWAREPROTOTYPE__USING_PHASE_2_BLE_LANYARD_W_ESPRESSIF_PROCESSOR_HARDWARE 1
#define HARDWAREPROTOTYPE__USING_ESPRESSIF_BREAKOUT_LANYARD_HARDWARE 1
// #define HARDWAREPROTOTYPE__USING_ESPRESSIF_PCB_1_0_LANYARD_HARDWARE 1

#ifdef HARDWAREPROTOTYPE__USING_PHASE_2_BLE_LANYARD_W_ESPRESSIF_PROCESSOR_HARDWARE
    // I2C Bus Configuration (Seeed Studio XIAO ESP32-S3)
    #define HARDWARE_I2C_NUM           I2C_NUM_0
    #define HARDWARE_I2C_SDA_PIN       GPIO_NUM_5   // GPIO5 for SDA (I2C0_SDA_GPIO5)
    #define HARDWARE_I2C_SCL_PIN       GPIO_NUM_6   // GPIO6 for SCL (I2C0_SCL_GPIO6)
    #define HARDWARE_I2C_CLOCK_SPEED   400000       // 400kHz (I2C_BITRATE_STANDARD)

    // DRV2605L Haptic Driver
    #define HARDWARE_DRV2605_I2C_ADDR  0x5A         // I2C address (ADDR pin determines 0x5A or 0x5C)

    // Buzzer (PWM via LEDC)
    #define HARDWARE_BUZZER_PIN        GPIO_NUM_2  

    // Side Buttons
    #define HARDWARE_BUTTON_LEFT_PIN   GPIO_NUM_3 
    #define HARDWARE_BUTTON_RIGHT_PIN  GPIO_NUM_4  
    #define HARDWARE_BUTTON_ACTIVE_HIGH 0 // Active low (pressed = 0)
    #define HARDWARE_BUTTON_PULLUP 1 // Pullup enabled (matches active low configuration)

    // LED Strip (WS2813 via RMT)
    #define HARDWARE_LED_STRIP_PIN     GPIO_NUM_1 
    #define HARDWARE_LED_STRIP_LENGTH 5  // Number of LEDs in strip

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
#endif

#ifdef HARDWAREPROTOTYPE__USING_ESPRESSIF_PCB_1_0_LANYARD_HARDWARE
    // I2C Bus Configuration (ESP32-S3-WROOM-1-U1)
    #define HARDWARE_I2C_NUM           I2C_NUM_0
    #define HARDWARE_I2C_SDA_PIN       GPIO_NUM_8 
    #define HARDWARE_I2C_SCL_PIN       GPIO_NUM_9 
    #define HARDWARE_I2C_CLOCK_SPEED   400000       // 400kHz (I2C_BITRATE_STANDARD)

    // DRV2605L Haptic Driver
    #define HARDWARE_DRV2605_I2C_ADDR  0x5A         // I2C address (ADDR pin determines 0x5A or 0x5C)

    // MCP17048 Fuel Gauge
    #define HARDWARE_MCP17048_I2C_ADDR  0x6B         // I2C address (ADDR pin determines 0x5A or 0x5C)

    // MCP73871 Battery Charger
    #define HARDWARE_MCP73871_CHARGING_PIN GPIO_NUM_4
    #define HARDWARE_MCP73871_CHARGED_PIN GPIO_NUM_5
    #define HARDWARE_MCP73871_CHARGE_POWER_GOOD_PIN GPIO_NUM_6


    // Buzzer (PWM via LEDC)
    #define HARDWARE_BUZZER_PIN        GPIO_NUM_18

    // Side Buttons
    #define HARDWARE_BUTTON_LEFT_PIN   GPIO_NUM_16
    #define HARDWARE_BUTTON_RIGHT_PIN  GPIO_NUM_15 
    #define HARDWARE_BUTTON_ACTIVE_HIGH 0 // Active low (pressed = 0)
    #define HARDWARE_BUTTON_PULLUP 0 // Pulldown enabled (matches active low with external pulldown, or no pull if external resistors used)

    // LED Strip (WS2813 via RMT)
    #define HARDWARE_LED_STRIP_PIN     GPIO_NUM_39  // GPIO1 (D0)
    #define HARDWARE_LED_STRIP_LENGTH  10  // Number of LEDs in strip
#endif
