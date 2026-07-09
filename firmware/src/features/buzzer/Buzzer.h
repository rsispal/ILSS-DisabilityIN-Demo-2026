#pragma once

#include <stdint.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "../../state/State.h"
#include "../../utils/Logger.h"
#include "../../lowlevel/LowLevel.h"
#include "../../speech/SpeechData.h"

// Forward declaration
struct BuzzerTimerState;

class Buzzer
{
    const char *TAG = "Buzzer";

public:
    Buzzer(State *state, LowLevel *lowLevel);
    ~Buzzer();

    bool begin();
    
    // Check if Buzzer is ready
    bool isReady() const;

    // Basic sound methods
    void tick(uint32_t freq = 2000);
    void beep(uint32_t freq = 2000, uint32_t duration_ms = 1000);
    void startContinuous(uint32_t freq = 2000);
    void stop();
    void requestStop();  // Request immediate stop (sets flag for interruptible patterns)

    // Pattern methods
    void playAlternating(uint32_t low_freq = 800, uint32_t high_freq = 970, uint32_t cycles = 0);
    void playMediumSweep(uint32_t low_freq = 800, uint32_t high_freq = 970, uint32_t cycles = 4);
    void playSiren(uint32_t low_freq = 800, uint32_t high_freq = 970, uint32_t cycles = 14);
    void playCode3Temporal(uint32_t freq = 3000, uint32_t cycles = 0);
    void playCode3Sweep(uint32_t low_freq = 2700, uint32_t high_freq = 3500, uint32_t cycles = 0);
    void playCode3Siren(uint32_t low_freq = 2700, uint32_t high_freq = 3500, uint32_t cycles = 0);
    void playLFBuzz(uint32_t low_freq = 800, uint32_t high_freq = 970, uint32_t cycles = 100);

    // Speech playback through piezo (experimental)
    // Plays 8-bit unsigned PCM audio by modulating PWM duty cycle
    // gain: 1-5, where 1 = no boost, 2 = 2x amplification (default, best quality)
    void playSpeech(const uint8_t* data, size_t length, uint32_t sample_rate = SPEECH_SAMPLE_RATE_HZ, int32_t gain = 2);
    void playSpeech(SpeechType type, int32_t gain = 2);

    // Queue methods for interrupt-safe operation
    void queueSiren(uint32_t low_freq = 2700, uint32_t high_freq = 3500, uint32_t cycles = 2);
    void queueCode3Temporal(uint32_t freq = 3000, uint32_t cycles = 0);
    void queueCode3Sweep(uint32_t low_freq = 2700, uint32_t high_freq = 3500, uint32_t cycles = 0);
    void queueCode3Siren(uint32_t low_freq = 2700, uint32_t high_freq = 3500, uint32_t cycles = 0);
    void queueBeep(uint32_t freq = 2000, uint32_t duration_ms = 1000);
    void queueTick(uint32_t freq = 2000);
    void queueLFBuzz(uint32_t low_freq = 800, uint32_t high_freq = 970, uint32_t cycles = 100);
    void queueMediumSweep(uint32_t low_freq = 800, uint32_t high_freq = 970, uint32_t cycles = 4);
    void queueAlternating(uint32_t low_freq = 800, uint32_t high_freq = 970, uint32_t cycles = 0);

    // Process pending patterns (call from main thread)
    bool processPendingBuzzer();

    // Test method
    void test();

    // Singleton instance needs to be public for C-style callbacks
    static Buzzer *instance;

private:
    State *state = nullptr;
    Logger logger;
    LowLevel *lowLevel = nullptr;
    bool m_initialized = false;
    bool isPlaying = false;

    // Flag to interrupt pattern playback (atomic for thread safety)
    std::atomic<bool> shouldStop{false};

    // Timer-based scheduler state
    TimerHandle_t patternTimer = nullptr;
    BuzzerTimerState* timerState = nullptr;

    // Internal play method
    void play(uint32_t freq, uint32_t duration_ms);
    
    // Stop hardware without setting shouldStop flag (for internal use in patterns)
    void stopHardware();

    // Timer-based pattern execution
    void startPatternTimer(uint32_t interval_ms);
    void stopPatternTimer();
    static void patternTimerCallback(TimerHandle_t xTimer);

    // Pattern state machine helpers
    void executeCode3PatternStep();
    void executeCode3SweepStep();
    void executeCode3SirenStep();

    // Pending buzzer patterns for deferred execution (interrupt-safe)
    std::atomic<uint8_t> pendingSiren{0};
    std::atomic<uint8_t> pendingCode3Temporal{0};
    std::atomic<uint8_t> pendingCode3Sweep{0};
    std::atomic<uint8_t> pendingCode3Siren{0};
    std::atomic<uint8_t> pendingBeep{0};
    std::atomic<uint8_t> pendingTick{0};
    std::atomic<uint8_t> pendingLFBuzz{0};
    std::atomic<uint8_t> pendingMediumSweep{0};
    std::atomic<uint8_t> pendingAlternating{0};
    std::atomic<uint32_t> pendingFreq{0};
    std::atomic<uint32_t> pendingDuration{0};
    std::atomic<uint32_t> pendingCycles{0};
};

