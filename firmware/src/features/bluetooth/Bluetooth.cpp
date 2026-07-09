#include "Bluetooth.h"
#include "../../layers/ble-beacon/BLEBeacon.h"
#include "../../lowlevel/bluetooth/BluetoothLowLevelDriver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdlib>
#include <algorithm>

// RSSI thresholds
static constexpr int8_t RSSI_CHANGE_THRESHOLD_DB = 5;  // Minimum RSSI change to log (in dB)
static constexpr int8_t MIN_RSSI_THRESHOLD_DB = -68;   // Ignore beacons with RSSI < -55 (too weak/far, beyond ~5 metres)

Bluetooth::Bluetooth(State *state, LowLevel *lowLevel)
    : state(state), lowLevel(lowLevel), m_initialized(false), currentScanInterval(0), currentScanWindow(0)
{
}

Bluetooth::~Bluetooth()
{
    if (isBeaconScanning())
    {
        stopBeaconScanning();
    }
}

bool Bluetooth::begin()
{
    if (m_initialized) {
        logger.LOGW(TAG, "Bluetooth already initialized");
        return true;
    }
    
    logger.LOGI(TAG, "Initialising Bluetooth");
    
    if (!lowLevel)
    {
        logger.LOGE(TAG, "Bluetooth driver not initialised");
        return false;
    }

    // Configure callbacks
    lowLevel->get_bluetooth().setBeaconDiscoveredCallback(
        [this](const BLEBeacon &beacon)
        { this->onBeaconDiscovered(beacon); });
    lowLevel->get_bluetooth().setScanStartedCallback(
        [this]()
        { this->onScanStarted(); });
    lowLevel->get_bluetooth().setScanStoppedCallback(
        [this]()
        { this->onScanStopped(); });
    lowLevel->get_bluetooth().setScanErrorCallback(
        [this](int error)
        { this->onScanError(error); });

    // Configure Honeywell beacon filter
    configureHoneywellBeaconFilter();

    m_initialized = true;
    logger.LOGI(TAG, "Bluetooth initialised");
    return true;
}

bool Bluetooth::isReady() const
{
    return m_initialized && lowLevel != nullptr;
}

void Bluetooth::stop()
{
    if (isBeaconScanning())
    {
        stopBeaconScanning();
    }
    logger.LOGI(TAG, "Stopping Bluetooth");
}

bool Bluetooth::startBeaconScanning(uint16_t scanInterval, uint16_t scanWindow)
{
    if (!lowLevel)
    {
        logger.LOGE(TAG, "Bluetooth driver not available");
        return false;
    }

    // Use stored scan interval/window if parameters are 0 (default)
    if (scanInterval == 0 && currentScanInterval > 0) {
        scanInterval = currentScanInterval;
    }
    if (scanWindow == 0 && currentScanWindow > 0) {
        scanWindow = currentScanWindow;
    }
    // If scanWindow is still 0, use half of scanInterval
    if (scanWindow == 0 && scanInterval > 0) {
        scanWindow = scanInterval / 2;
    }

    // Directly start scanning with the provided parameters
    bool success = lowLevel->get_bluetooth().startScanning(scanInterval, scanWindow);
    if (success)
    {
        logger.LOGI(TAG, "Beacon scanning started (interval: %d ms, window: %d ms)", scanInterval, scanWindow);
    } else {
        logger.LOGE(TAG, "Failed to start beacon scanning (interval: %d ms, window: %d ms)", scanInterval, scanWindow);
    }
    return success;
}

bool Bluetooth::stopBeaconScanning()
{
    if (!lowLevel)
    {
        return false;
    }

    logger.LOGI(TAG, "Stopping beacon scanning");

    // Directly stop the low-level scanning
    return lowLevel->get_bluetooth().stopScanning();
}

bool Bluetooth::isBeaconScanning() const
{
    if (!lowLevel)
    {
        return false;
    }
    return lowLevel->get_bluetooth().isScanning();
}

void Bluetooth::removeStaleBeacons()
{
    if (state)
    {
        state->removeStaleBeacons();
    }
}

void Bluetooth::clearBeacons()
{
    if (state)
    {
        state->clearBeacons();
    }
}

const std::vector<BLEBeacon> &Bluetooth::getBeacons() const
{
    if (state)
    {
        return state->getBeacons();
    }
    static const std::vector<BLEBeacon> empty;
    return empty;
}

BLEBeacon *Bluetooth::getBestBeacon()
{
    if (state)
    {
        return state->getBestBeacon();
    }
    return nullptr;
}

void Bluetooth::configureHoneywellBeaconFilter()
{
    if (!lowLevel)
    {
        logger.LOGE(TAG, "Bluetooth driver not available for filter configuration");
        return;
    }

    // Add Honeywell manufacturer ID filter (0x0526)
    lowLevel->get_bluetooth().addManufacturerFilter(0x0526);
    logger.LOGI(TAG, "Configured Honeywell beacon filter");
}

void Bluetooth::setScanInterval(uint16_t scanInterval, uint16_t scanWindow)
{
    currentScanInterval = scanInterval;
    currentScanWindow = scanWindow ? scanWindow : 0;

    // If currently scanning, restart with new parameters
    if (isBeaconScanning())
    {
        stopBeaconScanning();
        vTaskDelay(pdMS_TO_TICKS(100)); // Brief delay
        startBeaconScanning(scanInterval, scanWindow);
    }
}

void Bluetooth::onBeaconDiscovered(const BLEBeacon &beacon)
{
    // Ignore beacons with RSSI < -65 (too weak/far, beyond ~5 metres)
    // RSSI values are negative: closer to 0 = stronger signal = closer device
    if (beacon.getRssi() < MIN_RSSI_THRESHOLD_DB)
    {
        return;
    }

    if (state)
    {
        // Check if this is a new beacon or significant update before adding to state
        bool isNewBeacon = true;
        int8_t oldRssi = 0;

        // Look for existing beacon to determine if this is new or an update
        const std::vector<BLEBeacon> &existingBeacons = state->getBeacons();
        for (const auto &existingBeacon : existingBeacons)
        {
            if (existingBeacon.getIdentifier() == beacon.getIdentifier())
            {
                isNewBeacon = false;
                oldRssi = existingBeacon.getRssi();
                break;
            }
        }

        // Only log if it's a new beacon or significant RSSI change
        if (isNewBeacon)
        {
            const uint8_t *mfg_data = beacon.getManufacturerData();
            size_t mfg_len = beacon.getManufacturerDataLength();

            char hex_buf[128];
            size_t hex_pos = 0;
            for (size_t i = 0; i < mfg_len && hex_pos < 127; ++i)
            {
                hex_pos += snprintf(hex_buf + hex_pos, 127 - hex_pos, "%02X", mfg_data[i]);
                if (i < mfg_len - 1 && hex_pos < 127)
                {
                    hex_pos += snprintf(hex_buf + hex_pos, 127 - hex_pos, " ");
                }
            }

            logger.LOGD(TAG, "Beacon data [id=%llu]: %s", beacon.getIdentifier(), hex_buf);
            logger.LOGI(TAG, "New beacon: id=%llu rssi:%d", beacon.getIdentifier(), beacon.getRssi());
        }
        else
        {
            // Only log significant RSSI changes to reduce verbosity
            int8_t rssiChange = beacon.getRssi() - oldRssi;
            if (abs(rssiChange) > RSSI_CHANGE_THRESHOLD_DB)
            {
                logger.LOGI(TAG, "RSSI change: id=%llu %d->%d (%+d)",
                             beacon.getIdentifier(), oldRssi, beacon.getRssi(), rssiChange);
            }
        }

        // Add/update in state (this will handle the internal logging)
        state->addOrUpdateBeacon(beacon);
    }
}

void Bluetooth::onScanStarted()
{
    logger.LOGD(TAG, "Beacon scanning started");
}

void Bluetooth::onScanStopped()
{
    logger.LOGD(TAG, "Beacon scanning stopped");
    
    // Call scan complete callback if set (for event-driven architecture)
    if (scanCompleteCallback) {
        scanCompleteCallback();
    }
}

void Bluetooth::setScanCompleteCallback(std::function<void()> callback)
{
    scanCompleteCallback = callback;
}

void Bluetooth::onScanError(int error)
{
    logger.LOGE(TAG, "Beacon scanning error: %d", error);
}
