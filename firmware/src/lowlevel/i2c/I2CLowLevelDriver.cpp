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
    : m_i2c_port(i2c_port),
      m_device_address(device_address),
      m_initialized(false),
      m_owns_bus(true),
      m_bus_owner(nullptr),
      m_bus_handle(nullptr),
      m_dev_handle(nullptr),
      m_freq_hz(400000)
{
    logger.setLogLevel(static_cast<LogLevel>(CONFIG_ILSS_I2C_LOG_LEVEL));
}

I2CLowLevelDriver::I2CLowLevelDriver(I2CLowLevelDriver& bus_owner, uint8_t device_address)
    : m_i2c_port(bus_owner.m_i2c_port),
      m_device_address(device_address),
      m_initialized(false),
      m_owns_bus(false),
      m_bus_owner(&bus_owner),
      m_bus_handle(nullptr),
      m_dev_handle(nullptr),
      m_freq_hz(400000)
{
    logger.setLogLevel(static_cast<LogLevel>(CONFIG_ILSS_I2C_LOG_LEVEL));
}

I2CLowLevelDriver::~I2CLowLevelDriver()
{
    if (m_dev_handle) {
        i2c_master_bus_rm_device(m_dev_handle);
        m_dev_handle = nullptr;
    }
    if (m_owns_bus && m_bus_handle) {
        i2c_del_master_bus(m_bus_handle);
        m_bus_handle = nullptr;
    }
    m_initialized = false;
}

bool I2CLowLevelDriver::addDevice(uint32_t freq_hz)
{
    if (!m_bus_handle) return false;

    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.device_address = m_device_address;
    dev_config.scl_speed_hz = freq_hz;

    esp_err_t err = i2c_master_bus_add_device(m_bus_handle, &dev_config, &m_dev_handle);
    if (err != ESP_OK) {
        logger.LOGE(TAG, "I2C device add failed addr=0x%02X: %s", m_device_address, esp_err_to_name(err));
        return false;
    }
    m_freq_hz = freq_hz;
    return true;
}

bool I2CLowLevelDriver::begin(gpio_num_t sda_pin, gpio_num_t scl_pin, uint32_t freq_hz)
{
    if (!m_owns_bus) {
        logger.LOGE(TAG, "begin() is for the bus owner — use beginShared()");
        return false;
    }
    if (m_initialized) {
        logger.LOGW(TAG, "I2C already initialized");
        return true;
    }

    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = m_i2c_port;
    bus_config.sda_io_num = sda_pin;
    bus_config.scl_io_num = scl_pin;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_config, &m_bus_handle);
    if (err != ESP_OK) {
        logger.LOGE(TAG, "I2C bus creation failed: %s", esp_err_to_name(err));
        return false;
    }

    if (!addDevice(freq_hz)) {
        i2c_del_master_bus(m_bus_handle);
        m_bus_handle = nullptr;
        return false;
    }

    m_initialized = true;
    logger.LOGI(TAG, "I2C bus on port %d (SDA: GPIO%d, SCL: GPIO%d, freq: %lu Hz, addr: 0x%02X)",
                m_i2c_port, sda_pin, scl_pin, static_cast<unsigned long>(freq_hz), m_device_address);
    return true;
}

bool I2CLowLevelDriver::beginShared(uint32_t freq_hz)
{
    if (m_owns_bus) {
        logger.LOGE(TAG, "beginShared() is for secondary devices");
        return false;
    }
    if (m_initialized) {
        logger.LOGW(TAG, "I2C device already initialized");
        return true;
    }
    if (!m_bus_owner || !m_bus_owner->m_bus_handle) {
        logger.LOGE(TAG, "Shared I2C bus not ready");
        return false;
    }

    m_bus_handle = m_bus_owner->m_bus_handle;
    if (!addDevice(freq_hz)) {
        m_bus_handle = nullptr;
        return false;
    }

    m_initialized = true;
    logger.LOGI(TAG, "I2C shared-bus device addr=0x%02X freq=%lu Hz",
                m_device_address, static_cast<unsigned long>(freq_hz));
    return true;
}

int I2CLowLevelDriver::read_register(uint8_t reg, uint8_t *value)
{
    if (!is_ready()) {
        logger.LOGE(TAG, "I2C device not ready for read");
        return -ENODEV;
    }

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

int I2CLowLevelDriver::transmit(const uint8_t* data, size_t length, int timeout_ms)
{
    if (!is_ready()) return -ENODEV;
    esp_err_t ret = i2c_master_transmit(m_dev_handle, data, length, timeout_ms);
    if (ret != ESP_OK) {
        logger.LOGD(TAG, "I2C TX addr=0x%02X len=%zu: %s", m_device_address, length, esp_err_to_name(ret));
        return -EIO;
    }
    return 0;
}

int I2CLowLevelDriver::receive(uint8_t* data, size_t length, int timeout_ms)
{
    if (!is_ready()) return -ENODEV;
    esp_err_t ret = i2c_master_receive(m_dev_handle, data, length, timeout_ms);
    if (ret != ESP_OK) {
        logger.LOGD(TAG, "I2C RX addr=0x%02X len=%zu: %s", m_device_address, length, esp_err_to_name(ret));
        return -EIO;
    }
    return 0;
}

int I2CLowLevelDriver::transmitToAddress(uint8_t address, const uint8_t* data, size_t length, int timeout_ms)
{
    if (!m_initialized || !m_bus_handle) return -ENODEV;

    // ATECC wake (ArduinoECCX08 / Microchip): SDA must stay low ≥ tWLO.
    // That requires ~100 kHz — 400 kHz is too fast and the chip stays asleep.
    const uint32_t speed_hz = (address == 0x00) ? 100000u
                                                 : (m_freq_hz ? m_freq_hz : 400000u);

    i2c_device_config_t dev_config = {};
    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    // Address-only wake: Arduino beginTransmission(0x00); endTransmission()
    // and cryptoauthlib HAL write only the address byte with ACK check off.
    const bool address_only = (length == 0 || data == nullptr);
    if (address_only) {
        dev_config.device_address = I2C_DEVICE_ADDRESS_NOT_USED;
    } else {
        dev_config.device_address = address;
    }
    dev_config.scl_speed_hz = speed_hz;
    dev_config.flags.disable_ack_check = true;

    i2c_master_dev_handle_t tmp = nullptr;
    esp_err_t err = i2c_master_bus_add_device(m_bus_handle, &dev_config, &tmp);
    if (err != ESP_OK) {
        logger.LOGW(TAG, "transmitToAddress add 0x%02X failed: %s", address, esp_err_to_name(err));
        return -EIO;
    }

    if (address_only) {
        uint8_t addr_byte = static_cast<uint8_t>((address << 1) | 0); // 7-bit addr + WRITE
        i2c_operation_job_t jobs[3] = {};
        jobs[0].command = I2C_MASTER_CMD_START;
        jobs[1].command = I2C_MASTER_CMD_WRITE;
        jobs[1].write.ack_check = false;
        jobs[1].write.data = &addr_byte;
        jobs[1].write.total_bytes = 1;
        jobs[2].command = I2C_MASTER_CMD_STOP;
        err = i2c_master_execute_defined_operations(tmp, jobs, 3, timeout_ms);
    } else {
        err = i2c_master_transmit(tmp, data, length, timeout_ms);
    }

    i2c_master_bus_rm_device(tmp);
    if (err != ESP_OK) {
        logger.LOGD(TAG, "transmitToAddress 0x%02X len=%zu @%luHz: %s (ok for wake NACK)",
                    address, address_only ? 0 : length,
                    static_cast<unsigned long>(speed_hz), esp_err_to_name(err));
    }
    return 0;
}

int I2CLowLevelDriver::busReset()
{
    if (!m_bus_handle) return -ENODEV;
    esp_err_t err = i2c_master_bus_reset(m_bus_handle);
    if (err != ESP_OK) {
        logger.LOGW(TAG, "i2c_master_bus_reset: %s", esp_err_to_name(err));
        return -EIO;
    }
    return 0;
}

size_t I2CLowLevelDriver::scan(uint8_t* found_addrs, size_t max_addrs)
{
    if (!m_initialized || m_bus_handle == nullptr) {
        logger.LOGE(TAG, "I2C bus not ready for scan");
        return 0;
    }

    size_t found = 0;
    for (uint16_t addr = 0x08; addr <= 0x77; ++addr) {
        esp_err_t ret = i2c_master_probe(m_bus_handle, addr, 50);
        if (ret == ESP_OK) {
            logger.LOGI(TAG, "I2C device found at 0x%02X", addr);
            if (found_addrs && found < max_addrs) {
                found_addrs[found] = static_cast<uint8_t>(addr);
            }
            ++found;
        }
    }
    logger.LOGI(TAG, "I2C scan complete: %zu device(s)", found);
    return found;
}

bool I2CLowLevelDriver::is_ready() const
{
    return m_initialized && m_dev_handle != nullptr;
}
