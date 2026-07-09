#include "Haptics.h"
#include "../../utils/Logger.h"
#include "../../base/lowlevel/I2CLowLevelDriver.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <cstring>

Haptics::Haptics(Logger* logger, I2CLowLevelDriver* i2c, bool power_saving)
    : logger_(logger), i2c_(i2c), power_saving_(power_saving), initialized_(false),
      task_handle_(nullptr), waveform_queue_(nullptr), running_(false), is_playing_(false) {
}

Haptics::~Haptics() {
    // Signal task to stop
    running_.store(false, std::memory_order_release);
    
    // Notify task to wake up
    if (task_handle_ != nullptr) {
        xTaskNotify(task_handle_, 0, eNoAction);
    }
    
    // Wait for task to finish (with timeout)
    if (task_handle_ != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (eTaskGetState(task_handle_) != eDeleted) {
            vTaskDelete(task_handle_);
        }
    }
    
    // Clean up queue
    if (waveform_queue_ != nullptr) {
        vQueueDelete(waveform_queue_);
    }
    
    // Stop any ongoing playback
    if (i2c_ && i2c_->isInitialized()) {
        stop();
    }
}

bool Haptics::init() {
    if (initialized_) {
        logger_->LOGW(TAG, "Haptics already initialized");
        return true;
    }

    if (!i2c_ || !i2c_->isInitialized()) {
        logger_->LOGE(TAG, "I2C not initialized");
        return false;
    }

    // Verify device ID and check diagnostics
    uint8_t status;
    int ret = readReg(DRV2605_REG_STATUS, &status);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to read STATUS: %d", ret);
        return false;
    }
    logger_->LOGI(TAG, "STATUS register: 0x%02X", status);
    
    // Check device ID (bits 7:3 should be 0xE0)
    if ((status & 0xF8) != 0xE0) {
        logger_->LOGE(TAG, "Invalid device ID: 0x%02X (expected 0xE0)", status & 0xF8);
        return false;
    }
    
    // Clear any overcurrent condition by entering standby
    if (status & 0x04) {
        logger_->LOGW(TAG, "Overcurrent detected (OC_DETECT = 1), clearing...");
        ret = writeReg(DRV2605_REG_MODE, DRV2605_MODE_STANDBY);
        if (ret != 0) {
            logger_->LOGE(TAG, "Failed to enter standby to clear overcurrent: %d", ret);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Software reset: Set MODE register bit 7 (reset bit)
    logger_->LOGI(TAG, "Performing software reset...");
    ret = writeReg(DRV2605_REG_MODE, 0x80);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to perform software reset: %d", ret);
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for reset to complete

    // Set internal trigger mode (clear reset bit, set to internal trigger)
    logger_->LOGI(TAG, "Setting MODE to internal trigger (0x00)");
    ret = writeReg(DRV2605_REG_MODE, DRV2605_MODE_INTTRIG);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set MODE: %d", ret);
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for mode change to take effect

    // Disable real-time playback
    ret = writeReg(DRV2605_REG_RTPIN, 0x00);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set RTPIN: %d", ret);
        return false;
    }

    // Set waveform sequence (will be updated per waveform)
    ret = writeReg(DRV2605_REG_WAVESEQ1, 0);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set WAVESEQ1: %d", ret);
        return false;
    }

    ret = writeReg(DRV2605_REG_WAVESEQ2, 0);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set WAVESEQ2: %d", ret);
        return false;
    }

    // Disable overdrive and sustain
    ret = writeReg(DRV2605_REG_OVERDRIVE, 0);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set OVERDRIVE: %d", ret);
        return false;
    }

    ret = writeReg(DRV2605_REG_SUSTAINPOS, 0);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set SUSTAINPOS: %d", ret);
        return false;
    }

    ret = writeReg(DRV2605_REG_SUSTAINNEG, 0);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set SUSTAINNEG: %d", ret);
        return false;
    }

    ret = writeReg(DRV2605_REG_BREAK, 0);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set BREAK: %d", ret);
        return false;
    }

    // Set audio mode default
    ret = writeReg(DRV2605_REG_AUDIOMAX, 0x64);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set AUDIOMAX: %d", ret);
        return false;
    }

    // Set waveform library (ERM, TS2200 library A)
    ret = writeReg(DRV2605_REG_LIBRARY, 1);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set LIBRARY: %d", ret);
        return false;
    }

    // Set ERM mode
    ret = writeReg(DRV2605_REG_FEEDBACK, 0x00);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set FEEDBACK: %d", ret);
        return false;
    }

    // Set open-loop mode
    ret = writeReg(DRV2605_REG_CONTROL3, 0x20);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set CONTROL3: %d", ret);
        return false;
    }

    // Configure control registers
    ret = writeReg(DRV2605_REG_CONTROL1, 0x00);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set CONTROL1: %d", ret);
        return false;
    }

    ret = writeReg(DRV2605_REG_CONTROL2, 0x50);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set CONTROL2: %d", ret);
        return false;
    }

    // Create FreeRTOS queue for waveforms
    waveform_queue_ = xQueueCreate(WAVEFORM_QUEUE_SIZE, sizeof(uint8_t));
    if (waveform_queue_ == nullptr) {
        logger_->LOGE(TAG, "Failed to create waveform queue");
        return false;
    }

    // Start processing task
    running_.store(true, std::memory_order_release);
    BaseType_t task_ret = xTaskCreate(
        taskFunction,
        "HapticsTask",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &task_handle_
    );

    if (task_ret != pdPASS) {
        logger_->LOGE(TAG, "Failed to create haptics task");
        vQueueDelete(waveform_queue_);
        waveform_queue_ = nullptr;
        return false;
    }

    // Enter standby mode if power saving is enabled
    if (power_saving_) {
        logger_->LOGI(TAG, "Entering power-saving mode (standby)");
        if (!enablePowerSaving()) {
            logger_->LOGE(TAG, "Failed to enter initial power-saving mode");
            // Don't fail init if power saving fails, just log it
        }
    }

    initialized_ = true;
    logger_->LOGI(TAG, "DRV2605L initialized successfully");
    return true;
}

bool Haptics::queueWaveform(uint8_t waveform) {
    if (!initialized_ || waveform_queue_ == nullptr) {
        logger_->LOGE(TAG, "Haptics not initialized");
        return false;
    }

    if (waveform > 123) {
        logger_->LOGE(TAG, "Invalid waveform ID: %d", waveform);
        return false;
    }

    // Try to send to queue (non-blocking)
    BaseType_t ret = xQueueSend(waveform_queue_, &waveform, 0);
    if (ret != pdTRUE) {
        logger_->LOGW(TAG, "Waveform queue full, dropping waveform %d", waveform);
        return false;
    }

    // Notify task to wake up and process queue
    if (task_handle_ != nullptr) {
        xTaskNotify(task_handle_, 0, eNoAction);
    }

    return true;
}

void Haptics::stop() {
    if (!initialized_) {
        return;
    }

    // Clear GO bit to stop waveform playback
    writeReg(DRV2605_REG_GO, 0);
    is_playing_.store(false, std::memory_order_release);
    
    // Clear queue
    if (waveform_queue_ != nullptr) {
        xQueueReset(waveform_queue_);
    }

    // Enter standby mode if power_saving is enabled
    if (power_saving_) {
        enablePowerSaving();
    }
}

bool Haptics::enablePowerSaving() {
    if (!initialized_) {
        return false;
    }

    // First, stop any ongoing playback
    writeReg(DRV2605_REG_GO, 0);
    is_playing_.store(false, std::memory_order_release);
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for GO bit to clear
    
    int ret = writeReg(DRV2605_REG_MODE, DRV2605_MODE_STANDBY);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to write standby mode to MODE register: %d", ret);
        return false;
    }
    
    // Wait for mode change to take effect
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Verify the write by reading back
    uint8_t mode_read;
    ret = readReg(DRV2605_REG_MODE, &mode_read);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to read MODE register for verification: %d", ret);
        return false;
    }
    
    // Check if standby bit is set
    if ((mode_read & 0x40) == 0) {
        logger_->LOGE(TAG, "MODE register not in standby after write (read: 0x%02X, expected bit 6 set)", mode_read);
        return false;
    }
    
    logger_->LOGI(TAG, "Power-saving enabled (standby, MODE=0x%02X)", mode_read);
    return true;
}

bool Haptics::disablePowerSaving() {
    if (!initialized_) {
        return false;
    }

    int ret = writeReg(DRV2605_REG_MODE, DRV2605_MODE_INTTRIG);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to disable power-saving mode: %d", ret);
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(5)); // Small delay for mode change to take effect
    logger_->LOGI(TAG, "Power-saving disabled (active)");
    return true;
}

bool Haptics::isPlaying() const {
    return is_playing_.load(std::memory_order_acquire);
}

void Haptics::taskFunction(void* pvParameters) {
    Haptics* haptics = static_cast<Haptics*>(pvParameters);
    if (haptics != nullptr) {
        haptics->taskLoop();
    }
    vTaskDelete(nullptr);
}

void Haptics::taskLoop() {
    uint8_t waveform;
    const TickType_t queue_timeout = pdMS_TO_TICKS(1000); // 1 second timeout

    logger_->LOGI(TAG, "Haptics task started");

    while (running_.load(std::memory_order_acquire)) {
        // Wait for waveform in queue
        if (xQueueReceive(waveform_queue_, &waveform, queue_timeout) == pdTRUE) {
            // Play the waveform
            if (playWaveformInternal(waveform)) {
                // Wait for waveform to complete
                waitForWaveformComplete();
                
                // Enter standby if power saving enabled
                if (power_saving_) {
                    enablePowerSaving();
                }
            }
        }
        
        // Check if we should exit
        if (!running_.load(std::memory_order_acquire)) {
            break;
        }
    }

    logger_->LOGI(TAG, "Haptics task exiting");
}

bool Haptics::playWaveformInternal(uint8_t waveform) {
    if (waveform > 123) {
        logger_->LOGE(TAG, "Invalid waveform ID: %d", waveform);
        return false;
    }

    // Wake up the device from standby mode
    if (!disablePowerSaving()) {
        logger_->LOGE(TAG, "Failed to wake up DRV2605 for waveform %d", waveform);
        return false;
    }

    // Small delay to ensure device is ready
    vTaskDelay(pdMS_TO_TICKS(5));

    // Set waveform sequence
    int ret = writeReg(DRV2605_REG_WAVESEQ1, waveform);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set WAVESEQ1: %d", ret);
        return false;
    }

    ret = writeReg(DRV2605_REG_WAVESEQ2, 0); // End of sequence
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to set WAVESEQ2: %d", ret);
        return false;
    }

    // Small delay to ensure registers are set
    vTaskDelay(pdMS_TO_TICKS(1));

    // Trigger playback with GO bit
    ret = writeReg(DRV2605_REG_GO, 1);
    if (ret != 0) {
        logger_->LOGE(TAG, "Failed to trigger waveform: %d", ret);
        return false;
    }

    is_playing_.store(true, std::memory_order_release);
    logger_->LOGI(TAG, "Waveform %d playback triggered", waveform);
    return true;
}

bool Haptics::waitForWaveformComplete(uint32_t timeout_ms) {
    const TickType_t start_time = xTaskGetTickCount();
    const TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    
    while ((xTaskGetTickCount() - start_time) < timeout_ticks) {
        uint8_t go_reg;
        int ret = readReg(DRV2605_REG_GO, &go_reg);
        if (ret == 0) {
            // GO bit clears when waveform completes
            if ((go_reg & 0x01) == 0) {
                is_playing_.store(false, std::memory_order_release);
                logger_->LOGI(TAG, "Waveform completed");
                return true;
            }
        }
        
        // Check if we should exit
        if (!running_.load(std::memory_order_acquire)) {
            break;
        }
        
        // Small delay before next check
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Timeout or stopped
    is_playing_.store(false, std::memory_order_release);
    logger_->LOGW(TAG, "Waveform wait timeout or stopped");
    return false;
}

int Haptics::writeReg(uint8_t reg, uint8_t value) {
    int ret = i2c_->writeRegister(DRV2605_ADDR, reg, value);
    if (ret != 0) {
        logger_->LOGD(TAG, "Write to reg 0x%02X failed: %d", reg, ret);
    }
    return ret;
}

int Haptics::readReg(uint8_t reg, uint8_t* value) {
    const int max_retries = 3;
    int ret = -1;

    for (int i = 0; i < max_retries; i++) {
        ret = i2c_->readRegister(DRV2605_ADDR, reg, value);
        if (ret == 0) {
            if (i > 0) {
                logger_->LOGI(TAG, "Read from reg 0x%02X succeeded after %d retries", reg, i + 1);
            }
            return 0;
        }
        logger_->LOGD(TAG, "Read from reg 0x%02X failed (attempt %d/%d): %d", reg, i + 1, max_retries, ret);
        vTaskDelay(pdMS_TO_TICKS(10)); // Wait 10ms before retrying
    }

    logger_->LOGE(TAG, "Read from reg 0x%02X failed after %d retries: %d", reg, max_retries, ret);
    return ret;
}
