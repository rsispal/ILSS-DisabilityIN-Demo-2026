#pragma once

class Logger;
class UsbLowLevelDriver;
class I2CLowLevelDriver;
class BluetoothLowLevelDriver;
class BuzzerLowLevelDriver;
class DRV2605Driver;
class ATECC608BDriver;
class NVSLowLevelDriver;

/**
 * LowLevel - Container for hardware drivers used by the digital twin.
 */
class LowLevel {
    const char* TAG = "LowLevel";

public:
    LowLevel(Logger* logger);
    ~LowLevel();

    bool begin();

    NVSLowLevelDriver& get_nvs() { return *m_nvs_driver; }
    UsbLowLevelDriver& get_usb() { return *m_usb_driver; }
    I2CLowLevelDriver& get_i2c() { return *m_i2c_driver; }
    /** Shared-bus I2C handle bound to ATECC608B (same port as get_i2c()). */
    I2CLowLevelDriver& get_i2c_crypto() { return *m_i2c_crypto_driver; }
    BluetoothLowLevelDriver& get_bluetooth() { return *m_bluetooth_driver; }
    BuzzerLowLevelDriver& get_buzzer() { return *m_buzzer_driver; }
    DRV2605Driver& get_haptics() { return *m_haptics_driver; }
    ATECC608BDriver& get_crypto() { return *m_crypto_driver; }

    Logger* logger = nullptr;

private:
    NVSLowLevelDriver* m_nvs_driver = nullptr;
    UsbLowLevelDriver* m_usb_driver = nullptr;
    I2CLowLevelDriver* m_i2c_driver = nullptr;
    I2CLowLevelDriver* m_i2c_crypto_driver = nullptr;
    BluetoothLowLevelDriver* m_bluetooth_driver = nullptr;
    BuzzerLowLevelDriver* m_buzzer_driver = nullptr;
    DRV2605Driver* m_haptics_driver = nullptr;
    ATECC608BDriver* m_crypto_driver = nullptr;
};
