#include "LowLevel.h"
#include "nvs/NVSLowLevelDriver.h"
#include "wifi/WiFiLowLevelDriver.h"
#include "usb/UsbLowLevelDriver.h"
#include "i2c/I2CLowLevelDriver.h"
#include "bluetooth/BluetoothLowLevelDriver.h"
#include "buzzer/BuzzerLowLevelDriver.h"
#include "haptics/DRV2605Driver.h"
#include "../application/Hardware.h"
#include "../utils/Logger.h"

LowLevel::LowLevel(Logger* logger) {
    this->logger = logger;
    
    // Initialize low level drivers (NVS first as other drivers may depend on it)
    m_nvs_driver = new NVSLowLevelDriver(this->logger);
    m_wifi_driver = new WiFiLowLevelDriver(this->logger);
    m_usb_driver = new UsbLowLevelDriver(this->logger);
    m_bluetooth_driver = new BluetoothLowLevelDriver(this->logger);
    m_buzzer_driver = new BuzzerLowLevelDriver(HARDWARE_BUZZER_PIN);
    
    // I2C will be initialized by application with pin configuration
    // For now, create with default port and address from Hardware.h
    m_i2c_driver = new I2CLowLevelDriver(HARDWARE_I2C_NUM, HARDWARE_DRV2605_I2C_ADDR);
    m_haptics_driver = new DRV2605Driver(m_i2c_driver);
}

LowLevel::~LowLevel() {
    delete m_nvs_driver;
    delete m_wifi_driver;
    delete m_usb_driver;
    delete m_i2c_driver;
    delete m_bluetooth_driver;
    delete m_buzzer_driver;
    delete m_haptics_driver;
}

bool LowLevel::begin() {
    logger->LOGI(TAG, "Initializing low level drivers");

    // Initialize NVS first (required by State and other components)
    if (!m_nvs_driver->begin()) {
        logger->LOGE(TAG, "NVS initialization failed - persistent storage unavailable");
        // Continue anyway, but State will use defaults
    }

    // Initialize USB
    if (!m_usb_driver->begin()) {
        logger->LOGW(TAG, "USB initialization failed, continuing with other drivers.");
    }
    
    // Initialize Bluetooth controller early (before WiFi) for proper coexistence
    // This ensures WiFi and BLE can coexist properly
    if (m_bluetooth_driver) {
        // Just initialize the controller, not the full stack
        // The full stack will be initialized lazily on first scan
        m_bluetooth_driver->initController();
    }
    
    // Initialize WiFi (coexistence should now work properly)
    if (!m_wifi_driver->begin()) {
        logger->LOGW(TAG, "WiFi initialization failed, continuing with other drivers.");
    }
    
    // Initialize I2C (required for haptics and other I2C peripherals)
    if (m_i2c_driver && !m_i2c_driver->begin(HARDWARE_I2C_SDA_PIN, HARDWARE_I2C_SCL_PIN, HARDWARE_I2C_CLOCK_SPEED)) {
        logger->LOGW(TAG, "I2C initialization failed, continuing with other drivers.");
    }
    
    // Initialize haptics driver (DRV2605)
    if (m_haptics_driver && !m_haptics_driver->begin()) {
        logger->LOGW(TAG, "Haptics initialization failed, continuing with other drivers.");
    }

    logger->LOGI(TAG, "Low level drivers initialized");
    return true;
}
