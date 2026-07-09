#include "RGBLED.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "../../application/Hardware.h"

RGBLED::RGBLED(State* state, LowLevel* lowLevel)
    : state(state), lowLevel(lowLevel), m_initialized(false),
      led_strip(nullptr),
      effect_change_requested(false),
      effect_step(0), last_update(0)
{
    logger.setLogLevel(LogLevel::DEBUG);
    current_effect.active = false;
}

RGBLED::~RGBLED()
{
    if (led_strip) {
        led_strip_del(led_strip);
        led_strip = nullptr;
    }
}

bool RGBLED::begin()
{
    if (m_initialized) {
        logger.LOGW(TAG, "RGBLED already initialized");
        return true;
    }
    
    logger.LOGI(TAG, "Initializing WS2813 LED strip...");

    // LED strip configuration
    led_strip_config_t strip_config = {
        .strip_gpio_num = HARDWARE_LED_STRIP_PIN,
        .max_leds = NUM_PIXELS,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
    };

    // RMT configuration
    led_strip_rmt_config_t rmt_config = {};
    rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;     // Use default clock source
    rmt_config.resolution_hz = 10 * 1000 * 1000;  // 10MHz
    rmt_config.flags.with_dma = false;

    // Install the LED strip driver
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (ret != ESP_OK) {
        logger.LOGE(TAG, "Failed to install LED strip driver: %s", esp_err_to_name(ret));
        return false;
    }

    // Clear the strip
    ret = led_strip_clear(led_strip);
    if (ret != ESP_OK) {
        logger.LOGE(TAG, "Failed to clear LED strip: %s", esp_err_to_name(ret));
        return false;
    }

    m_initialized = true;
    logger.LOGI(TAG, "LED strip initialized successfully (%d pixels)", NUM_PIXELS);
    return true;
}

bool RGBLED::isReady() const
{
    return m_initialized && led_strip != nullptr;
}

void RGBLED::queueEffect(LedEffect effect, LedColor color, Brightness brightness, uint32_t duration)
{
    // Interrupt-safe
    current_effect.effect = effect;
    current_effect.color = color;
    current_effect.brightness = brightness;
    current_effect.duration = duration;
    current_effect.start_time = getCurrentTimeMs();
    current_effect.active = true;
    effect_change_requested = true;
    effect_step = 0;
}

void RGBLED::stopEffect()
{
    // Interrupt-safe
    current_effect.active = false;
    current_effect.effect = LedEffect::OFF;
    effect_change_requested = true;
    
    // Reset effect state
    effect_step = 0;
    last_update = 0;
    
    // Immediately clear the strip to ensure all LEDs are off
    clearStrip();
    
    // Clear again to be absolutely sure
    vTaskDelay(pdMS_TO_TICKS(10));
    clearStrip();
    
    logger.LOGI(TAG, "LED effect stopped and strip cleared");
}

void RGBLED::process()
{
    if (!current_effect.active) {
        if (effect_change_requested) {
            clearStrip();
            effect_change_requested = false;
        }
        return;
    }

    uint32_t now = getCurrentTimeMs();
    
    // Check duration
    if (current_effect.duration > 0 && 
        (now - current_effect.start_time) >= current_effect.duration) {
        current_effect.active = false;
        clearStrip();
        return;
    }

    // Rate limiting - only update at UPDATE_RATE_MS
    if ((now - last_update) < UPDATE_RATE_MS) {
        return;
    }
    last_update = now;

    // Process current effect
    switch (current_effect.effect) {
        case LedEffect::OFF:
            clearStrip();
            break;
        case LedEffect::PULSE:
            updatePulseEffect(now);
            break;
        case LedEffect::RAPID_PULSE:
            updateRapidPulseEffect(now);
            break;
        case LedEffect::BLINK_ALTERNATE:
            updateBlinkAlternateEffect(now);
            break;
        case LedEffect::FLASH_1S:
            updateFlashEffect(now, 1000);
            break;
        case LedEffect::FLASH_2S:
            updateFlashEffect(now, 2000);
            break;
        case LedEffect::CHASE_FADE:
            updateChaseFadeEffect(now);
            break;
        case LedEffect::CONTINUOUS:
            updateContinuousEffect();
            break;
        case LedEffect::DOUBLE_FLASH:
            updateDoubleFlashEffect(now);
            break;
        case LedEffect::WATER_DROP:
            updateWaterDropEffect(now);
            break;
    }

    effect_change_requested = false;
}

void RGBLED::clearStrip()
{
    if (!led_strip) {
        return;
    }
    
    // Clear all pixels
    esp_err_t ret = led_strip_clear(led_strip);
    if (ret != ESP_OK) {
        logger.LOGE(TAG, "Failed to clear LED strip: %s", esp_err_to_name(ret));
    } else {
        logger.LOGD(TAG, "LED strip cleared (all %d pixels set to 0,0,0)", NUM_PIXELS);
    }
}

void RGBLED::updatePulseEffect(uint32_t now)
{
    // Smooth sine wave pulse - 2 second cycle
    float phase = (now % 2000) / 2000.0f * 2 * M_PI;
    float intensity = (sinf(phase) + 1.0f) / 2.0f;
    
    uint8_t r, g, b;
    getColorWithBrightness(current_effect.color, current_effect.brightness, intensity, &r, &g, &b);
    setAllPixels(r, g, b);
}

void RGBLED::updateRapidPulseEffect(uint32_t now)
{
    // Rapid sine wave pulse - 500ms cycle
    float phase = (now % 500) / 500.0f * 2 * M_PI;
    float intensity = (sinf(phase) + 1.0f) / 2.0f;
    
    uint8_t r, g, b;
    getColorWithBrightness(current_effect.color, current_effect.brightness, intensity, &r, &g, &b);
    setAllPixels(r, g, b);
}

void RGBLED::updateBlinkAlternateEffect(uint32_t now)
{
    // On/off toggle - 1 second cycle, 50% duty
    bool on = (now % 1000) < 500;
    
    uint8_t r, g, b;
    getColorWithBrightness(current_effect.color, current_effect.brightness, on ? 1.0f : 0.0f, &r, &g, &b);
    setAllPixels(r, g, b);
}

void RGBLED::updateFlashEffect(uint32_t now, uint32_t period)
{
    // Quick flash (100ms on) every period
    bool on = (now % period) < 100;
    
    uint8_t r, g, b;
    getColorWithBrightness(current_effect.color, current_effect.brightness, on ? 1.0f : 0.0f, &r, &g, &b);
    setAllPixels(r, g, b);
}

void RGBLED::updateChaseFadeEffect(uint32_t now)
{
    // Chasing LED with fade trail - 100ms per step
    uint32_t step = (now / 100) % NUM_PIXELS;
    
    for (uint8_t i = 0; i < NUM_PIXELS; i++) {
        float intensity = 0.0f;
        
        if (i == step) {
            intensity = 1.0f;  // Leading LED at full brightness
        } else if (i == (step + NUM_PIXELS - 1) % NUM_PIXELS) {
            intensity = 0.3f;  // First trailing LED
        } else if (i == (step + NUM_PIXELS - 2) % NUM_PIXELS) {
            intensity = 0.1f;  // Second trailing LED
        }
        
        uint8_t r, g, b;
        getColorWithBrightness(current_effect.color, current_effect.brightness, intensity, &r, &g, &b);
        
        esp_err_t ret = led_strip_set_pixel(led_strip, i, r, g, b);
        if (ret != ESP_OK) {
            logger.LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(ret));
        }
    }
    
    esp_err_t ret = led_strip_refresh(led_strip);
    if (ret != ESP_OK) {
        logger.LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(ret));
    }
}

void RGBLED::updateContinuousEffect()
{
    // Solid color at full intensity
    uint8_t r, g, b;
    getColorWithBrightness(current_effect.color, current_effect.brightness, 1.0f, &r, &g, &b);
    setAllPixels(r, g, b);
}

void RGBLED::updateDoubleFlashEffect(uint32_t now)
{
    // Double flash pattern - two 100ms flashes with 100ms gap, then 1.2s off
    uint32_t cycle = now % 1500;
    bool on = (cycle < 100) || (cycle >= 200 && cycle < 300);
    
    uint8_t r, g, b;
    getColorWithBrightness(current_effect.color, current_effect.brightness, on ? 1.0f : 0.0f, &r, &g, &b);
    setAllPixels(r, g, b);
}

void RGBLED::updateWaterDropEffect(uint32_t now)
{
    // Water drop effect: Starts in center, simultaneously propagates left and right, then fades out
    // Total duration: ~1000ms (1 second)
    // Phase 1 (0-150ms): Center LEDs light up at full brightness
    // Phase 2 (150-600ms): Simultaneous propagation left and right from center
    // Phase 3 (600-1000ms): Fade out from all lit LEDs
    
    if (NUM_PIXELS == 0) {
        return;  // Safety check
    }
    
    uint32_t elapsed = now - current_effect.start_time;
    
    // Calculate center region (middle 3 LEDs, or fewer if strip is small)
    uint8_t center_start = (NUM_PIXELS >= 3) ? (NUM_PIXELS / 2 - 1) : 0;
    uint8_t center_end = (NUM_PIXELS >= 3) ? (NUM_PIXELS / 2 + 1) : (NUM_PIXELS - 1);
    if (center_end >= NUM_PIXELS) {
        center_end = NUM_PIXELS - 1;
    }
    
    // Calculate maximum propagation distance (limited by strip length)
    uint8_t max_propagation_left = center_start;
    uint8_t max_propagation_right = (NUM_PIXELS - 1) - center_end;
    uint8_t max_steps = (max_propagation_left > max_propagation_right) ? 
                        max_propagation_left : max_propagation_right;
    
    // Clear all LEDs first
    clearStrip();
    
    if (elapsed < 150) {
        // Phase 1: Center LEDs at full brightness
        uint8_t r, g, b;
        getColorWithBrightness(current_effect.color, current_effect.brightness, 1.0f, &r, &g, &b);
        for (uint8_t i = center_start; i <= center_end && i < NUM_PIXELS; i++) {
            led_strip_set_pixel(led_strip, i, r, g, b);
        }
    } else if (elapsed < 600) {
        // Phase 2: Simultaneous propagation left and right from center
        uint32_t phase_time = elapsed - 150;
        // Calculate how far the wave has propagated
        // Total phase time is 450ms, so each LED step takes ~150ms
        uint32_t step = phase_time / 150;  // 0, 1, 2, 3, etc. (limited by max_steps)
        if (step > max_steps) {
            step = max_steps;
        }
        
        uint8_t r, g, b;
        
        // Center LEDs - always lit during propagation
        getColorWithBrightness(current_effect.color, current_effect.brightness, 1.0f, &r, &g, &b);
        for (uint8_t i = center_start; i <= center_end && i < NUM_PIXELS; i++) {
            led_strip_set_pixel(led_strip, i, r, g, b);
        }
        
        // Propagate left from center
        for (uint32_t s = 1; s <= step && s <= max_propagation_left; s++) {
            uint8_t pixel_idx = center_start - s;
            if (pixel_idx < NUM_PIXELS) {  // Safety check
                led_strip_set_pixel(led_strip, pixel_idx, r, g, b);
            }
        }
        
        // Propagate right from center
        for (uint32_t s = 1; s <= step && s <= max_propagation_right; s++) {
            uint8_t pixel_idx = center_end + s;
            if (pixel_idx < NUM_PIXELS) {  // Safety check
                led_strip_set_pixel(led_strip, pixel_idx, r, g, b);
            }
        }
    } else if (elapsed < 1000) {
        // Phase 3: Fade out from all lit LEDs
        uint32_t phase_time = elapsed - 600;
        float fade = 1.0f - (phase_time / 400.0f);  // Fade from 1.0 to 0.0 over 400ms
        
        uint8_t r, g, b;
        getColorWithBrightness(current_effect.color, current_effect.brightness, fade, &r, &g, &b);
        
        // Fade all LEDs that could have been lit during propagation
        for (uint8_t i = 0; i < NUM_PIXELS; i++) {
            led_strip_set_pixel(led_strip, i, r, g, b);
        }
    }
    // After 1000ms, effect will be stopped by duration check
    
    esp_err_t ret = led_strip_refresh(led_strip);
    if (ret != ESP_OK) {
        logger.LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(ret));
    }
}

void RGBLED::setAllPixels(uint8_t r, uint8_t g, uint8_t b)
{
    if (!led_strip) {
        return;
    }
    
    for (uint8_t i = 0; i < NUM_PIXELS; i++) {
        esp_err_t ret = led_strip_set_pixel(led_strip, i, r, g, b);
        if (ret != ESP_OK) {
            logger.LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(ret));
            return;
        }
    }
    
    esp_err_t ret = led_strip_refresh(led_strip);
    if (ret != ESP_OK) {
        logger.LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(ret));
    }
    
    // Only log every 50th update to avoid spam
    static int update_count = 0;
    if (++update_count % 50 == 0) {
        logger.LOGI(TAG, "LED update #%d: R=%d G=%d B=%d", update_count, r, g, b);
    }
}

void RGBLED::getColorWithBrightness(LedColor color, Brightness brightness, float intensity, uint8_t* r, uint8_t* g, uint8_t* b)
{
    *r = 0;
    *g = 0;
    *b = 0;
    
    // Convert brightness enum to multiplier (5% to 100% in 5% increments)
    float bright_factor = (static_cast<int>(brightness) + 1) * 5 / 100.0f;
    intensity *= bright_factor;

    // Apply color
    switch (color) {
        case LedColor::RED:
            *r = static_cast<uint8_t>(255 * intensity);
            break;
        case LedColor::GREEN:
            *g = static_cast<uint8_t>(255 * intensity);
            break;
        case LedColor::BLUE:
            *b = static_cast<uint8_t>(255 * intensity);
            break;
        case LedColor::PURPLE:
            *r = static_cast<uint8_t>(255 * intensity);
            *b = static_cast<uint8_t>(255 * intensity);
            break;
        case LedColor::YELLOW:
            *r = static_cast<uint8_t>(255 * intensity);
            *g = static_cast<uint8_t>(255 * intensity);
            break;
        case LedColor::CYAN:
            *g = static_cast<uint8_t>(255 * intensity);
            *b = static_cast<uint8_t>(255 * intensity);
            break;
        case LedColor::WHITE:
            *r = static_cast<uint8_t>(255 * intensity);
            *g = static_cast<uint8_t>(255 * intensity);
            *b = static_cast<uint8_t>(255 * intensity);
            break;
        case LedColor::ORANGE:
            *r = static_cast<uint8_t>(255 * intensity);
            *g = static_cast<uint8_t>(165 * intensity);
            break;
    }
}

uint32_t RGBLED::getCurrentTimeMs()
{
    return esp_timer_get_time() / 1000; // Convert microseconds to milliseconds
}

// Helper to wait while processing LED effects at precise 50Hz
void RGBLED::processWait(uint32_t duration_ms)
{
    uint32_t end_time = getCurrentTimeMs() + duration_ms;
    uint32_t next_update = getCurrentTimeMs();
    
    while (getCurrentTimeMs() < end_time) {
        uint32_t now = getCurrentTimeMs();
        if (now >= next_update) {
            process();  // Process LED effects
            next_update = now + UPDATE_RATE_MS;  // Strict 20ms (50Hz) timing
        }
        vTaskDelay(pdMS_TO_TICKS(1));  // Yield CPU, check every 1ms
    }
}

void RGBLED::test()
{
    logger.LOGI(TAG, "Starting LED test sequence...");
    
    // Test each color
    LedColor colors[] = {
        LedColor::RED, LedColor::GREEN, LedColor::BLUE,
        LedColor::YELLOW, LedColor::CYAN, LedColor::PURPLE,
        LedColor::ORANGE, LedColor::WHITE
    };
    
    const char* color_names[] = {
        "RED", "GREEN", "BLUE", "YELLOW",
        "CYAN", "PURPLE", "ORANGE", "WHITE"
    };
    
    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        logger.LOGI(TAG, "  %s - Continuous", color_names[i]);
        queueEffect(LedEffect::CONTINUOUS, colors[i], Brightness::B50, 1000);
        processWait(1200);  // Process LEDs while waiting
    }
    
    logger.LOGI(TAG, "  PULSE effect");
    queueEffect(LedEffect::PULSE, LedColor::BLUE, Brightness::B50, 3000);
    processWait(3200);  // Process LEDs while waiting
    
    logger.LOGI(TAG, "  CHASE_FADE effect");
    queueEffect(LedEffect::CHASE_FADE, LedColor::GREEN, Brightness::B50, 3000);
    processWait(3200);  // Process LEDs while waiting
    
    logger.LOGI(TAG, "  DOUBLE_FLASH effect");
    queueEffect(LedEffect::DOUBLE_FLASH, LedColor::RED, Brightness::B50, 3000);
    processWait(3200);  // Process LEDs while waiting
    
    logger.LOGI(TAG, "  Turning off all LEDs...");
    stopEffect();
    vTaskDelay(pdMS_TO_TICKS(100));  // Give time for the clear command to complete
    logger.LOGI(TAG, "LED test sequence complete");
}

