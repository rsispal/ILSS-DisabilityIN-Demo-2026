#include "DRV2605Driver.h"
#include <errno.h>

DRV2605Driver::DRV2605Driver(I2CLowLevelDriver *i2c_driver)
    : m_i2c_driver(i2c_driver), m_initialized(false)
{
    logger.setLogLevel(LogLevel::INFO);
}

DRV2605Driver::~DRV2605Driver()
{
    if (m_initialized) {
        stop();
        m_initialized = false;
    }
}

int DRV2605Driver::write_register(uint8_t reg, uint8_t value)
{
    if (!m_i2c_driver) {
        return -ENODEV;
    }
    return m_i2c_driver->write_register(reg, value);
}

int DRV2605Driver::read_register(uint8_t reg, uint8_t *value)
{
    if (!m_i2c_driver) {
        return -ENODEV;
    }
    return m_i2c_driver->read_register(reg, value);
}

uint8_t DRV2605Driver::voltage_to_reg_value(float voltage)
{
    // Convert voltage (in volts) to register value
    // Formula from datasheet: Rated Voltage = (value / 255) * 5.3V * 2
    // So: value = (voltage / 10.6) * 255
    // For LRA, Adafruit typically uses higher voltages for better intensity:
    // Rated voltage: 3.0V, Overdrive: 3.6V
    uint8_t value = static_cast<uint8_t>((voltage / 10.6f) * 255.0f);
    if (value > 255) {
        value = 255;
    }
    return value;
}

void DRV2605Driver::log_status(uint8_t status)
{
    const uint8_t device_id = (status >> 5) & 0x07;
    logger.LOGI(TAG, "DRV2605 Status: 0x%02x (id=%u%s%s%s)",
                status,
                device_id,
                (status & 0x08) ? " DIAG_FAIL" : "",
                (status & 0x02) ? " OVER_TEMP" : "",
                (status & 0x01) ? " OC_DETECT" : "");
    if (device_id != 7) {
        logger.LOGW(TAG, "Unexpected device ID %u (want 7 = DRV2605L)", device_id);
    }
    if (status & 0x01) {
        logger.LOGW(TAG, "OC_DETECT set — H-bridge disabled until fault cleared; "
                         "check MOTOR+/- short/open, EN jumper, and actuator wiring");
    }
}

int DRV2605Driver::read_status(uint8_t* status_out)
{
    uint8_t value = 0;
    int ret = read_register(DRV2605_REG_STATUS, &value);
    if (ret < 0) {
        return ret;
    }
    if (status_out) {
        *status_out = value;
    }
    return 0;
}

int DRV2605Driver::recover()
{
    uint8_t status = 0;
    // Reading STATUS clears sticky OC/OT/DIAG flags.
    int ret = read_status(&status);
    if (ret < 0) {
        return ret;
    }
    log_status(status);

    // Adafruit-compatible: write MODE directly (must clear STANDBY bit 6).
    ret = write_register(DRV2605_REG_MODE, DRV2605_MODE_INTTRIG);
    if (ret < 0) {
        return ret;
    }
    ret = write_register(DRV2605_REG_GO, 0x00);
    return ret;
}

bool DRV2605Driver::begin()
{
    if (m_initialized) {
        logger.LOGW(TAG, "DRV2605 already initialized");
        return true;
    }
    
    int ret;
    uint8_t value;
    
    // Read status register to verify communication (also clears sticky faults)
    ret = read_register(DRV2605_REG_STATUS, &value);
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to read status register: %d", ret);
        return false;
    }
    log_status(value);
    
    // Match Arduino's exact initialization sequence (ERM mode)
    logger.LOGD(TAG, "Initializing DRV2605 (ERM mode)");
    
    ret = write_register(DRV2605_REG_MODE, 0x00); // out of standby
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set MODE: %d", ret);
        return false;
    }
    
    ret = write_register(DRV2605_REG_RTPIN, 0x00); // no real-time-playback
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set RTPIN: %d", ret);
        return false;
    }
    
    ret = write_register(DRV2605_REG_WAVESEQ1, 1); // strong click
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set WAVESEQ1: %d", ret);
        return false;
    }
    
    ret = write_register(DRV2605_REG_WAVESEQ2, 0); // end sequence
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set WAVESEQ2: %d", ret);
        return false;
    }
    
    ret = write_register(DRV2605_REG_OVERDRIVE, 0); // no overdrive
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set OVERDRIVE: %d", ret);
        return false;
    }
    
    ret = write_register(DRV2605_REG_SUSTAINPOS, 0);
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set SUSTAINPOS: %d", ret);
        return false;
    }
    
    ret = write_register(DRV2605_REG_SUSTAINNEG, 0);
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set SUSTAINNEG: %d", ret);
        return false;
    }
    
    ret = write_register(DRV2605_REG_BREAK, 0);
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set BREAK: %d", ret);
        return false;
    }
    
    ret = write_register(DRV2605_REG_AUDIOMAX, 0x64);
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set AUDIOMAX: %d", ret);
        return false;
    }
    
    // ERM mode: clear N_ERM_LRA bit (bit 7) in FEEDBACK register
    ret = read_register(DRV2605_REG_FEEDBACK, &value);
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to read FEEDBACK: %d", ret);
        return false;
    }
    ret = write_register(DRV2605_REG_FEEDBACK, value & 0x7F); // Clear bit 7
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set FEEDBACK: %d", ret);
        return false;
    }
    
    // ERM open loop: set ERM_OPEN_LOOP bit (bit 5) in CONTROL3 register
    ret = read_register(DRV2605_REG_CONTROL3, &value);
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to read CONTROL3: %d", ret);
        return false;
    }
    ret = write_register(DRV2605_REG_CONTROL3, value | 0x20); // Set bit 5
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set CONTROL3: %d", ret);
        return false;
    }

    // Clear any OC latch from power-up / prior runs and ensure not in standby.
    recover();
    
    m_initialized = true;
    logger.LOGI(TAG, "DRV2605 initialized successfully (ERM mode)");
    return true;
}

int DRV2605Driver::set_mode(uint8_t mode)
{
    // Match Adafruit: write MODE register directly so STANDBY (bit 6) is cleared.
    // Preserving STANDBY left the chip silent after OC_DETECT auto-standby.
    return write_register(DRV2605_REG_MODE, mode & 0x07);
}

int DRV2605Driver::set_library(uint8_t library)
{
    return write_register(DRV2605_REG_LIBRARY, library);
}

int DRV2605Driver::set_waveform(uint8_t slot, uint8_t waveform)
{
    if (slot > 7) {
        return -EINVAL;
    }
    return write_register(DRV2605_REG_WAVESEQ1 + slot, waveform);
}

int DRV2605Driver::set_waveform_sequence(uint8_t slot, uint8_t waveform)
{
    return set_waveform(slot, waveform);
}

int DRV2605Driver::go()
{
    return write_register(DRV2605_REG_GO, DRV2605_GO);
}

int DRV2605Driver::stop()
{
    // Clear the GO bit
    return write_register(DRV2605_REG_GO, 0x00);
}

int DRV2605Driver::set_realtime_value(uint8_t rtp)
{
    int ret = set_mode(DRV2605_MODE_REALTIME);
    if (ret < 0) {
        return ret;
    }
    return write_register(DRV2605_REG_RTPIN, rtp);
}

int DRV2605Driver::set_rated_voltage(float voltage)
{
    uint8_t value = voltage_to_reg_value(voltage);
    logger.LOGI(TAG, "Setting rated voltage: %.2fV -> reg_value=0x%02X (%d)", 
            static_cast<double>(voltage), value, value);
    return write_register(DRV2605_REG_RATEDV, value);
}

int DRV2605Driver::set_overdrive_clamp_voltage(float voltage)
{
    uint8_t value = voltage_to_reg_value(voltage);
    logger.LOGI(TAG, "Setting overdrive clamp voltage: %.2fV -> reg_value=0x%02X (%d)", 
            static_cast<double>(voltage), value, value);
    return write_register(DRV2605_REG_CLAMPV, value);
}

int DRV2605Driver::set_lra_resonance_frequency(uint8_t frequency_hz)
{
    // Formula from datasheet: value = (frequency_hz / 100) - 1
    // Range: 100-400 Hz, so value range is 0x00 to 0x03
    uint8_t value = 0;
    if (frequency_hz >= 100 && frequency_hz <= 400) {
        value = static_cast<uint8_t>((frequency_hz / 100) - 1);
        if (value > 3) {
            value = 3; // Max value
        }
    }
    logger.LOGI(TAG, "Setting LRA resonance frequency: %d Hz -> reg_value=0x%02X (%d)", frequency_hz, value, value);
    return write_register(DRV2605_REG_LRARESON, value);
}

int DRV2605Driver::select_library(uint8_t lib)
{
    return set_library(lib);
}

int DRV2605Driver::play_pattern(uint8_t pattern)
{
    int ret;
    uint8_t status = 0;
    
    logger.LOGD(TAG, "Playing pattern %d", pattern);

    // Clear sticky OC/OT and force out of standby before every playback.
    ret = read_status(&status);
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to read status before play: %d", ret);
        return ret;
    }
    if (status & 0x01) {
        logger.LOGW(TAG, "OC_DETECT was set before play (0x%02x) — cleared; retrying wake", status);
    }
    
    // Match Arduino's exact sequence: selectLibrary(1), setMode(0), setWaveform, go()
    // Note: Arduino uses Library 1 (ERM), not Library 6 (LRA)
    ret = set_library(1); // ERM library, not LRA!
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set library: %d", ret);
        return ret;
    }
    
    ret = set_mode(DRV2605_MODE_INTTRIG);
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set mode: %d", ret);
        return ret;
    }
    
    ret = set_waveform(0, pattern);
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set waveform: %d", ret);
        return ret;
    }
    
    ret = set_waveform(1, 0);
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to set waveform slot 1: %d", ret);
        return ret;
    }
    
    ret = go();
    if (ret < 0) {
        logger.LOGE(TAG, "Failed to trigger GO: %d", ret);
        return ret;
    }

    // If the actuator path is shorted/missing, OC re-latches immediately.
    ret = read_status(&status);
    if (ret == 0 && (status & 0x01)) {
        logger.LOGE(TAG, "Pattern %d: OC_DETECT re-latched after GO (0x%02x) — motor path fault",
                    pattern, status);
        return -EIO;
    }
    
    logger.LOGD(TAG, "Pattern %d triggered", pattern);
    return 0;
}

