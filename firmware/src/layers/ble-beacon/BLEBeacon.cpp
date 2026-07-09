#include "BLEBeacon.h"
#include "../../utils/Utils.h"
#include <cstring>
#include <cstdio>

BLEBeacon::BLEBeacon()
    : beaconType(SupportedBeaconType::UNKNOWN), identifier(0), rssi(0), lastSeenTimestamp(0), manufacturerDataLength(0), manufacturerId(0)
{
    memset(manufacturerData, 0, sizeof(manufacturerData));
}

BLEBeacon::BLEBeacon(const BLEBeacon &other)
    : beaconType(other.beaconType), identifier(other.identifier), rssi(other.rssi),
      lastSeenTimestamp(other.lastSeenTimestamp), manufacturerDataLength(other.manufacturerDataLength),
      manufacturerId(other.manufacturerId)
{
    memcpy(manufacturerData, other.manufacturerData, sizeof(manufacturerData));
}

BLEBeacon::~BLEBeacon()
{
}

void BLEBeacon::parse(const uint8_t *data, size_t length, uint16_t manufacturerId, int8_t rssi)
{
    this->manufacturerId = manufacturerId;
    this->rssi = rssi;
    this->lastSeenTimestamp = Utils::getCurrentTimestamp();

    // Store manufacturer data for comparison
    if (length <= sizeof(manufacturerData))
    {
        memcpy(manufacturerData, data, length);
        manufacturerDataLength = length;
    }

    // Reset identifier
    identifier = 0;

    // Check beacon type based on manufacturer data
    if (isHoneywellSelfTestSensor(data, length, manufacturerId))
    {
        beaconType = SupportedBeaconType::HONEYWELL_SELF_TEST;
        parseHoneywellSelfTestSensor(data, length);
    }
    else
    {
        beaconType = SupportedBeaconType::UNKNOWN;
    }
}

bool BLEBeacon::isHoneywellSelfTestSensor() const
{
    return beaconType == SupportedBeaconType::HONEYWELL_SELF_TEST;
}

uint64_t BLEBeacon::getIdentifier() const
{
    return identifier;
}

SupportedBeaconType BLEBeacon::getType() const
{
    return beaconType;
}

uint32_t BLEBeacon::getLastSeenTimestamp() const
{
    return lastSeenTimestamp;
}

uint32_t BLEBeacon::getTimeSinceLastSeen() const
{
    return Utils::getCurrentTimestamp() - lastSeenTimestamp;
}

void BLEBeacon::updateRssi(int8_t newRssi)
{
    rssi = newRssi;
    lastSeenTimestamp = Utils::getCurrentTimestamp();
}

int8_t BLEBeacon::getRssi() const
{
    return rssi;
}

const uint8_t *BLEBeacon::getManufacturerData() const
{
    return manufacturerData;
}

size_t BLEBeacon::getManufacturerDataLength() const
{
    return manufacturerDataLength;
}

void BLEBeacon::parseHoneywellSelfTestSensor(const uint8_t *data, size_t length)
{
    if (length != HONEYWELL_EXPECTED_LENGTH)
    {
        return;
    }

    // Check beacon code (bytes 0-1)
    if (data[0] != 0xBE || data[1] != 0xAC)
    {
        return;
    }

    // Extract serial number from bytes 18-21 (4 bytes)
    // Format: 4 hex bytes representing the serial number (prefixed with 0x05 though for Self Test)
    // Combine the 5 bytes into a uint64_t, starting from the end

    uint64_t serialNumber = Utils::make64BitSerialNumber(0x05, data[18], data[19], data[20], data[21]);
    uint8_t checkDigit = Utils::computeCheckDigit(serialNumber);

    // Store the final serial number with check digit as identifier
    identifier = serialNumber * 10 + checkDigit;
}

bool BLEBeacon::isHoneywellSelfTestSensor(const uint8_t *data, size_t length, uint16_t manufacturerId)
{
    if (length != HONEYWELL_EXPECTED_LENGTH)
    {
        return false;
    }

    if (manufacturerId != HONEYWELL_COMPANY_ID)
    {
        return false;
    }

    if (data[0] != 0xBE || data[1] != 0xAC)
    {
        return false;
    }

    return true;
}
