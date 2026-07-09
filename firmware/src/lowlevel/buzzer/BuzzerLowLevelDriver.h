#pragma once

#include <stdint.h>
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "../../utils/Logger.h"

class BuzzerLowLevelDriver
{
    const char *TAG = "BuzzerLowLevelDriver";

public:
    BuzzerLowLevelDriver(gpio_num_t buzzer_pin = GPIO_NUM_2);  // Default: GPIO2 per Hardware.h
    ~BuzzerLowLevelDriver();

    bool begin();

    // PWM control methods
    bool setPWM(uint32_t frequency_hz, uint32_t duty_cycle_percent = 50);
    bool setPWMPeriod(uint32_t period_us, uint32_t pulse_us, bool inverted = false);
    bool setDutyOnly(uint32_t duty_percent);  // For speech: change duty without frequency change
    bool stop();
    bool isReady() const;
    
    // Speech mode: reconfigure PWM for 8-bit @ 31.25kHz (like S-Quad)
    bool enterSpeechMode();
    bool exitSpeechMode();

    // Singleton instance needs to be public for C-style callbacks
    static BuzzerLowLevelDriver *instance;

private:
    Logger logger;
    gpio_num_t buzzer_pin_;
    ledc_channel_t ledc_channel_;
    ledc_timer_t ledc_timer_;
    bool isInitialised = false;
    bool speech_mode_ = false;  // True when in 8-bit speech mode

    // Helper methods
    uint32_t frequencyToPeriodUs(uint32_t frequency_hz) const;
    uint32_t frequencyToPulseUs(uint32_t frequency_hz, uint32_t duty_cycle_percent) const;
    uint32_t calculateDuty(uint32_t duty_percent) const;
};

