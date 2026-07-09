#include "Utils.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>

uint32_t Utils::getCurrentTimestamp() {
    return (uint32_t)(esp_timer_get_time() / 1000); // Convert microseconds to milliseconds
}

void Utils::rawUuidToString(const uint8_t* rawUuid, char* uuidString, size_t uuidStringSize) {
    if (uuidStringSize < 37) { // UUID string needs 36 chars + null terminator
        uuidString[0] = '\0';
        return;
    }

    // Convert raw bytes to UUID string format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    snprintf(uuidString, uuidStringSize,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             rawUuid[0], rawUuid[1], rawUuid[2], rawUuid[3],
             rawUuid[4], rawUuid[5],
             rawUuid[6], rawUuid[7],
             rawUuid[8], rawUuid[9],
             rawUuid[10], rawUuid[11], rawUuid[12], rawUuid[13], rawUuid[14], rawUuid[15]);
}

bool Utils::stringToRawUuid(const char* uuidString, uint8_t* rawUuid, size_t rawUuidSize) {
    if (rawUuidSize < 16) {
        return false;
    }

    // Expected format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    if (strlen(uuidString) != 36) {
        return false;
    }

    // Parse the UUID string, skipping hyphens
    const char* ptr = uuidString;
    for (int i = 0; i < 16; i++) {
        // Skip hyphens at positions 8, 13, 18, 23 in the UUID string
        // This corresponds to after bytes 3, 5, 7, 9
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            if (*ptr != '-') {
                return false;
            }
            ptr++;
        }

        // Parse two hex characters
        unsigned int temp;
        if (sscanf(ptr, "%2x", &temp) != 1) {
            return false;
        }
        rawUuid[i] = (uint8_t)temp;
        ptr += 2;
    }

    return true;
}

uint64_t Utils::make64BitSerialNumber(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4) {
    return ((uint64_t)b0 << 32) |
           ((uint64_t)b1 << 24) |
           ((uint64_t)b2 << 16) |
           ((uint64_t)b3 << 8) |
           (uint64_t)b4;
}

uint8_t Utils::computeCheckDigit(uint64_t serial) {
    int sum = 0;
    int pos = 0;
    while (serial > 0) {
        int digit = serial % 10;
        sum += (pos % 2 == 0) ? digit * 3 : digit;
        serial /= 10;
        ++pos;
    }
    int mod = sum % 10;
    return (mod == 0) ? 0 : (uint8_t)(10 - mod);
}

