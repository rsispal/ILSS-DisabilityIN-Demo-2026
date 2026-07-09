#pragma once

#include <cstdint>
#include <cstddef>

/**
 * Utils - Static utility functions
 * 
 * Common utility functions adapted for ESP-IDF.
 */
class Utils {
public:
    // Get current timestamp in milliseconds (ESP-IDF equivalent of k_uptime_get_32)
    static uint32_t getCurrentTimestamp();

    // UUID conversion utilities
    static void rawUuidToString(const uint8_t* rawUuid, char* uuidString, size_t uuidStringSize);
    static bool stringToRawUuid(const char* uuidString, uint8_t* rawUuid, size_t rawUuidSize);

    // Serial number utilities
    static uint64_t make64BitSerialNumber(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4);
    static uint8_t computeCheckDigit(uint64_t serial);
};

