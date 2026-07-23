#pragma once

#include "../i2c/I2CLowLevelDriver.h"
#include "../../utils/Logger.h"
#include <cstdint>

/**
 * Minimal ATECC608B (CryptoAuthentication) driver over I2C.
 *
 * Diagnostics-focused: wake / idle / sleep, Info revision, config serial,
 * and Random. Not a full CryptoAuthLib port — no slot provisioning yet.
 */
class ATECC608BDriver {
    const char* TAG = "ATECC608B";

public:
    static constexpr size_t kSerialLen = 9;
    static constexpr size_t kRevisionLen = 4;
    static constexpr size_t kRandomLen = 32;

    explicit ATECC608BDriver(I2CLowLevelDriver* i2c);
    ~ATECC608BDriver() = default;

    bool begin();
    bool isReady() const { return m_initialized; }

    bool wake();
    bool idle();
    bool sleep();

    /** Info command mode 0 — hardware revision (ATECC608B ≈ 00 00 60 03). */
    bool readRevision(uint8_t out[kRevisionLen]);

    /** 9-byte unique serial from config zone block 0. */
    bool readSerial(uint8_t out[kSerialLen]);

    /** True RNG — 32 bytes. */
    bool random(uint8_t out[kRandomLen]);

    /** Best-effort product name from Info revision bytes. */
    static const char* revisionName(const uint8_t rev[kRevisionLen]);

private:
    I2CLowLevelDriver* m_i2c = nullptr;
    Logger logger;
    bool m_initialized = false;

    static uint16_t crc16(const uint8_t* data, size_t len);
    bool sendCommand(uint8_t opcode, uint8_t param1, uint16_t param2,
                     const uint8_t* data, size_t data_len,
                     uint8_t* response, size_t response_len,
                     uint32_t exec_ms);
    bool readResponse(uint8_t* response, size_t response_len, uint32_t timeout_ms);
    bool pollWakeResponse(uint8_t resp[4], int retries);
    bool wakeI2cToken();
};
