#include "LowLevel.h"
#include "nvs/NVSLowLevelDriver.h"
#include "usb/UsbLowLevelDriver.h"
#include "i2c/I2CLowLevelDriver.h"
#include "bluetooth/BluetoothLowLevelDriver.h"
#include "buzzer/BuzzerLowLevelDriver.h"
#include "haptics/DRV2605Driver.h"
#include "../application/Hardware.h"
#include "../utils/Logger.h"

LowLevel::LowLevel(Logger* logger) {
    this->logger = logger;
    m_nvs_driver = new NVSLowLevelDriver(this->logger);
    m_usb_driver = new UsbLowLevelDriver(this->logger);
    m_bluetooth_driver = new BluetoothLowLevelDriver(this->logger);
    m_buzzer_driver = new BuzzerLowLevelDriver(HARDWARE_BUZZER_PIN);
    m_i2c_driver = new I2CLowLevelDriver(HARDWARE_I2C_NUM, HARDWARE_DRV2605_I2C_ADDR);
    m_haptics_driver = new DRV2605Driver(m_i2c_driver);
}

LowLevel::~LowLevel() {
    delete m_nvs_driver;
    delete m_usb_driver;
    delete m_i2c_driver;
    delete m_bluetooth_driver;
    delete m_buzzer_driver;
    delete m_haptics_driver;
}

bool LowLevel::begin() {
    logger->LOGI(TAG, "Initializing low level drivers");

    if (!m_nvs_driver->begin()) {
        logger->LOGE(TAG, "NVS initialization failed - persistent storage unavailable");
    }

    if (!m_usb_driver->begin()) {
        logger->LOGW(TAG, "USB initialization failed, continuing with other drivers.");
    }

    if (m_bluetooth_driver) {
        m_bluetooth_driver->initController();
    }

    if (m_i2c_driver && !m_i2c_driver->begin(HARDWARE_I2C_SDA_PIN, HARDWARE_I2C_SCL_PIN, HARDWARE_I2C_CLOCK_SPEED)) {
        logger->LOGW(TAG, "I2C initialization failed, continuing with other drivers.");
    }

    if (m_haptics_driver && !m_haptics_driver->begin()) {
        logger->LOGW(TAG, "Haptics initialization failed, continuing with other drivers.");
    }

    if (m_buzzer_driver && !m_buzzer_driver->begin()) {
        logger->LOGW(TAG, "Buzzer initialization failed, continuing with other drivers.");
    }

    logger->LOGI(TAG, "Low level drivers initialized");
    return true;
}
