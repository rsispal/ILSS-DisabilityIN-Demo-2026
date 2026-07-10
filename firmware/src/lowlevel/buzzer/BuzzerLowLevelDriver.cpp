#include "BuzzerLowLevelDriver.h"
#include "esp_log.h"

// Initialise the singleton instance
BuzzerLowLevelDriver *BuzzerLowLevelDriver::instance = nullptr;

BuzzerLowLevelDriver::BuzzerLowLevelDriver(gpio_num_t buzzer_pin)
    : buzzer_pin_(buzzer_pin), ledc_channel_(LEDC_CHANNEL_0), ledc_timer_(LEDC_TIMER_0)
{
    logger.setLogLevel(LogLevel::INFO);
}

BuzzerLowLevelDriver::~BuzzerLowLevelDriver()
{
    if (isInitialised)
    {
        stop();
    }
}

bool BuzzerLowLevelDriver::begin()
{
    if (isInitialised)
    {
        logger.LOGW(TAG, "Buzzer already initialized");
        return true;
    }

    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_timer.timer_num = ledc_timer_;
    ledc_timer.duty_resolution = LEDC_TIMER_13_BIT; // 13-bit resolution for better frequency range
    ledc_timer.freq_hz = 2000; // Default frequency, will be changed by setPWM
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;
    
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK)
    {
        logger.LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Configure LEDC channel
    ledc_channel_config_t ledc_channel = {};
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel.channel = ledc_channel_;
    ledc_channel.timer_sel = ledc_timer_;
    ledc_channel.intr_type = LEDC_INTR_DISABLE;
    ledc_channel.gpio_num = buzzer_pin_;
    ledc_channel.duty = 0; // Start with 0% duty (silent)
    ledc_channel.hpoint = 0;
    
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK)
    {
        logger.LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(ret));
        return false;
    }

    isInitialised = true;
    logger.LOGI(TAG, "Buzzer PWM driver initialised successfully on GPIO%d", buzzer_pin_);
    return true;
}

bool BuzzerLowLevelDriver::setPWM(uint32_t frequency_hz, uint32_t duty_cycle_percent)
{
    if (!isReady())
    {
        logger.LOGE(TAG, "PWM not ready");
        return false;
    }

    // After ledc_stop(), the channel may be idle — restore tone-mode timer if needed.
    if (speech_mode_)
    {
        exitSpeechMode();
    }

    uint32_t period_us = frequencyToPeriodUs(frequency_hz);
    uint32_t pulse_us = frequencyToPulseUs(frequency_hz, duty_cycle_percent);

    logger.LOGD(TAG, "Setting PWM: freq=%uHz, duty=%u%%, period=%uus, pulse=%uus", 
                frequency_hz, duty_cycle_percent, period_us, pulse_us);

    return setPWMPeriod(period_us, pulse_us, false);
}

bool BuzzerLowLevelDriver::setPWMPeriod(uint32_t period_us, uint32_t pulse_us, bool inverted)
{
    if (!isReady())
    {
        logger.LOGE(TAG, "PWM not ready");
        return false;
    }

    if (period_us == 0)
    {
        logger.LOGE(TAG, "Invalid period: 0us");
        return false;
    }

    // Calculate frequency from period
    uint32_t frequency_hz = 1000000 / period_us;
    
    // Update timer frequency
    esp_err_t ret = ledc_set_freq(LEDC_LOW_SPEED_MODE, ledc_timer_, frequency_hz);
    if (ret != ESP_OK)
    {
        logger.LOGE(TAG, "Failed to set LEDC frequency: %s", esp_err_to_name(ret));
        return false;
    }

    // Calculate duty cycle percentage from pulse_us
    uint32_t duty_percent = (pulse_us * 100) / period_us;
    if (duty_percent > 100)
    {
        duty_percent = 100;
    }

    // Set duty cycle
    uint32_t duty = calculateDuty(duty_percent);
    ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, ledc_channel_, duty);
    if (ret != ESP_OK)
    {
        logger.LOGE(TAG, "Failed to set LEDC duty: %s", esp_err_to_name(ret));
        return false;
    }

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, ledc_channel_);
    if (ret != ESP_OK)
    {
        logger.LOGE(TAG, "Failed to update LEDC duty: %s", esp_err_to_name(ret));
        return false;
    }

    logger.LOGD(TAG, "PWM set successfully: period=%uus, pulse=%uus, freq=%uHz, duty=%u%%", 
                period_us, pulse_us, frequency_hz, duty_percent);
    return true;
}

bool BuzzerLowLevelDriver::stop()
{
    if (!isReady())
    {
        return false;
    }

    // If speech mode left the timer at 8-bit / 31.25 kHz, restore tone mode first
    // so duty math and subsequent setPWM() behave correctly.
    if (speech_mode_)
    {
        exitSpeechMode();
    }

    // Silence: duty 0, then force the update through.
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, ledc_channel_, 0);
    if (ret != ESP_OK)
    {
        logger.LOGE(TAG, "Failed to stop PWM: %s", esp_err_to_name(ret));
        return false;
    }

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, ledc_channel_);
    if (ret != ESP_OK)
    {
        logger.LOGE(TAG, "Failed to update duty for stop: %s", esp_err_to_name(ret));
        return false;
    }

    // Belt-and-braces: some ESP-IDF paths leave residual output after duty=0 alone.
    (void)ledc_stop(LEDC_LOW_SPEED_MODE, ledc_channel_, 0);

    return true;
}

bool BuzzerLowLevelDriver::setDutyOnly(uint32_t duty_percent)
{
    if (!isReady())
    {
        return false;
    }

    // Set duty cycle without changing frequency (for speech playback)
    uint32_t max_duty = speech_mode_ ? 256 : 8192;  // 8-bit vs 13-bit
    uint32_t duty = (max_duty * duty_percent) / 100;
    if (duty > max_duty - 1) duty = max_duty - 1;
    
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, ledc_channel_, duty);
    if (ret != ESP_OK)
    {
        return false;
    }

    ret = ledc_update_duty(LEDC_LOW_SPEED_MODE, ledc_channel_);
    return ret == ESP_OK;
}

bool BuzzerLowLevelDriver::enterSpeechMode()
{
    if (!isReady())
    {
        return false;
    }

    // Reconfigure LEDC timer for speech: 8-bit resolution enables higher PWM frequency
    // 80MHz APB / 256 (8-bit) = 312.5kHz max, we'll use 31.25kHz like S-Quad
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_timer.timer_num = ledc_timer_;
    ledc_timer.duty_resolution = LEDC_TIMER_8_BIT;  // 8-bit for speech (256 levels)
    ledc_timer.freq_hz = 31250;  // 31.25kHz like S-Quad (2.4 PWM cycles per 13kHz sample)
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;
    
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK)
    {
        logger.LOGE(TAG, "Speech mode timer config failed: %s", esp_err_to_name(ret));
        return false;
    }

    speech_mode_ = true;
    logger.LOGI(TAG, "Entered speech mode: 8-bit @ 31.25kHz");
    return true;
}

bool BuzzerLowLevelDriver::exitSpeechMode()
{
    if (!isReady())
    {
        return false;
    }

    // Restore original 13-bit resolution for buzzer tones
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_timer.timer_num = ledc_timer_;
    ledc_timer.duty_resolution = LEDC_TIMER_13_BIT;  // Back to 13-bit for tones
    ledc_timer.freq_hz = 2000;  // Default frequency
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;
    
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK)
    {
        logger.LOGE(TAG, "Exit speech mode timer config failed: %s", esp_err_to_name(ret));
        return false;
    }

    speech_mode_ = false;
    logger.LOGI(TAG, "Exited speech mode: 13-bit @ 2kHz default");
    return true;
}

bool BuzzerLowLevelDriver::isReady() const
{
    return isInitialised;
}

uint32_t BuzzerLowLevelDriver::frequencyToPeriodUs(uint32_t frequency_hz) const
{
    if (frequency_hz == 0)
    {
        return 0;
    }
    return 1000000 / frequency_hz; // Convert Hz to microseconds
}

uint32_t BuzzerLowLevelDriver::frequencyToPulseUs(uint32_t frequency_hz, uint32_t duty_cycle_percent) const
{
    if (frequency_hz == 0 || duty_cycle_percent > 100)
    {
        return 0;
    }

    uint32_t period_us = frequencyToPeriodUs(frequency_hz);
    return (period_us * duty_cycle_percent) / 100;
}

uint32_t BuzzerLowLevelDriver::calculateDuty(uint32_t duty_percent) const
{
    // LEDC_TIMER_13_BIT = 8192 max value
    // Calculate duty: (duty_percent / 100) * 8192
    if (duty_percent > 100)
    {
        duty_percent = 100;
    }
    return (8192 * duty_percent) / 100;
}

