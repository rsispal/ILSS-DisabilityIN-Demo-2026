/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "I2CLowLevelDriver.h"
#include "esp_log.h"
#include <errno.h>
#include <cstring>

I2CLowLevelDriver::I2CLowLevelDriver(i2c_port_num_t i2c_port, uint8_t device_address)
    : m_i2c_port(i2c_port), m_device_address(device_address), m_initialized(false),
      m_bus_handle(nullptr), m_dev_handle(nullptr)
{
    logger.setLogLevel(static_cast<LogLevel>(CONFIG_ILSS_I2C_LOG_LEVEL));
}

I2CLowLevelDriver::~I2CLowLevelDriver()
{
    if (m_initialized) {
        if (m_dev_handle) {
            i2c_master_bus_rm_device(m_dev_handle);
            m_dev_handle = nullptr;
        }
        if (m_bus_handle) {
            i2c_del_master_bus(m_bus_handle);
            m_bus_handle = nullptr;
        }
    }
}

bool I2CLowLevelDriver::begin(gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t freq_hz)
{
    if (m_initialized) {
        logger.LOGW(TAG, "I2C already initialized");
        return true;
    }

    // Configure I2C master bus
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = m_i2c_port;
    bus_config.sda_io_num = sda_pin;
    bus_config.scl_io_num = scl_pin;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;  // Use default clock source
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_config, &m_bus_handle);
    if (err != ESP_OK) {
        logger.LOGE(TAG, "I2C bus creation failed: %s", esp_err_to_name(err));
        return false;
    }

    // Add device to bus
    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = m_device_address;
    dev_config.scl_speed_hz = freq_hz;

    err = i2c_master_bus_add_device(m_bus_handle, &dev_config, &m_dev_handle);
    if (err != ESP_OK) {
        logger.LOGE(TAG, "I2C device addition failed: %s", esp_err_to_name(err));
        i2c_del_master_bus(m_bus_handle);
        m_bus_handle = nullptr;
        return false;
    }

    m_initialized = true;
    logger.LOGI(TAG, "I2C initialized on port %d (SDA: GPIO%d, SCL: GPIO%d, freq: %lu Hz, addr: 0x%02X)", 
                m_i2c_port, sda_pin, scl_pin, freq_hz, m_device_address);
    return true;
}

int I2CLowLevelDriver::read_register(uint8_t reg, uint8_t *value)
{
    if (!is_ready()) {
        logger.LOGE(TAG, "I2C device not ready for read");
        return -ENODEV;
    }
    
    // Write register address, then read value
    uint8_t write_buf[1] = {reg};
    uint8_t read_buf[1] = {0};
    
    esp_err_t ret = i2c_master_transmit_receive(m_dev_handle, write_buf, 1, read_buf, 1, 1000);
    
    if (ret != ESP_OK) {
        logger.LOGE(TAG, "I2C read failed: reg=0x%02X, addr=0x%02X, ret=%s", 
                    reg, m_device_address, esp_err_to_name(ret));
        return -EIO;
    }
    
    *value = read_buf[0];
    logger.LOGD(TAG, "I2C READ:  addr=0x%02X, reg=0x%02X, value=0x%02X (%d)", 
                m_device_address, reg, *value, *value);
    return 0;
}

int I2CLowLevelDriver::write_register(uint8_t reg, uint8_t value)
{
    if (!is_ready()) {
        logger.LOGE(TAG, "I2C device not ready for write");
        return -ENODEV;
    }
    
    logger.LOGD(TAG, "I2C WRITE: addr=0x%02X, reg=0x%02X, value=0x%02X (%d)", 
                m_device_address, reg, value, value);
    
    // Write register address and value
    uint8_t write_buf[2] = {reg, value};
    
    esp_err_t ret = i2c_master_transmit(m_dev_handle, write_buf, 2, 1000);
    
    if (ret != ESP_OK) {
        logger.LOGE(TAG, "I2C write failed: reg=0x%02X, value=0x%02X, addr=0x%02X, ret=%s", 
                    reg, value, m_device_address, esp_err_to_name(ret));
        return -EIO;
    }
    
    return 0;
}

int I2CLowLevelDriver::read_registers(uint8_t reg, uint8_t *buffer, size_t length)
{
    if (!is_ready()) {
        logger.LOGE(TAG, "I2C device not ready for burst read");
        return -ENODEV;
    }
    
    logger.LOGD(TAG, "I2C BURST READ: addr=0x%02X, reg=0x%02X, length=%zu", 
                m_device_address, reg, length);
    
    // Write register address, then read data
    uint8_t write_buf[1] = {reg};
    
    esp_err_t ret = i2c_master_transmit_receive(m_dev_handle, write_buf, 1, buffer, length, 1000);
    
    if (ret != ESP_OK) {
        logger.LOGE(TAG, "I2C burst read failed: reg=0x%02X, length=%zu, addr=0x%02X, ret=%s", 
                    reg, length, m_device_address, esp_err_to_name(ret));
        return -EIO;
    }
    
    logger.LOGDB(TAG, buffer, length, "I2C burst read data");
    return 0;
}

int I2CLowLevelDriver::write_registers(uint8_t reg, const uint8_t *buffer, size_t length)
{
    if (!is_ready()) {
        logger.LOGE(TAG, "I2C device not ready for burst write");
        return -ENODEV;
    }
    
    logger.LOGD(TAG, "I2C BURST WRITE: addr=0x%02X, reg=0x%02X, length=%zu", 
                m_device_address, reg, length);
    logger.LOGDB(TAG, buffer, length, "I2C burst write data");
    
    // Write register address followed by data
    uint8_t *write_buf = new uint8_t[length + 1];
    write_buf[0] = reg;
    memcpy(&write_buf[1], buffer, length);
    
    esp_err_t ret = i2c_master_transmit(m_dev_handle, write_buf, length + 1, 1000);
    
    delete[] write_buf;
    
    if (ret != ESP_OK) {
        logger.LOGE(TAG, "I2C burst write failed: reg=0x%02X, length=%zu, addr=0x%02X, ret=%s", 
                    reg, length, m_device_address, esp_err_to_name(ret));
        return -EIO;
    }
    
    return 0;
}

bool I2CLowLevelDriver::is_ready() const
{
    return m_initialized && m_dev_handle != nullptr;
}

