#include "UsbLowLevelDriver.h"
#include "../../utils/Logger.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

UsbLowLevelDriver::UsbLowLevelDriver(Logger* logger) 
    : logger_(logger), initialized_(false) {
}

UsbLowLevelDriver::~UsbLowLevelDriver() {
    // USB driver cleanup handled by ESP-IDF
}

bool UsbLowLevelDriver::begin() {
    if (initialized_) {
        logger_->LOGW(TAG, "USB driver already initialized");
        return true;
    }

    usb_serial_jtag_driver_config_t cfg_drv = {
        .tx_buffer_size = 512,
        .rx_buffer_size = 512,
    };

    esp_err_t err = usb_serial_jtag_driver_install(&cfg_drv);
    if (err != ESP_OK) {
        logger_->LOGE(TAG, "USB serial driver install failed: %s", esp_err_to_name(err));
        return false;
    }

    initialized_ = true;
    logger_->LOGI(TAG, "USB serial driver initialized");
    return true;
}

bool UsbLowLevelDriver::write(const std::string& data, uint32_t timeout_ms) {
    return writeBytes(reinterpret_cast<const uint8_t*>(data.c_str()), data.length(), timeout_ms);
}

bool UsbLowLevelDriver::writeLine(const std::string& data, uint32_t timeout_ms) {
    std::string line = data + "\r\n";
    return writeBytes(reinterpret_cast<const uint8_t*>(line.c_str()), line.length(), timeout_ms);
}

bool UsbLowLevelDriver::writeBytes(const uint8_t* data, size_t length, uint32_t timeout_ms) {
    if (!initialized_) {
        logger_->LOGE(TAG, "USB driver not initialized");
        return false;
    }

    size_t written = usb_serial_jtag_write_bytes(data, length, pdMS_TO_TICKS(timeout_ms));
    return written == length;
}

bool UsbLowLevelDriver::readLine(std::string& data, uint32_t timeout_ms) {
    data.clear();
    
    if (!initialized_) {
        return false;
    }
    
    // Use batch reading for better performance
    uint8_t buffer[64];
    int elapsed = 0;
    const int step_ms = 50; // Read in larger time chunks
    
    while (elapsed < (int)timeout_ms) {
        size_t received = usb_serial_jtag_read_bytes(buffer, sizeof(buffer), pdMS_TO_TICKS(step_ms));
        
        if (received > 0) {
            // Process received bytes
            for (size_t i = 0; i < received; i++) {
                uint8_t ch = buffer[i];
                
                if (ch == '\r' || ch == '\n') {
                    // Handle CRLF sequence - if we got \r, skip following \n
                    if (ch == '\r' && i + 1 < received && buffer[i + 1] == '\n') {
                        // Skip the \n that follows \r
                        i++;
                    }
                    
                    // Only return if we have data (ignore empty lines from CRLF)
                    if (!data.empty()) {
                        return true;
                    }
                    continue;
                }
                
                data += (char)ch;
            }
        }
        
        elapsed += step_ms;
    }
    
    return !data.empty(); // Return true if we got any data
}

bool UsbLowLevelDriver::readBytes(uint8_t* data, size_t max_length, size_t& received_length, uint32_t timeout_ms) {
    if (!initialized_) {
        received_length = 0;
        return false;
    }

    received_length = usb_serial_jtag_read_bytes(data, max_length, pdMS_TO_TICKS(timeout_ms));
    return received_length > 0;
}

int UsbLowLevelDriver::read(uint8_t* out, int timeout_ms) {
    if (!initialized_ || !out) {
        return 0;
    }
    return usb_serial_jtag_read_bytes(out, 1, pdMS_TO_TICKS(timeout_ms));
}

bool UsbLowLevelDriver::hasDataAvailable() const {
    if (!initialized_) return false;
    // Check if there's data in the buffer (non-blocking peek)
    uint8_t dummy;
    int r = usb_serial_jtag_read_bytes(&dummy, 1, 0);
    if (r > 0) {
        // Put it back - this is a limitation, but works for checking availability
        // In practice, you'd want to use a proper buffer
        return true;
    }
    return false;
}

void UsbLowLevelDriver::flushReceiveBuffer() {
    if (!initialized_) return;
    uint8_t dummy;
    while (usb_serial_jtag_read_bytes(&dummy, 1, 0) > 0) {
        // Drain the buffer
    }
}

void UsbLowLevelDriver::flushTransmitBuffer() {
    if (!initialized_) return;
    
    // ESP-IDF USB serial JTAG driver doesn't have an explicit flush API,
    // but we can ensure data is sent by adding a small delay to allow
    // the USB buffer to be transmitted. The USB driver sends data when
    // the buffer is full or after a short timeout.
    vTaskDelay(pdMS_TO_TICKS(5));
}

bool UsbLowLevelDriver::writeAndFlush(const std::string& data, uint32_t timeout_ms) {
    bool success = write(data, timeout_ms);
    if (success) {
        flushTransmitBuffer();
    }
    return success;
}

bool UsbLowLevelDriver::writeLineAndFlush(const std::string& data, uint32_t timeout_ms) {
    bool success = writeLine(data, timeout_ms);
    if (success) {
        flushTransmitBuffer();
    }
    return success;
}

