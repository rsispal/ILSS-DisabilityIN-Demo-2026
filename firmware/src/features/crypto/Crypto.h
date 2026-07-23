#pragma once

#include "../../utils/Logger.h"
#include "../../state/State.h"
#include "../../lowlevel/LowLevel.h"
#include "../../lowlevel/crypto/ATECC608BDriver.h"
#include <cstdint>

/**
 * Crypto — feature wrapper over ATECC608B.
 *
 * Not yet wired into DigitalTwinApplication; USB CLI diagnostics construct
 * this for bring-up. Future: device identity, pairing attestations, etc.
 */
class Crypto {
    const char* TAG = "Crypto";

public:
    Crypto(State* state, LowLevel* lowLevel);
    ~Crypto() = default;

    bool begin();
    bool isReady() const;

    bool wake();
    bool idle();
    bool sleep();

    bool readRevision(uint8_t out[ATECC608BDriver::kRevisionLen]);
    bool readSerial(uint8_t out[ATECC608BDriver::kSerialLen]);
    bool randomBytes(uint8_t out[ATECC608BDriver::kRandomLen]);

    static const char* revisionName(const uint8_t rev[ATECC608BDriver::kRevisionLen]);

private:
    State* state_ = nullptr;
    LowLevel* lowLevel_ = nullptr;
    Logger logger_;
    bool initialized_ = false;
};
