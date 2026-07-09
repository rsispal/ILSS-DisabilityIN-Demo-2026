#pragma once

#include <cstdint>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Forward declarations
class Logger;
class I2CLowLevelDriver;

/**
 * Haptics - Feature class for DRV2605L haptic feedback
 * 
 * Provides high-level haptic feedback functionality using the DRV2605L driver.
 * Uses a FreeRTOS task for non-blocking waveform processing.
 * Safe to call from interrupt context via queueWaveform().
 */
class Haptics {
    const char* TAG = "Haptics";

public:
    // DRV2605L I2C address
    static constexpr uint8_t DRV2605_ADDR = 0x5A;

    // DRV2605L registers
    static constexpr uint8_t DRV2605_REG_STATUS = 0x00;
    static constexpr uint8_t DRV2605_REG_MODE = 0x01;
    static constexpr uint8_t DRV2605_MODE_INTTRIG = 0x00;  // Internal trigger mode
    static constexpr uint8_t DRV2605_MODE_STANDBY = 0x40;  // Standby mode
    static constexpr uint8_t DRV2605_REG_RTPIN = 0x02;
    static constexpr uint8_t DRV2605_REG_LIBRARY = 0x03;
    static constexpr uint8_t DRV2605_REG_WAVESEQ1 = 0x04;
    static constexpr uint8_t DRV2605_REG_WAVESEQ2 = 0x05;
    static constexpr uint8_t DRV2605_REG_GO = 0x0C;
    static constexpr uint8_t DRV2605_REG_OVERDRIVE = 0x0D;
    static constexpr uint8_t DRV2605_REG_SUSTAINPOS = 0x0E;
    static constexpr uint8_t DRV2605_REG_SUSTAINNEG = 0x0F;
    static constexpr uint8_t DRV2605_REG_BREAK = 0x10;
    static constexpr uint8_t DRV2605_REG_AUDIOMAX = 0x13;
    static constexpr uint8_t DRV2605_REG_FEEDBACK = 0x1A;
    static constexpr uint8_t DRV2605_REG_CONTROL1 = 0x1B;
    static constexpr uint8_t DRV2605_REG_CONTROL2 = 0x1C;
    static constexpr uint8_t DRV2605_REG_CONTROL3 = 0x1D;
    static constexpr uint8_t DRV2605_REG_CONTROL4 = 0x1E;

    // Queue configuration
    static constexpr size_t WAVEFORM_QUEUE_SIZE = 10;
    static constexpr uint32_t TASK_STACK_SIZE = 4096;
    static const UBaseType_t TASK_PRIORITY = 5;

    Haptics(Logger* logger, I2CLowLevelDriver* i2c, bool power_saving = true);
    ~Haptics();

    // Initialize DRV2605L in ERM open-loop mode and start processing task
    bool init();

    // Queue a waveform for playback (non-blocking, safe from interrupt context)
    // Returns true if queued successfully, false if queue is full
    bool queueWaveform(uint8_t waveform);

    // Stop the current waveform playback (non-blocking)
    void stop();

    // Enable power-saving mode (enter standby)
    bool enablePowerSaving();

    // Disable power-saving mode (exit standby)
    bool disablePowerSaving();

    // Check if a waveform is currently playing
    bool isPlaying() const;

private:
    Logger* logger_;
    I2CLowLevelDriver* i2c_;
    bool power_saving_;
    bool initialized_;
    
    // FreeRTOS task and queue
    TaskHandle_t task_handle_;
    QueueHandle_t waveform_queue_;
    std::atomic<bool> running_;
    
    // State tracking
    std::atomic<bool> is_playing_;

    // FreeRTOS task function
    static void taskFunction(void* pvParameters);

    // Main task loop
    void taskLoop();

    // Internal waveform playback (called from task)
    bool playWaveformInternal(uint8_t waveform);

    // Write to a DRV2605 register
    int writeReg(uint8_t reg, uint8_t value);

    // Read from a DRV2605 register
    int readReg(uint8_t reg, uint8_t* value);

    // Wait for waveform to complete by polling GO bit
    bool waitForWaveformComplete(uint32_t timeout_ms = 5000);
};
