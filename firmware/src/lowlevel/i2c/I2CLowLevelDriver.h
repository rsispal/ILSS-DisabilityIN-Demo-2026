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
 * I2CLowLevelDriver — ESP-IDF I2C master helper.
 *
 * One instance owns the bus (begin with SDA/SCL). Additional devices on the
 * same bus use the shared-bus constructor + beginShared().
 */
class I2CLowLevelDriver {
    const char* TAG = "I2CLowLevelDriver";
public:
    /** Bus-owning device (creates master bus in begin()). */
    I2CLowLevelDriver(i2c_port_num_t i2c_port, uint8_t device_address);

    /** Extra device on an existing bus owner's bus (beginShared()). */
    I2CLowLevelDriver(I2CLowLevelDriver& bus_owner, uint8_t device_address);

    ~I2CLowLevelDriver();

    bool begin(gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t freq_hz = 400000);
    bool beginShared(uint32_t freq_hz = 400000);

    int read_register(uint8_t reg, uint8_t *value);
    int write_register(uint8_t reg, uint8_t value);
    int read_registers(uint8_t reg, uint8_t *buffer, size_t length);
    int write_registers(uint8_t reg, const uint8_t *buffer, size_t length);

    /** Raw device transmit / receive (no register address prefix). */
    int transmit(const uint8_t* data, size_t length, int timeout_ms = 1000);
    int receive(uint8_t* data, size_t length, int timeout_ms = 1000);

    /**
     * Transmit to an arbitrary 7-bit address on this bus.
     * ACK checking is disabled (ATECC wake @ 0x00 NACKs by design).
     * length 0 = address-only (ArduinoECCX08 wake). Address 0x00 forces 100 kHz
     * so SDA stays low ≥ tWLO.
     */
    int transmitToAddress(uint8_t address, const uint8_t* data, size_t length, int timeout_ms = 50);

    /** Soft-reset the I2C master state machine (after GPIO wake bit-bang). */
    int busReset();

    /**
     * Probe 7-bit addresses 0x08..0x77 on the bus.
     * Writes up to max_addrs found addresses into found_addrs.
     * Returns the number of responding devices (may exceed max_addrs).
     */
    size_t scan(uint8_t* found_addrs, size_t max_addrs);

    bool is_ready() const;
    uint8_t address() const { return m_device_address; }
    i2c_master_bus_handle_t bus_handle() const { return m_bus_handle; }

private:
    i2c_port_num_t m_i2c_port;
    uint8_t m_device_address;
    bool m_initialized;
    bool m_owns_bus;
    I2CLowLevelDriver* m_bus_owner;
    i2c_master_bus_handle_t m_bus_handle;
    i2c_master_dev_handle_t m_dev_handle;
    uint32_t m_freq_hz;
    Logger logger;

    bool addDevice(uint32_t freq_hz);
};
