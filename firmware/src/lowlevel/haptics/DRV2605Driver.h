#pragma once

#include "lowlevel/i2c/I2CLowLevelDriver.h"
#include "../../utils/Logger.h"
#include <cstdint>

// DRV2605 Register definitions (based on Adafruit library)
#define DRV2605_REG_STATUS           0x00
#define DRV2605_REG_MODE             0x01
#define DRV2605_REG_RTPIN            0x02
#define DRV2605_REG_LIBRARY          0x03
#define DRV2605_REG_WAVESEQ1         0x04
#define DRV2605_REG_WAVESEQ2         0x05
#define DRV2605_REG_WAVESEQ3         0x06
#define DRV2605_REG_WAVESEQ4         0x07
#define DRV2605_REG_WAVESEQ5         0x08
#define DRV2605_REG_WAVESEQ6         0x09
#define DRV2605_REG_WAVESEQ7         0x0A
#define DRV2605_REG_WAVESEQ8         0x0B
#define DRV2605_REG_GO               0x0C
#define DRV2605_REG_OVERDRIVE        0x0D
#define DRV2605_REG_SUSTAINPOS       0x0E
#define DRV2605_REG_SUSTAINNEG       0x0F
#define DRV2605_REG_BREAK            0x10
#define DRV2605_REG_AUDIOMODE        0x11
#define DRV2605_REG_AUDIOLVL         0x12
#define DRV2605_REG_AUDIOMAX         0x13
#define DRV2605_REG_AUDIOMIN         0x14
#define DRV2605_REG_AUDIOTHRESH      0x15
#define DRV2605_REG_AUDIOFRQ         0x16
#define DRV2605_REG_AUDIOFRQ2        0x17
#define DRV2605_REG_RATEDV           0x16
#define DRV2605_REG_CLAMPV           0x17
#define DRV2605_REG_AUTOCALCOMP      0x18
#define DRV2605_REG_AUTOCALEMP       0x19
#define DRV2605_REG_FEEDBACK         0x1A
#define DRV2605_REG_CONTROL1         0x1B
#define DRV2605_REG_CONTROL2         0x1C
#define DRV2605_REG_CONTROL3         0x1D
#define DRV2605_REG_CONTROL4         0x1E
#define DRV2605_REG_VBAT             0x1F
#define DRV2605_REG_LRARESON         0x20

// Mode definitions
#define DRV2605_MODE_INTTRIG         0x00
#define DRV2605_MODE_EXTTRIGEDGE     0x01
#define DRV2605_MODE_EXTTRIGLVL      0x02
#define DRV2605_MODE_PWMANALOG       0x03
#define DRV2605_MODE_AUDIOVIBE       0x04
#define DRV2605_MODE_REALTIME        0x05
#define DRV2605_MODE_DIAGNOS         0x06
#define DRV2605_MODE_AUTOCAL         0x07

// Library selection
#define DRV2605_LIBRARY_EMPTY        0x00
#define DRV2605_LIBRARY_TS2200_A     0x01
#define DRV2605_LIBRARY_TS2200_B     0x02
#define DRV2605_LIBRARY_TS2200_C     0x03
#define DRV2605_LIBRARY_TS2200_D     0x04
#define DRV2605_LIBRARY_TS2200_E     0x05
#define DRV2605_LIBRARY_TS2200_F     0x06
#define DRV2605_LIBRARY_LRA          0x06

// Go register
#define DRV2605_GO                   0x01

// Standby bit
#define DRV2605_STANDBY              0x40


class DRV2605Driver {
    const char* TAG = "Haptic";

public:
    DRV2605Driver(I2CLowLevelDriver *i2c_driver);
    ~DRV2605Driver();
    
    // Initialize DRV2605 driver (standardized API)
    bool begin();
    
    // Check if driver is ready
    bool isReady() const { return m_initialized; }
    
    int set_mode(uint8_t mode);
    int set_library(uint8_t library);
    int set_waveform(uint8_t slot, uint8_t waveform);
    int go();
    int stop();
    int set_realtime_value(uint8_t rtp);
    int set_rated_voltage(float voltage);
    int set_overdrive_clamp_voltage(float voltage);
    int set_lra_resonance_frequency(uint8_t frequency_hz);
    int select_library(uint8_t lib);
    int set_waveform_sequence(uint8_t slot, uint8_t waveform);
    int play_pattern(uint8_t pattern);

private:
    I2CLowLevelDriver *m_i2c_driver;
    Logger logger;
    bool m_initialized;
    int write_register(uint8_t reg, uint8_t value);
    int read_register(uint8_t reg, uint8_t *value);
    uint8_t voltage_to_reg_value(float voltage);
};

