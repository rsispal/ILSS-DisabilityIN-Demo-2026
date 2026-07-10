#pragma once

#include <functional>
#include <cmath>
#include "../../utils/Logger.h"
#include "../../state/State.h"
#include "../../application/Hardware.h"
#include "led_strip.h"
#include "led_strip_rmt.h"
#include "esp_timer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Default LED strip length if not defined in Hardware.h
#ifndef HARDWARE_LED_STRIP_LENGTH
#define HARDWARE_LED_STRIP_LENGTH 10  // Default fallback
#endif

/**
 * LED effect types
 */
enum class LedEffect {
    OFF,
    PULSE,           // Smooth sine wave pulse (2s cycle)
    RAPID_PULSE,     // Rapid sine wave pulse (500ms cycle)
    BLINK_ALTERNATE, // Odd/even pixels swap — web alt (0.7s, 2 steps)
    HALF_HALF,       // First half vs second half of strip — web half (1.0s)
    FLASH_1S,        // Quick flash every 1s
    FLASH_2S,        // Quick flash every 2s
    FLASH_SINGLE_3S, // Single-pixel brief flash every 3s (unpaired heartbeat)
    CHASE_FADE,      // Chasing LED with fade trail
    CONTINUOUS,      // Solid color
    DOUBLE_FLASH,    // Double flash pattern
    WATER_DROP,      // Water drop: LEDs 4,5,6 light up, chase left-right, fade out
    TWINKLE          // Soft multi-colour star twinkle (power-up)
};

/**
 * LED color presets
 */
enum class LedColor {
    RED,
    GREEN,
    BLUE,
    PURPLE,
    YELLOW,
    CYAN,
    WHITE,
    ORANGE
};

/**
 * Brightness levels (5% to 100% in 5% increments)
 */
enum class Brightness {
    B5, B10, B15, B20, B25, B30, B35, B40, B45, B50,
    B55, B60, B65, B70, B75, B80, B85, B90, B95, B100
};

/**
 * RGBLED - WS2813 RGB LED Strip Controller
 * 
 * Features:
 * - Multiple visual effects (pulse, blink, chase, etc.)
 * - Color and brightness control
 * - Interrupt-safe queueing system
 * - Process-based update loop (call from main thread)
 * - Uses ESP-IDF's LED strip component (RMT-based)
 */
class RGBLED {
public:
    explicit RGBLED(State* state);
    ~RGBLED();

    /**
     * Initialize the LED strip
     * @return true on success, false on failure
     */
    bool begin();

    /**
     * Process LED effects - call this regularly from main loop
     * Should be called at least 50Hz for smooth effects
     */
    void process();
    
    /**
     * Helper to wait while continuously processing LED effects
     * @param duration_ms Duration to wait in milliseconds
     */
    void processWait(uint32_t duration_ms);

    /**
     * Queue a new LED effect (interrupt-safe)
     * @param effect The effect to display
     * @param color The color to use
     * @param brightness The brightness level
     * @param duration Duration in milliseconds (0 = infinite)
     */
    void queueEffect(LedEffect effect, LedColor color, Brightness brightness, uint32_t duration = 0);

    /**
     * Stop the current effect (interrupt-safe)
     */
    void stopEffect();

    /**
     * Test routine - cycles through effects
     */
    void test();
    
    /**
     * Check if RGBLED is ready
     */
    bool isReady() const;

private:
    static constexpr const char* TAG = "RGBLED";
    static constexpr uint8_t NUM_PIXELS = HARDWARE_LED_STRIP_LENGTH;  // Number of LEDs in strip (from Hardware.h)
    static constexpr uint32_t UPDATE_RATE_MS = 20;  // 50Hz update rate

    State* state;
    Logger logger;
    bool m_initialized;

    led_strip_handle_t led_strip;
    
    // Effect state
    struct {
        volatile bool active;
        LedEffect effect;
        LedColor color;
        Brightness brightness;
        uint32_t duration;
        uint32_t start_time;
    } current_effect;

    volatile bool effect_change_requested;
    uint32_t effect_step;
    uint32_t last_update;

    // Effect update methods
    void clearStrip();
    void updatePulseEffect(uint32_t now);
    void updateRapidPulseEffect(uint32_t now);
    void updateBlinkAlternateEffect(uint32_t now);
    void updateHalfHalfEffect(uint32_t now);
    void updateFlashEffect(uint32_t now, uint32_t period);
    void updateSingleFlashEffect(uint32_t now, uint32_t period);
    void updateChaseFadeEffect(uint32_t now);
    void updateContinuousEffect();
    void updateDoubleFlashEffect(uint32_t now);
    void updateWaterDropEffect(uint32_t now);
    void updateTwinkleEffect(uint32_t now);

    // Helper methods
    void setAllPixels(uint8_t r, uint8_t g, uint8_t b);
    void getColorWithBrightness(LedColor color, Brightness brightness, float intensity, uint8_t* r, uint8_t* g, uint8_t* b);
    uint32_t getCurrentTimeMs();
};

