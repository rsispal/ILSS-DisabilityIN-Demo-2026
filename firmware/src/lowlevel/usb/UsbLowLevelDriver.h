#pragma once

#include <string>
#include <cstdint>
#include <cstddef>

// Forward declaration
class Logger;

/**
 * UsbLowLevelDriver - Low-level USB serial driver
 * 
 * Handles USB serial communication using ESP-IDF's usb_serial_jtag driver.
 * Provides basic read/write operations for USB CDC functionality.
 */
class UsbLowLevelDriver {
    const char* TAG = "UsbLowLevelDriver";

public:
    UsbLowLevelDriver(Logger* logger);
    ~UsbLowLevelDriver();

    // Initialize USB serial driver
    bool begin();

    // Check if USB is connected
    bool isConnected() const { return initialized_; }
    
    // Check if USB driver is ready (initialized)
    bool isReady() const { return initialized_; }

    // Write string data (no newline)
    bool write(const std::string& data, uint32_t timeout_ms = 100);

    // Write string data with newline
    bool writeLine(const std::string& data, uint32_t timeout_ms = 100);

    // Read string data until newline or timeout
    bool readLine(std::string& data, uint32_t timeout_ms = 1000);

    // Raw byte operations
    bool writeBytes(const uint8_t* data, size_t length, uint32_t timeout_ms = 100);
    bool readBytes(uint8_t* data, size_t max_length, size_t& received_length, uint32_t timeout_ms = 1000);

    // Read a single byte
    int read(uint8_t* out, int timeout_ms);

    // Check if data is available to read
    bool hasDataAvailable() const;

    // Flush receive buffer (discard any pending input)
    void flushReceiveBuffer();
    
    // Flush transmit buffer (ensure all pending output is sent)
    void flushTransmitBuffer();
    
    // Write string data and flush transmit buffer
    bool writeAndFlush(const std::string& data, uint32_t timeout_ms = 100);
    
    // Write string data with newline and flush transmit buffer
    bool writeLineAndFlush(const std::string& data, uint32_t timeout_ms = 100);

private:
    Logger* logger_;
    bool initialized_;
    static constexpr size_t RX_BUFFER_SIZE = 512;
};

