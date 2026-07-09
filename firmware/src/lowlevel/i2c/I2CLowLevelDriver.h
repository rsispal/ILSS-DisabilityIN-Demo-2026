/*
 * Copyright (c) 2024
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include "driver/i2c_master.h"
#include "../../utils/Logger.h"

/**
 * I2CLowLevelDriver - Low-level I2C driver for ESP-IDF
 * 
 * Provides a simple interface for I2C communication, compatible with the
 * Zephyr API but implemented using ESP-IDF drivers.
 * Device-specific driver: one instance per I2C device address.
 */
class I2CLowLevelDriver {
    const char* TAG = "I2CLowLevelDriver";
public:
    I2CLowLevelDriver(i2c_port_num_t i2c_port, uint8_t device_address);
    ~I2CLowLevelDriver();
    
    // Initialize I2C bus (call once at startup)
    bool begin(gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t freq_hz = 400000);
    
    int read_register(uint8_t reg, uint8_t *value);
    int write_register(uint8_t reg, uint8_t value);
    int read_registers(uint8_t reg, uint8_t *buffer, size_t length);
    int write_registers(uint8_t reg, const uint8_t *buffer, size_t length);
    
    bool is_ready() const;

private:
    i2c_port_num_t m_i2c_port;
    uint8_t m_device_address;
    bool m_initialized;
    i2c_master_bus_handle_t m_bus_handle;
    i2c_master_dev_handle_t m_dev_handle;
    Logger logger;
};

