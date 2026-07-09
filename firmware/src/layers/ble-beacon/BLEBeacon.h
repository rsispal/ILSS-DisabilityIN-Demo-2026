#ifndef LAYERS_BLE_BEACON_BLEBEACON_H
#define LAYERS_BLE_BEACON_BLEBEACON_H

#include <cstdint>
#include <cstring>
#include <cstdio>
#include "../../utils/Utils.h"

enum class SupportedBeaconType
{
    UNKNOWN,
    HONEYWELL_SELF_TEST,
};

class BLEBeacon
{
public:
    BLEBeacon();
    BLEBeacon(const BLEBeacon &other); // Copy constructor
    ~BLEBeacon();

    // Parse beacon data and determine type
    void parse(const uint8_t *data, size_t length, uint16_t manufacturerId, int8_t rssi);

    // Check if this is a Honeywell Self Test Sensor
    bool isHoneywellSelfTestSensor() const;

    // Get the identifier (serial number for Honeywell)
    uint64_t getIdentifier() const;

    // Get the beacon type
    SupportedBeaconType getType() const;

    // Time tracking methods
    uint32_t getLastSeenTimestamp() const;
    uint32_t getTimeSinceLastSeen() const;

    // Update RSSI and timestamp
    void updateRssi(int8_t newRssi);

    // Get RSSI
    int8_t getRssi() const;

    // Get manufacturer data for comparison
    const uint8_t *getManufacturerData() const;
    size_t getManufacturerDataLength() const;

private:
    // Parse Honeywell Self Test Sensor data
    void parseHoneywellSelfTestSensor(const uint8_t *data, size_t length);

    // Check if data matches Honeywell Self Test Sensor format
    static bool isHoneywellSelfTestSensor(const uint8_t *data, size_t length, uint16_t manufacturerId);

    // Constants for Honeywell Self Test Sensor
    static constexpr size_t HONEYWELL_EXPECTED_LENGTH = 24;
    static constexpr uint16_t HONEYWELL_COMPANY_ID = 0x0526;
    static constexpr uint8_t HONEYWELL_SELF_TEST_STATUS = 0x00;

    // Beacon data
    SupportedBeaconType beaconType;
    uint64_t identifier; // Serial number for Honeywell
    int8_t rssi;
    uint32_t lastSeenTimestamp;

    // Raw manufacturer data for comparison
    uint8_t manufacturerData[32]; // Buffer for manufacturer data
    size_t manufacturerDataLength;
    uint16_t manufacturerId;
};

#endif // LAYERS_BLE_BEACON_BLEBEACON_H