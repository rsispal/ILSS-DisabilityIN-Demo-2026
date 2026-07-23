#include "Buzzer.h"
#include "../../lowlevel/buzzer/BuzzerLowLevelDriver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

// Initialise the singleton instance
Buzzer *Buzzer::instance = nullptr;

// Timer state structure for pattern execution
struct BuzzerTimerState {
    Buzzer* buzzer;
    enum PatternType {
        NONE,
        CODE3_TEMPORAL,
        CODE3_SWEEP,
        CODE3_SIREN
    } patternType;
    
    // Pattern parameters
    uint32_t freq;
    uint32_t low_freq;
    uint32_t high_freq;
    uint32_t cycles;
    uint32_t current_cycle;
    
    // State machine state
    uint8_t pulse_number;  // 0, 1, 2 (for 3 pulses)
    bool is_on;
    uint32_t step_index;   // For sweep patterns
    
    // Timing constants
    static constexpr uint32_t ON_DURATION_MS = 500;
    static constexpr uint32_t OFF_BETWEEN_MS = 500;
    static constexpr uint32_t OFF_AFTER_THIRD_MS = 1500;
    static constexpr uint32_t SWEEP_STEP_MS = 10;
};

Buzzer::Buzzer(State *state, LowLevel *lowLevel)
    : state(state), lowLevel(lowLevel), isPlaying(false)
{
    logger.setLogLevel(LogLevel::INFO);
}

Buzzer::~Buzzer()
{
    stop();
    
    // Clean up timer resources
    if (patternTimer) {
        xTimerStop(patternTimer, portMAX_DELAY);
        xTimerDelete(patternTimer, portMAX_DELAY);
        patternTimer = nullptr;
    }
    
    if (timerState) {
        delete timerState;
        timerState = nullptr;
    }
}

bool Buzzer::begin()
{
    if (m_initialized) {
        logger.LOGW(TAG, "Buzzer already initialized");
        return true;
    }
    
    if (!lowLevel)
    {
        logger.LOGE(TAG, "LowLevel is null");
        return false;
    }

    if (!lowLevel->get_buzzer().begin())
    {
        logger.LOGE(TAG, "Failed to initialise buzzer driver");
        return false;
    }

    // Create pattern timer (one-shot, we'll restart it for each step)
    patternTimer = xTimerCreate(
        "buzzer_pattern",
        pdMS_TO_TICKS(10),  // Initial period (will be changed)
        pdFALSE,            // One-shot (we'll restart it)
        this,               // Timer ID
        patternTimerCallback
    );
    if (!patternTimer) {
        logger.LOGE(TAG, "Failed to create pattern timer");
        return false;
    }

    // Allocate timer state
    timerState = new BuzzerTimerState();
    if (!timerState) {
        logger.LOGE(TAG, "Failed to allocate timer state");
        xTimerDelete(patternTimer, 0);
        patternTimer = nullptr;
        return false;
    }
    timerState->buzzer = this;
    timerState->patternType = BuzzerTimerState::NONE;

    m_initialized = true;
    logger.LOGI(TAG, "Buzzer initialised successfully");
    return true;
}

bool Buzzer::isReady() const
{
    return m_initialized && lowLevel != nullptr && lowLevel->get_buzzer().isReady();
}

void Buzzer::tick(uint32_t freq)
{
    play(freq, 50); // 50ms for a short click sound
}

void Buzzer::beep(uint32_t freq, uint32_t duration_ms)
{
    play(freq, duration_ms);
}

void Buzzer::playToneKeepAlive(uint32_t freq, uint32_t on_ms, const std::function<void()>& pump)
{
    if (!lowLevel || !lowLevel->get_buzzer().isReady()) {
        for (uint32_t t = 0; t < on_ms; t += 20) {
            if (pump) pump();
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        return;
    }
    if (!lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50))) {
        return;
    }
    isPlaying = true;
    for (uint32_t t = 0; t < on_ms; t += 20) {
        if (pump) pump();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    lowLevel->get_buzzer().setDutyOnly(0);
}

void Buzzer::silenceKeepAlive(uint32_t gap_ms, const std::function<void()>& pump)
{
    if (lowLevel && lowLevel->get_buzzer().isReady()) {
        lowLevel->get_buzzer().setDutyOnly(0);
    }
    for (uint32_t t = 0; t < gap_ms; t += 20) {
        if (pump) pump();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void Buzzer::startContinuous(uint32_t freq)
{
    if (!lowLevel || !lowLevel->get_buzzer().isReady())
    {
        return;
    }

    if (lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50)))
    {
        isPlaying = true;
    }
}

void Buzzer::clearPending()
{
    pendingSiren.store(0, std::memory_order_relaxed);
    pendingCode3Temporal.store(0, std::memory_order_relaxed);
    pendingCode3Sweep.store(0, std::memory_order_relaxed);
    pendingCode3Siren.store(0, std::memory_order_relaxed);
    pendingBeep.store(0, std::memory_order_relaxed);
    pendingTick.store(0, std::memory_order_relaxed);
    pendingLFBuzz.store(0, std::memory_order_relaxed);
    pendingMediumSweep.store(0, std::memory_order_relaxed);
    pendingAlternating.store(0, std::memory_order_relaxed);
}

void Buzzer::requestStop()
{
    // Set flag to interrupt any playing pattern immediately
    shouldStop.store(true, std::memory_order_relaxed);
}

void Buzzer::stop()
{
    // Set flag to interrupt any playing pattern
    shouldStop.store(true, std::memory_order_relaxed);

    // Drop any queued patterns so processPending cannot restart after silence.
    clearPending();

    // Stop timer-based patterns
    stopPatternTimer();

    if (!lowLevel || !lowLevel->get_buzzer().isReady())
    {
        return;
    }

    if (lowLevel->get_buzzer().stop())
    {
        isPlaying = false;
    }
}

void Buzzer::stopHardware()
{
    // Stop hardware without setting shouldStop flag (for internal use in patterns)
    if (!lowLevel || !lowLevel->get_buzzer().isReady())
    {
        return;
    }

    if (lowLevel->get_buzzer().stop())
    {
        isPlaying = false;
    }
}

void Buzzer::startPatternTimer(uint32_t interval_ms)
{
    if (patternTimer) {
        xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(interval_ms), 0);
        xTimerStart(patternTimer, 0);
    }
}

void Buzzer::stopPatternTimer()
{
    if (patternTimer) {
        xTimerStop(patternTimer, 0);
    }
    if (timerState) {
        timerState->patternType = BuzzerTimerState::NONE;
    }
}

void Buzzer::test()
{
    tick(200); // Low frequency click for button press
    vTaskDelay(pdMS_TO_TICKS(1000));

    beep(3000, 200);
    vTaskDelay(pdMS_TO_TICKS(1000));

    playAlternating(1000, 740, 8);
    vTaskDelay(pdMS_TO_TICKS(1000));

    playMediumSweep(800, 970, 8);
    vTaskDelay(pdMS_TO_TICKS(1000));

    playSiren(2700, 3500, 24);
    vTaskDelay(pdMS_TO_TICKS(1000));

    playLFBuzz(800, 970, 300);
}

void Buzzer::playAlternating(uint32_t low_freq, uint32_t high_freq, uint32_t cycles)
{
    if (!lowLevel || !lowLevel->get_buzzer().isReady())
    {
        return;
    }

    // Reset stop flag at start
    shouldStop.store(false, std::memory_order_relaxed);

    const uint32_t duration_ms = 250; // 250ms per tone (2 Hz = 500ms cycle)

    if (cycles == 0)
    {
        // Infinite loop - runs until shouldStop is set
        while (!shouldStop.load(std::memory_order_relaxed))
        {
            // Low tone
            if (lowLevel->get_buzzer().setPWM(low_freq, static_cast<uint32_t>(50)))
            {
                vTaskDelay(pdMS_TO_TICKS(duration_ms));
            }
            if (shouldStop.load(std::memory_order_relaxed)) break;

            // High tone
            if (lowLevel->get_buzzer().setPWM(high_freq, static_cast<uint32_t>(50)))
            {
                vTaskDelay(pdMS_TO_TICKS(duration_ms));
            }
        }
        stopHardware();
    }
    else
    {
        for (uint32_t i = 0; i < cycles && !shouldStop.load(std::memory_order_relaxed); i++)
        {
            // Low tone
            if (lowLevel->get_buzzer().setPWM(low_freq, static_cast<uint32_t>(50)))
            {
                vTaskDelay(pdMS_TO_TICKS(duration_ms));
            }
            if (shouldStop.load(std::memory_order_relaxed)) break;

            // High tone
            if (lowLevel->get_buzzer().setPWM(high_freq, static_cast<uint32_t>(50)))
            {
                vTaskDelay(pdMS_TO_TICKS(duration_ms));
            }
        }
        stopHardware();
    }
}

void Buzzer::playMediumSweep(uint32_t low_freq, uint32_t high_freq, uint32_t cycles, uint32_t cycle_ms)
{
    if (!lowLevel || !lowLevel->get_buzzer().isReady())
    {
        return;
    }

    // Reset stop flag at start
    shouldStop.store(false, std::memory_order_relaxed);

    // bs-sweep ≈ 1000ms; bs-fast-sweep ≈ 150ms per upward sweep.
    if (cycle_ms < 40) {
        cycle_ms = 40;
    }
    const uint32_t step_ms = 20;                                   // 20ms steps for smooth sweep
    uint32_t steps = cycle_ms / step_ms;                           // Steps per cycle (upward only)
    if (steps < 2) {
        steps = 2;
    }
    const float freq_step = (high_freq - low_freq) / (float)steps; // Frequency increment per step

    if (cycles == 0)
    {
        // Infinite loop - runs until shouldStop is set
        while (!shouldStop.load(std::memory_order_relaxed))
        {
            // Sweep up: low_freq to high_freq
            for (uint32_t i = 0; i < steps && !shouldStop.load(std::memory_order_relaxed); i++)
            {
                uint32_t freq = low_freq + (uint32_t)(i * freq_step);
                if (lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50)))
                {
                    vTaskDelay(pdMS_TO_TICKS(step_ms));
                }
            }
        }
        stopHardware();
    }
    else
    {
        for (uint32_t c = 0; c < cycles && !shouldStop.load(std::memory_order_relaxed); c++)
        {
            // Sweep up: low_freq to high_freq
            for (uint32_t i = 0; i < steps && !shouldStop.load(std::memory_order_relaxed); i++)
            {
                uint32_t freq = low_freq + (uint32_t)(i * freq_step);
                if (lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50)))
                {
                    vTaskDelay(pdMS_TO_TICKS(step_ms));
                }
            }
        }
        stopHardware();
    }
}

void Buzzer::playSiren(uint32_t low_freq, uint32_t high_freq, uint32_t cycles)
{
    if (!lowLevel || !lowLevel->get_buzzer().isReady())
    {
        return;
    }

    // Reset stop flag at start
    shouldStop.store(false, std::memory_order_relaxed);

    // Smooth continuous siren: ~7Hz oscillation with smooth sweep
    // Each full cycle (up + down) takes ~140ms for warbling effect
    const uint32_t step_ms = 10;                                   // 10ms steps (FreeRTOS safe)
    const uint32_t steps_per_sweep = 7;                            // 7 steps per half-cycle = 70ms
    const float freq_step = (high_freq - low_freq) / (float)steps_per_sweep;

    if (cycles == 0)
    {
        // Infinite loop - runs until shouldStop is set
        while (!shouldStop.load(std::memory_order_relaxed))
        {
            // Sweep up: low_freq to high_freq
            for (uint32_t i = 0; i < steps_per_sweep && !shouldStop.load(std::memory_order_relaxed); i++)
            {
                uint32_t freq = low_freq + (uint32_t)(i * freq_step);
                lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50));
                vTaskDelay(pdMS_TO_TICKS(step_ms));
            }
            // Sweep down: high_freq to low_freq
            for (uint32_t i = 0; i < steps_per_sweep && !shouldStop.load(std::memory_order_relaxed); i++)
            {
                uint32_t freq = high_freq - (uint32_t)(i * freq_step);
                lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50));
                vTaskDelay(pdMS_TO_TICKS(step_ms));
            }
        }
        stopHardware();
    }
    else
    {
        for (uint32_t c = 0; c < cycles && !shouldStop.load(std::memory_order_relaxed); c++)
        {
            // Sweep up: low_freq to high_freq
            for (uint32_t i = 0; i < steps_per_sweep && !shouldStop.load(std::memory_order_relaxed); i++)
            {
                uint32_t freq = low_freq + (uint32_t)(i * freq_step);
                lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50));
                vTaskDelay(pdMS_TO_TICKS(step_ms));
            }
            // Sweep down: high_freq to low_freq
            for (uint32_t i = 0; i < steps_per_sweep && !shouldStop.load(std::memory_order_relaxed); i++)
            {
                uint32_t freq = high_freq - (uint32_t)(i * freq_step);
                lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50));
                vTaskDelay(pdMS_TO_TICKS(step_ms));
            }
        }
        stopHardware();
    }
}

// Timer callback for pattern state machine
void Buzzer::patternTimerCallback(TimerHandle_t xTimer)
{
    Buzzer* buzzer = static_cast<Buzzer*>(pvTimerGetTimerID(xTimer));
    if (!buzzer || !buzzer->timerState) {
        return;
    }

    BuzzerTimerState* state = buzzer->timerState;
    
    // Check if we should stop
    if (buzzer->shouldStop.load(std::memory_order_relaxed)) {
        buzzer->stopHardware();
        xTimerStop(xTimer, 0);
        state->patternType = BuzzerTimerState::NONE;
        buzzer->shouldStop.store(false, std::memory_order_relaxed);
        return;
    }
    
    // Execute pattern step based on type
    switch (state->patternType) {
        case BuzzerTimerState::CODE3_TEMPORAL:
            buzzer->executeCode3PatternStep();
                    break;
        case BuzzerTimerState::CODE3_SWEEP:
            buzzer->executeCode3SweepStep();
                break;
        case BuzzerTimerState::CODE3_SIREN:
            buzzer->executeCode3SirenStep();
                    break;
        default:
            xTimerStop(xTimer, 0);
                    break;
            }
}

void Buzzer::executeCode3PatternStep()
{
    if (!timerState || !lowLevel || !lowLevel->get_buzzer().isReady()) {
        stopPatternTimer();
        return;
    }
    
    BuzzerTimerState* state = timerState;
    
    if (state->is_on) {
        // Turn off after ON duration
        stopHardware();
        state->is_on = false;
        
        // Determine next delay based on pulse number
        uint32_t delay_ms;
        if (state->pulse_number < 2) {
            delay_ms = BuzzerTimerState::OFF_BETWEEN_MS;
        } else {
            delay_ms = BuzzerTimerState::OFF_AFTER_THIRD_MS;
        }
        
        // Check if we've completed a cycle
        if (state->pulse_number >= 2) {
            state->pulse_number = 0;
            state->current_cycle++;
            
            // Check if we've completed all cycles
            if (state->cycles > 0 && state->current_cycle >= state->cycles) {
                stopPatternTimer();
                stopHardware();
                shouldStop.store(false, std::memory_order_relaxed);
                return;
            }
        } else {
            state->pulse_number++;
        }
        
        // Schedule next step
        xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(delay_ms), 0);
        xTimerStart(patternTimer, 0);
    } else {
        // Turn on for next pulse
        if (lowLevel->get_buzzer().setPWM(state->freq, static_cast<uint32_t>(50))) {
            state->is_on = true;
            xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(BuzzerTimerState::ON_DURATION_MS), 0);
            xTimerStart(patternTimer, 0);
        } else {
            stopPatternTimer();
        }
    }
}

void Buzzer::playCode3Temporal(uint32_t freq, uint32_t cycles)
{
    if (!lowLevel || !lowLevel->get_buzzer().isReady() || !patternTimer || !timerState)
    {
        return;
    }

    // Stop any existing pattern
    stopPatternTimer();

    // Reset stop flag at start of pattern
    shouldStop.store(false, std::memory_order_relaxed);

    // Initialize timer state
    timerState->patternType = BuzzerTimerState::CODE3_TEMPORAL;
    timerState->freq = freq;
    timerState->cycles = cycles;
    timerState->current_cycle = 0;
    timerState->pulse_number = 0;
    timerState->is_on = false;

    // Start the pattern: first pulse ON
    if (lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50))) {
        timerState->is_on = true;
        xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(BuzzerTimerState::ON_DURATION_MS), 0);
        xTimerStart(patternTimer, 0);
    }
}

void Buzzer::executeCode3SweepStep()
{
    if (!timerState || !lowLevel || !lowLevel->get_buzzer().isReady()) {
        stopPatternTimer();
        return;
    }
    
    BuzzerTimerState* state = timerState;
    
    // Validate frequency parameters
    if (state->low_freq == 0 || state->high_freq == 0 || state->low_freq >= state->high_freq) {
        logger.LOGE(TAG, "Invalid frequency parameters: low=%u, high=%u", state->low_freq, state->high_freq);
        stopPatternTimer();
        stopHardware();
        return;
    }
    
    const uint32_t steps = BuzzerTimerState::ON_DURATION_MS / BuzzerTimerState::SWEEP_STEP_MS;
    const float freq_step = (state->high_freq - state->low_freq) / (float)steps;
    
    if (state->is_on) {
        // Update frequency for sweep
        if (state->step_index < steps) {
            uint32_t freq = state->low_freq + (uint32_t)(state->step_index * freq_step);
            
            // Validate calculated frequency
            if (freq == 0 || freq < 100 || freq > 20000) {
                logger.LOGW(TAG, "Invalid calculated frequency: %u, clamping", freq);
                freq = (freq == 0) ? state->low_freq : ((freq < 100) ? 100 : 20000);
            }
            
            if (!lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50))) {
                logger.LOGE(TAG, "Failed to set PWM frequency: %u", freq);
                stopPatternTimer();
                stopHardware();
                return;
            }
            
            state->step_index++;
            // Continue sweep - schedule next step
            xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(BuzzerTimerState::SWEEP_STEP_MS), 0);
            xTimerStart(patternTimer, 0);
        } else {
            // Sweep complete, turn off
            stopHardware();
            state->is_on = false;
            state->step_index = 0;
            
            // Determine next delay based on pulse number
            uint32_t delay_ms;
            if (state->pulse_number < 2) {
                delay_ms = BuzzerTimerState::OFF_BETWEEN_MS;
            } else {
                delay_ms = BuzzerTimerState::OFF_AFTER_THIRD_MS;
            }
            
            // Check if we've completed a cycle
            if (state->pulse_number >= 2) {
                state->pulse_number = 0;
                state->current_cycle++;
                
                // Check if we've completed all cycles
                if (state->cycles > 0 && state->current_cycle >= state->cycles) {
                    stopPatternTimer();
                    stopHardware();
                    shouldStop.store(false, std::memory_order_relaxed);
                    return;
                }
                // For infinite cycles (cycles=0), continue to next cycle immediately after delay
            } else {
                state->pulse_number++;
            }
            
            // Schedule next step
            xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(delay_ms), 0);
            xTimerStart(patternTimer, 0);
        }
    } else {
        // Turn on for next pulse (start sweep)
        state->is_on = true;
        state->step_index = 0;
        
        // Validate low_freq before starting
        if (state->low_freq == 0 || state->low_freq < 100 || state->low_freq > 20000) {
            logger.LOGE(TAG, "Invalid low_freq: %u", state->low_freq);
            stopPatternTimer();
            stopHardware();
            return;
        }
        
        if (!lowLevel->get_buzzer().setPWM(state->low_freq, static_cast<uint32_t>(50))) {
            logger.LOGE(TAG, "Failed to start sweep pulse");
            stopPatternTimer();
            stopHardware();
            return;
        }
        
        xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(BuzzerTimerState::SWEEP_STEP_MS), 0);
        xTimerStart(patternTimer, 0);
    }
}

void Buzzer::playCode3Sweep(uint32_t low_freq, uint32_t high_freq, uint32_t cycles)
{
    if (!lowLevel || !lowLevel->get_buzzer().isReady() || !patternTimer || !timerState)
    {
        return;
    }

    // Validate frequency parameters
    if (low_freq == 0 || high_freq == 0 || low_freq >= high_freq || low_freq < 100 || high_freq > 20000) {
        logger.LOGE(TAG, "Invalid frequency parameters: low=%u, high=%u", low_freq, high_freq);
        return;
    }

    // Stop any existing pattern
    stopPatternTimer();

    // Reset stop flag at start of pattern
    shouldStop.store(false, std::memory_order_relaxed);

    // Initialize timer state
    timerState->patternType = BuzzerTimerState::CODE3_SWEEP;
    timerState->low_freq = low_freq;
    timerState->high_freq = high_freq;
    timerState->cycles = cycles;
    timerState->current_cycle = 0;
    timerState->pulse_number = 0;
    timerState->is_on = false;
    timerState->step_index = 0;

    // Start the pattern: first pulse ON (start sweep)
    if (lowLevel->get_buzzer().setPWM(low_freq, static_cast<uint32_t>(50))) {
        timerState->is_on = true;
        xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(BuzzerTimerState::SWEEP_STEP_MS), 0);
        xTimerStart(patternTimer, 0);
    } else {
        logger.LOGE(TAG, "Failed to start Code-3 Sweep pattern");
    }
}

void Buzzer::executeCode3SirenStep()
{
    if (!timerState || !lowLevel || !lowLevel->get_buzzer().isReady()) {
        stopPatternTimer();
        return;
    }
    
    BuzzerTimerState* state = timerState;
    
    // Validate frequency parameters to prevent invalid calculations
    if (state->low_freq == 0 || state->high_freq == 0 || state->low_freq >= state->high_freq) {
        logger.LOGE(TAG, "Invalid frequency parameters: low=%u, high=%u", state->low_freq, state->high_freq);
        stopPatternTimer();
        stopHardware();
        return;
    }
    
    const uint32_t steps_per_sweep = 7;  // 7 steps per half-cycle = 70ms per sweep
    const float freq_step = (state->high_freq - state->low_freq) / (float)steps_per_sweep;
    const uint32_t steps_per_pulse = BuzzerTimerState::ON_DURATION_MS / BuzzerTimerState::SWEEP_STEP_MS;  // 50 steps for 500ms
    
    if (state->is_on) {
        // Update frequency for siren sweep
        uint32_t freq;
        if (state->step_index < steps_per_sweep) {
            // Sweep up
            freq = state->low_freq + (uint32_t)(state->step_index * freq_step);
        } else if (state->step_index < (steps_per_sweep * 2)) {
            // Sweep down
            uint32_t down_index = state->step_index - steps_per_sweep;
            freq = state->high_freq - (uint32_t)(down_index * freq_step);
        } else {
            // Continue repeating the siren pattern within the pulse
            // Wrap around: repeat up/down pattern
            uint32_t wrapped_index = state->step_index % (steps_per_sweep * 2);
            if (wrapped_index < steps_per_sweep) {
                freq = state->low_freq + (uint32_t)(wrapped_index * freq_step);
            } else {
                uint32_t down_index = wrapped_index - steps_per_sweep;
                freq = state->high_freq - (uint32_t)(down_index * freq_step);
            }
        }
        
        // Ensure frequency is valid (not zero)
        if (freq == 0 || freq < 100 || freq > 20000) {
            logger.LOGW(TAG, "Invalid calculated frequency: %u, using low_freq", freq);
            freq = state->low_freq;
        }
        
        if (!lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50))) {
            logger.LOGE(TAG, "Failed to set PWM frequency: %u", freq);
            stopPatternTimer();
            stopHardware();
            return;
        }
        
        state->step_index++;
        
        // Check if we've completed the pulse (500ms = 50 steps of 10ms)
        if (state->step_index >= steps_per_pulse) {
            // Pulse complete, turn off
            stopHardware();
            state->is_on = false;
            state->step_index = 0;
            
            // Determine next delay based on pulse number
            uint32_t delay_ms;
            if (state->pulse_number < 2) {
                delay_ms = BuzzerTimerState::OFF_BETWEEN_MS;
            } else {
                delay_ms = BuzzerTimerState::OFF_AFTER_THIRD_MS;
            }
            
            // Check if we've completed a cycle
            if (state->pulse_number >= 2) {
                state->pulse_number = 0;
                state->current_cycle++;
                
                // Check if we've completed all cycles
                if (state->cycles > 0 && state->current_cycle >= state->cycles) {
                    stopPatternTimer();
                    stopHardware();
                    shouldStop.store(false, std::memory_order_relaxed);
                    return;
                }
                // For infinite cycles (cycles=0), continue to next cycle immediately after delay
            } else {
                state->pulse_number++;
            }
            
            // Schedule next step (delay before next pulse)
            xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(delay_ms), 0);
            xTimerStart(patternTimer, 0);
        } else {
            // Continue siren - schedule next step
            xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(BuzzerTimerState::SWEEP_STEP_MS), 0);
            xTimerStart(patternTimer, 0);
        }
    } else {
        // Turn on for next pulse (start siren)
        state->is_on = true;
        state->step_index = 0;
        
        // Validate low_freq before starting
        if (state->low_freq == 0 || state->low_freq < 100 || state->low_freq > 20000) {
            logger.LOGE(TAG, "Invalid low_freq: %u", state->low_freq);
            stopPatternTimer();
            stopHardware();
            return;
        }
        
        if (!lowLevel->get_buzzer().setPWM(state->low_freq, static_cast<uint32_t>(50))) {
            logger.LOGE(TAG, "Failed to start siren pulse");
            stopPatternTimer();
            stopHardware();
            return;
        }
        
        xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(BuzzerTimerState::SWEEP_STEP_MS), 0);
        xTimerStart(patternTimer, 0);
    }
}

void Buzzer::playCode3Siren(uint32_t low_freq, uint32_t high_freq, uint32_t cycles)
{
    if (!lowLevel || !lowLevel->get_buzzer().isReady() || !patternTimer || !timerState)
    {
        return;
    }

    // Validate frequency parameters
    if (low_freq == 0 || high_freq == 0 || low_freq >= high_freq || low_freq < 100 || high_freq > 20000) {
        logger.LOGE(TAG, "Invalid frequency parameters: low=%u, high=%u", low_freq, high_freq);
        return;
    }

    // Stop any existing pattern
    stopPatternTimer();

    // Reset stop flag at start of pattern
    shouldStop.store(false, std::memory_order_relaxed);

    // Initialize timer state
    timerState->patternType = BuzzerTimerState::CODE3_SIREN;
    timerState->low_freq = low_freq;
    timerState->high_freq = high_freq;
    timerState->cycles = cycles;
    timerState->current_cycle = 0;
    timerState->pulse_number = 0;
    timerState->is_on = false;
    timerState->step_index = 0;

    // Start the pattern: first pulse ON (start siren)
    if (lowLevel->get_buzzer().setPWM(low_freq, static_cast<uint32_t>(50))) {
        timerState->is_on = true;
        xTimerChangePeriod(patternTimer, pdMS_TO_TICKS(BuzzerTimerState::SWEEP_STEP_MS), 0);
        xTimerStart(patternTimer, 0);
    } else {
        logger.LOGE(TAG, "Failed to start Code-3 Siren pattern");
    }
}

void Buzzer::playLFBuzz(uint32_t low_freq, uint32_t high_freq, uint32_t cycles)
{
    if (!lowLevel || !lowLevel->get_buzzer().isReady())
    {
        return;
    }

    // Reset stop flag at start
    shouldStop.store(false, std::memory_order_relaxed);

    // Simplex TrueAlert style fire horn buzz: rapid modulation for harsh buzz
    // Creates the distinctive aggressive buzzing sound of electronic fire alarm horns
    const uint32_t modulation_ms = 10;      // 10ms modulation (50Hz buzz rate - FreeRTOS safe)
    
    // Use two frequencies for modulation effect (creates the harsh buzz)
    // low_freq is the base (e.g., 600Hz), we modulate between base and base+50Hz
    const uint32_t freq1 = low_freq;           // Base frequency
    const uint32_t freq2 = low_freq + 50;      // Slightly higher for modulation

    if (cycles == 0)
    {
        // Infinite loop - runs until shouldStop is set
        bool use_freq1 = true;
        while (!shouldStop.load(std::memory_order_relaxed))
        {
            if (!lowLevel || !lowLevel->get_buzzer().isReady())
            {
                break;
            }
            uint32_t freq = use_freq1 ? freq1 : freq2;
            lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50));
            vTaskDelay(pdMS_TO_TICKS(modulation_ms));
            use_freq1 = !use_freq1;
        }
        stopHardware();
    }
    else
    {
        // Calculate total duration based on cycles (each cycle = 1 second)
        uint32_t total_duration_ms = cycles * 1000;

        // Continuous modulated buzz with no gaps
        uint32_t elapsed = 0;
        bool use_freq1 = true;
        while (elapsed < total_duration_ms && !shouldStop.load(std::memory_order_relaxed))
        {
            if (!lowLevel || !lowLevel->get_buzzer().isReady())
            {
                break;
            }
            uint32_t freq = use_freq1 ? freq1 : freq2;
            lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50));
            vTaskDelay(pdMS_TO_TICKS(modulation_ms));
            elapsed += modulation_ms;
            use_freq1 = !use_freq1;
        }
        stopHardware();
    }
}

void Buzzer::playSpeech(SpeechType type, int32_t gain)
{
    switch (type) {
        case SpeechType::FIRE_ALARM:
            playSpeech(getFireAlarmSpeech(), getFireAlarmSpeechSize(), SPEECH_SAMPLE_RATE_HZ, gain);
            break;
        case SpeechType::OCCUPANT_ALERT:
            playSpeech(getOccupantAlertSpeech(), getOccupantAlertSpeechSize(), SPEECH_SAMPLE_RATE_HZ, gain);
            break;
    }
}

void Buzzer::play(uint32_t freq, uint32_t duration_ms)
{
    if (!lowLevel || !lowLevel->get_buzzer().isReady())
    {
        return;
    }

    if (lowLevel->get_buzzer().setPWM(freq, static_cast<uint32_t>(50)))
    {
        isPlaying = true;
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        // Finite tones must not clearPending()/shouldStop — that races alert queues
        // (e.g. both-hold beep vs personal Code-3 siren).
        stopHardware();
    }
}

void Buzzer::queueSiren(uint32_t low_freq, uint32_t high_freq, uint32_t cycles)
{
    // Original siren: continuous up/down frequency sweep
    // Safe to call from interrupt context - use atomic operations
    pendingSiren.store(1, std::memory_order_relaxed);
    pendingFreq.store(low_freq, std::memory_order_relaxed);
    pendingDuration.store(high_freq, std::memory_order_relaxed); // Reuse for high_freq
    pendingCycles.store(cycles, std::memory_order_relaxed);
}

void Buzzer::queueCode3Sweep(uint32_t low_freq, uint32_t high_freq, uint32_t cycles)
{
    // Safe to call from interrupt context - use atomic operations
    pendingCode3Sweep.store(1, std::memory_order_relaxed);
    pendingFreq.store(low_freq, std::memory_order_relaxed);
    pendingDuration.store(high_freq, std::memory_order_relaxed); // Reuse for high_freq
    pendingCycles.store(cycles, std::memory_order_relaxed);
}

void Buzzer::queueCode3Siren(uint32_t low_freq, uint32_t high_freq, uint32_t cycles)
{
    // Code-3 temporal with smooth up/down siren sweep
    // Safe to call from interrupt context - use atomic operations
    pendingCode3Siren.store(1, std::memory_order_relaxed);
    pendingFreq.store(low_freq, std::memory_order_relaxed);
    pendingDuration.store(high_freq, std::memory_order_relaxed); // Reuse for high_freq
    pendingCycles.store(cycles, std::memory_order_relaxed);
}

void Buzzer::queueCode3Temporal(uint32_t freq, uint32_t cycles)
{
    // Safe to call from interrupt context - use atomic operations
    pendingCode3Temporal.store(1, std::memory_order_relaxed);
    pendingFreq.store(freq, std::memory_order_relaxed);
    pendingCycles.store(cycles, std::memory_order_relaxed);
}

void Buzzer::queueBeep(uint32_t freq, uint32_t duration_ms)
{
    // Safe to call from interrupt context - use atomic operations
    pendingBeep.store(1, std::memory_order_relaxed);
    pendingFreq.store(freq, std::memory_order_relaxed);
    pendingDuration.store(duration_ms, std::memory_order_relaxed);
}

void Buzzer::queueTick(uint32_t freq)
{
    // Safe to call from interrupt context - use atomic operations
    pendingTick.store(1, std::memory_order_relaxed);
    pendingFreq.store(freq, std::memory_order_relaxed);
}

void Buzzer::queueLFBuzz(uint32_t low_freq, uint32_t high_freq, uint32_t cycles)
{
    // Safe to call from interrupt context - use atomic operations
    pendingLFBuzz.store(1, std::memory_order_relaxed);
    pendingFreq.store(low_freq, std::memory_order_relaxed);
    pendingDuration.store(high_freq, std::memory_order_relaxed); // Reuse for high_freq
    pendingCycles.store(cycles, std::memory_order_relaxed);
}

void Buzzer::queueMediumSweep(uint32_t low_freq, uint32_t high_freq, uint32_t cycles, uint32_t cycle_ms)
{
    // Safe to call from interrupt context - use atomic operations
    pendingMediumSweep.store(1, std::memory_order_relaxed);
    pendingFreq.store(low_freq, std::memory_order_relaxed);
    pendingDuration.store(high_freq, std::memory_order_relaxed); // Reuse for high_freq
    pendingCycles.store(cycles, std::memory_order_relaxed);
    pendingCycleMs.store(cycle_ms, std::memory_order_relaxed);
}

void Buzzer::queueAlternating(uint32_t low_freq, uint32_t high_freq, uint32_t cycles)
{
    // Safe to call from interrupt context - use atomic operations
    pendingAlternating.store(1, std::memory_order_relaxed);
    pendingFreq.store(low_freq, std::memory_order_relaxed);
    pendingDuration.store(high_freq, std::memory_order_relaxed); // Reuse for high_freq
    pendingCycles.store(cycles, std::memory_order_relaxed);
}

bool Buzzer::processPendingBuzzer()
{
    // Check for pending siren using atomic operation
    uint8_t siren_pending = pendingSiren.exchange(0, std::memory_order_relaxed);
    if (siren_pending > 0)
    {
        uint32_t low_freq = pendingFreq.load(std::memory_order_relaxed);
        uint32_t high_freq = pendingDuration.load(std::memory_order_relaxed);
        uint32_t cycles = pendingCycles.load(std::memory_order_relaxed);
        playSiren(low_freq, high_freq, cycles);
        return true;
    }

    // Check for pending Code-3 temporal using atomic operation
    uint8_t code3_pending = pendingCode3Temporal.exchange(0, std::memory_order_relaxed);
    if (code3_pending > 0)
    {
        uint32_t freq = pendingFreq.load(std::memory_order_relaxed);
        uint32_t cycles = pendingCycles.load(std::memory_order_relaxed);
        playCode3Temporal(freq, cycles);
        return true;
    }

    // Check for pending Code-3 sweep using atomic operation
    uint8_t code3_sweep_pending = pendingCode3Sweep.exchange(0, std::memory_order_relaxed);
    if (code3_sweep_pending > 0)
    {
        uint32_t low_freq = pendingFreq.load(std::memory_order_relaxed);
        uint32_t high_freq = pendingDuration.load(std::memory_order_relaxed);
        uint32_t cycles = pendingCycles.load(std::memory_order_relaxed);
        playCode3Sweep(low_freq, high_freq, cycles);
        return true;
    }

    // Check for pending Code-3 siren using atomic operation
    uint8_t code3_siren_pending = pendingCode3Siren.exchange(0, std::memory_order_relaxed);
    if (code3_siren_pending > 0)
    {
        uint32_t low_freq = pendingFreq.load(std::memory_order_relaxed);
        uint32_t high_freq = pendingDuration.load(std::memory_order_relaxed);
        uint32_t cycles = pendingCycles.load(std::memory_order_relaxed);
        playCode3Siren(low_freq, high_freq, cycles);
        return true;
    }

    // Check for pending beep using atomic operation
    uint8_t beep_pending = pendingBeep.exchange(0, std::memory_order_relaxed);
    if (beep_pending > 0)
    {
        uint32_t freq = pendingFreq.load(std::memory_order_relaxed);
        uint32_t duration = pendingDuration.load(std::memory_order_relaxed);
        beep(freq, duration);
        return true;
    }

    // Check for pending tick using atomic operation
    uint8_t tick_pending = pendingTick.exchange(0, std::memory_order_relaxed);
    if (tick_pending > 0)
    {
        uint32_t freq = pendingFreq.load(std::memory_order_relaxed);
        tick(freq);
        return true;
    }

    // Check for pending lfbuzz using atomic operation
    uint8_t lfbuzz_pending = pendingLFBuzz.exchange(0, std::memory_order_relaxed);
    if (lfbuzz_pending > 0)
    {
        uint32_t low_freq = pendingFreq.load(std::memory_order_relaxed);
        uint32_t high_freq = pendingDuration.load(std::memory_order_relaxed);
        uint32_t cycles = pendingCycles.load(std::memory_order_relaxed);
        playLFBuzz(low_freq, high_freq, cycles);
        return true;
    }

    // Check for pending medium sweep using atomic operation
    uint8_t medium_sweep_pending = pendingMediumSweep.exchange(0, std::memory_order_relaxed);
    if (medium_sweep_pending > 0)
    {
        uint32_t low_freq = pendingFreq.load(std::memory_order_relaxed);
        uint32_t high_freq = pendingDuration.load(std::memory_order_relaxed);
        uint32_t cycles = pendingCycles.load(std::memory_order_relaxed);
        uint32_t cycle_ms = pendingCycleMs.load(std::memory_order_relaxed);
        playMediumSweep(low_freq, high_freq, cycles, cycle_ms);
        return true;
    }

    // Check for pending alternating using atomic operation
    uint8_t alternating_pending = pendingAlternating.exchange(0, std::memory_order_relaxed);
    if (alternating_pending > 0)
    {
        uint32_t low_freq = pendingFreq.load(std::memory_order_relaxed);
        uint32_t high_freq = pendingDuration.load(std::memory_order_relaxed);
        uint32_t cycles = pendingCycles.load(std::memory_order_relaxed);
        playAlternating(low_freq, high_freq, cycles);
        return true;
    }

    // No pending buzzer patterns found
    return false;
}

