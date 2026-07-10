#include "Buzzer.h"
#include "../../lowlevel/LowLevel.h"
#include "../../lowlevel/buzzer/BuzzerLowLevelDriver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "esp_task_wdt.h"

// Speech playback task parameters
struct SpeechTaskParams {
    Buzzer* buzzer;
    LowLevel* lowLevel;
    const uint8_t* data;
    size_t length;
    uint32_t sample_rate;
    int32_t gain;
    volatile bool* done;
};

// Speech playback task - runs on Core 1 to avoid starving IDLE0
static void speechPlaybackTask(void* params)
{
    SpeechTaskParams* p = static_cast<SpeechTaskParams*>(params);
    
    // Subscribe this task to watchdog
    esp_task_wdt_add(NULL);
    
    // Simple playback without interpolation - direct sample output
    // This matches S-Quad approach more closely
    const uint32_t sample_period_us = 1000000 / p->sample_rate;  // ~77us at 13kHz
    const uint32_t overhead_us = 5;  // Overhead for LEDC calls
    const uint32_t delay_us = sample_period_us > overhead_us ? sample_period_us - overhead_us : 1;
    
    // Use gain from parameters
    const int32_t gain = p->gain;
    
    // Click prevention: Start at silence
    p->lowLevel->get_buzzer().setDutyOnly(50);
    esp_rom_delay_us(1000);
    
    // Feed watchdog every N samples to prevent timeout (~77ms at 13kHz)
    const size_t wdt_feed_interval = 1000;
    
    // Play each sample with amplification
    for (size_t i = 0; i < p->length; i++)
    {
        int32_t sample = p->data[i];
        
        // Amplify: scale deviation from center (128) by gain factor
        int32_t deviation = sample - 128;
        int32_t amplified = 128 + (deviation * gain);
        
        // Clamp to valid range
        if (amplified < 0) amplified = 0;
        if (amplified > 255) amplified = 255;
        
        // Map to duty cycle (0-100%)
        uint32_t duty = (static_cast<uint32_t>(amplified) * 100) / 255;
        
        p->lowLevel->get_buzzer().setDutyOnly(duty);
        esp_rom_delay_us(delay_us);
        
        // Feed watchdog periodically
        if ((i % wdt_feed_interval) == 0)
        {
            esp_task_wdt_reset();
        }
    }
    
    // Click prevention: Ramp back to silence
    uint8_t final_sample = (p->length > 0) ? p->data[p->length - 1] : 128;
    for (uint32_t step = 1; step <= 10; step++)
    {
        int32_t ramp_value = final_sample + ((128 - static_cast<int32_t>(final_sample)) * step) / 10;
        uint32_t duty = (static_cast<uint32_t>(ramp_value) * 100) / 255;
        p->lowLevel->get_buzzer().setDutyOnly(duty);
        esp_rom_delay_us(500);
    }
    
    // Remove from watchdog before exiting
    esp_task_wdt_delete(NULL);
    
    *(p->done) = true;
    vTaskDelete(NULL);
}

void Buzzer::playSpeech(const uint8_t* data, size_t length, uint32_t sample_rate, int32_t gain)
{
    if (!lowLevel || !lowLevel->get_buzzer().isReady() || !data || length == 0)
    {
        return;
    }

    // Reset stop flag at start
    shouldStop.store(false, std::memory_order_relaxed);

    // Speech playback using PWM amplitude modulation (based on S-Quad/Vigilon approach):
    // - Unsigned 8-bit PCM: 0x00 = max negative, 0x80 (128) = silence, 0xFF = max positive
    // - PWM frequency: 31.25kHz (8-bit resolution, ~2.4 PWM cycles per 13kHz sample)
    // - Direct sample playback (no interpolation for cleaner output)
    // - Run on Core 1 to avoid starving IDLE0 watchdog
    
    logger.LOGI(TAG, "Playing speech: %u bytes at %u Hz, gain=%d", length, sample_rate, gain);
    
    // Enter speech mode: 8-bit resolution @ 31.25kHz (like S-Quad)
    if (!lowLevel->get_buzzer().enterSpeechMode())
    {
        logger.LOGE(TAG, "Failed to enter speech mode");
        return;
    }
    
    // Set up task parameters
    volatile bool taskDone = false;
    SpeechTaskParams taskParams = {
        this,
        lowLevel,
        data,
        length,
        sample_rate,
        gain,
        &taskDone
    };
    
    // Create playback task on Core 1 (high priority)
    TaskHandle_t speechTask = NULL;
    xTaskCreatePinnedToCore(
        speechPlaybackTask,
        "speech",
        4096,
        &taskParams,
        configMAX_PRIORITIES - 1,  // Highest priority
        &speechTask,
        1  // Core 1
    );
    
    // Wait for playback to complete
    while (!taskDone && !shouldStop.load(std::memory_order_relaxed))
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // If stopped early, delete the task
    if (!taskDone && speechTask != NULL)
    {
        vTaskDelete(speechTask);
    }
    
    // Exit speech mode and stop
    lowLevel->get_buzzer().exitSpeechMode();
    stopHardware();
    
    logger.LOGI(TAG, "Speech playback complete");
}

