#pragma once

// Forward declarations
class Logger;
class WiFiLowLevelDriver;
class UsbLowLevelDriver;
class I2CLowLevelDriver;
class BluetoothLowLevelDriver;
class BuzzerLowLevelDriver;
class DRV2605Driver;
class NVSLowLevelDriver;

/**
 * LowLevel - Container for all low-level hardware drivers
 * 
 * Follows dependency injection pattern - all drivers are created here
 * and passed to higher-level feature classes.
 */
class LowLevel {
    const char* TAG = "LowLevel";

public:
    LowLevel(Logger* logger);
    ~LowLevel();
    
    // Initialize all low-level drivers
    bool begin();

    // Getters for dependency injection (Zephyr-compatible API)
    NVSLowLevelDriver& get_nvs() { return *m_nvs_driver; }
    WiFiLowLevelDriver& get_wifi() { return *m_wifi_driver; }
    UsbLowLevelDriver& get_usb() { return *m_usb_driver; }
    I2CLowLevelDriver& get_i2c() { return *m_i2c_driver; }
    BluetoothLowLevelDriver& get_bluetooth() { return *m_bluetooth_driver; }
    BuzzerLowLevelDriver& get_buzzer() { return *m_buzzer_driver; }
    DRV2605Driver& get_haptics() { return *m_haptics_driver; }
    
    // Legacy public access (for backward compatibility)
    Logger* logger = nullptr;
    
private:
    // Low level drivers
    NVSLowLevelDriver* m_nvs_driver = nullptr;
    WiFiLowLevelDriver* m_wifi_driver = nullptr;
    UsbLowLevelDriver* m_usb_driver = nullptr;
    I2CLowLevelDriver* m_i2c_driver = nullptr;
    BluetoothLowLevelDriver* m_bluetooth_driver = nullptr;
    BuzzerLowLevelDriver* m_buzzer_driver = nullptr;
    DRV2605Driver* m_haptics_driver = nullptr;
};
