#pragma once

#include <functional>
#include <set>
#include <vector>
#include "../../state/State.h"
#include "../../utils/Logger.h"
#include "../../lowlevel/LowLevel.h"

// Forward declaration
class BLEBeacon;

/**
 * Bluetooth - High-level Bluetooth feature management
 * 
 * Manages BLE beacon scanning, filtering, and beacon state tracking.
 */
class Bluetooth
{
    const char *TAG = "Bluetooth";

public:
    Bluetooth(State *state, LowLevel *lowLevel);
    ~Bluetooth();
    
    bool begin();
    void stop();
    
    // Check if Bluetooth is ready
    bool isReady() const;
    
    bool startBeaconScanning(uint16_t scanInterval = 0, uint16_t scanWindow = 0);
    bool stopBeaconScanning();
    bool isBeaconScanning() const;
    
    void removeStaleBeacons();
    void clearBeacons();
    const std::vector<BLEBeacon> &getBeacons() const;
    BLEBeacon *getBestBeacon();
    
    void configureHoneywellBeaconFilter();
    void setScanInterval(uint16_t scanInterval, uint16_t scanWindow = 0);
    
    // Set callback for when scan completes (for event-driven architecture)
    void setScanCompleteCallback(std::function<void()> callback);

private:
    Logger logger;
    State *state;
    LowLevel *lowLevel;
    bool m_initialized;

    uint16_t currentScanInterval;
    uint16_t currentScanWindow;
    
    // Scan complete callback (for event-driven architecture)
    std::function<void()> scanCompleteCallback;
    
    void onBeaconDiscovered(const BLEBeacon &beacon);
    void onScanStarted();
    void onScanStopped();
    void onScanError(int error);
};
