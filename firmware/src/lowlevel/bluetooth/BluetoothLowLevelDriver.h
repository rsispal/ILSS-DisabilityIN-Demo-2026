#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

// Forward declarations
class Logger;
class BLEBeacon;

/**
 * BluetoothLowLevelDriver - Low-level BLE driver using NimBLE
 * 
 * Provides BLE scanning functionality for beacon detection.
 * Uses ESP32's NimBLE stack (lighter than Bluedroid, BLE-only).
 */
class BluetoothLowLevelDriver {
    const char* TAG = "BluetoothLowLevel";

public:
    // Callback types for beacon discovery
    using BeaconDiscoveredCallback = std::function<void(const BLEBeacon &beacon)>;
    using ScanStartedCallback = std::function<void()>;
    using ScanStoppedCallback = std::function<void()>;
    using ScanErrorCallback = std::function<void(int error)>;

    BluetoothLowLevelDriver(Logger* logger);
    ~BluetoothLowLevelDriver();

    // Initialize BLE subsystem (lazy - only initializes on first scan)
    bool begin();
    
    // Initialize BT controller early (for coexistence with WiFi)
    // This should be called before WiFi starts
    bool initController();
    
    void stop();

    // Start BLE scanning
    // scanInterval: Scan interval in 0.625ms units (0 = use default 100ms)
    // scanWindow: Scan window in 0.625ms units (0 = use default 50ms)
    bool startScanning(uint16_t scanInterval = 0, uint16_t scanWindow = 0);

    // Stop BLE scanning
    bool stopScanning();

    // Check if currently scanning
    bool isScanning() const { return is_scanning_; }
    
    // Check if Bluetooth driver is ready (initialized)
    bool isReady() const { return initialized_; }

    // Callback registration
    void setBeaconDiscoveredCallback(BeaconDiscoveredCallback callback);
    void setScanStartedCallback(ScanStartedCallback callback);
    void setScanStoppedCallback(ScanStoppedCallback callback);
    void setScanErrorCallback(ScanErrorCallback callback);

    // Filter configuration
    void addManufacturerFilter(uint16_t manufacturerId);
    void clearManufacturerFilters();

private:
    Logger* logger_;
    bool initialized_;
    bool hci_initialized_;  // Track if HCI has been initialized
    bool is_scanning_;
    
    // Callbacks
    BeaconDiscoveredCallback onBeaconDiscovered_;
    ScanStartedCallback onScanStarted_;
    ScanStoppedCallback onScanStopped_;
    ScanErrorCallback onScanError_;
    
    // Manufacturer filters
    std::vector<uint16_t> manufacturerFilters_;
    
    SemaphoreHandle_t sync_sem_;
    
    // Timer for explicit scan window control (stop scan after 5 seconds)
    esp_timer_handle_t scan_window_timer_;
    uint16_t current_scan_interval_ms_;  // Store interval for periodic restarts

    // NimBLE callbacks
    static void onHostSync();
    static int onGapEvent(struct ble_gap_event *event, void *arg);
    static void hostTask(void *param);

    // Helper to parse manufacturer data from advertisement
    static bool parseManufacturerData(const uint8_t* adv_data, size_t adv_len,
                                     uint16_t* manufacturer_id,
                                     const uint8_t** manufacturer_data,
                                     size_t* manufacturer_data_len);
    
    // Process advertisement data and trigger callbacks
    void processAdvertisementData(const uint8_t* address, int8_t rssi,
                                  uint16_t manufacturer_id,
                                  const uint8_t* manufacturer_data,
                                  size_t manufacturer_data_len);
    
    // Check if manufacturer ID should be processed
    bool shouldProcessManufacturerData(uint16_t manufacturerId) const;
    
    // Static instance for callbacks
    static BluetoothLowLevelDriver* s_instance;
};
