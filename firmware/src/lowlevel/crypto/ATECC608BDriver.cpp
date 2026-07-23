#include "ATECC608BDriver.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

// Word addresses (first byte after I2C address)
static constexpr uint8_t kWordSleep = 0x01;
static constexpr uint8_t kWordIdle  = 0x02;
static constexpr uint8_t kWordCommand = 0x03;

// Opcodes
static constexpr uint8_t kOpInfo   = 0x30;
static constexpr uint8_t kOpRead   = 0x02;
static constexpr uint8_t kOpRandom = 0x1B;

static constexpr uint8_t kInfoModeRevision = 0x00;
static constexpr uint8_t kZoneConfig = 0x00;

static constexpr uint8_t kWakeTokenAddr = 0x00;
static constexpr uint8_t kStatusSuccess = 0x00;
static constexpr uint8_t kWakeStatusCount = 0x04;

// tWHI >= 1500us after wake token (datasheet). Self-test-on-wake can need ~20ms.
static constexpr uint32_t kWakeHighUs = 2500;

ATECC608BDriver::ATECC608BDriver(I2CLowLevelDriver* i2c)
    : m_i2c(i2c)
{
#ifdef CONFIG_ILSS_CRYPTO_LOG_LEVEL
    logger.setLogLevel(static_cast<LogLevel>(CONFIG_ILSS_CRYPTO_LOG_LEVEL));
#else
    logger.setLogLevel(LogLevel::INFO);
#endif
}

bool ATECC608BDriver::begin()
{
    if (!m_i2c || !m_i2c->is_ready()) {
        logger.LOGW(TAG, "I2C device not ready");
        return false;
    }

    if (!wake()) {
        logger.LOGW(TAG, "Wake failed — chip @ 0x%02X ACKs probe but did not return wake token",
                    m_i2c->address());
        return false;
    }

    uint8_t rev[kRevisionLen] = {};
    const bool ok = readRevision(rev);
    (void)idle();

    if (!ok) {
        logger.LOGW(TAG, "Info/Revision failed after wake");
        return false;
    }

    m_initialized = true;
    logger.LOGI(TAG, "Ready (%s) rev=%02X %02X %02X %02X addr=0x%02X",
                revisionName(rev), rev[0], rev[1], rev[2], rev[3], m_i2c->address());
    return true;
}

uint16_t ATECC608BDriver::crc16(const uint8_t* data, size_t len)
{
    uint16_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        uint8_t val = data[i];
        for (uint8_t b = 0; b < 8; ++b) {
            const bool bit = ((crc >> 15) ^ val) & 0x01;
            crc <<= 1;
            if (bit) crc ^= 0x8005;
            val >>= 1;
        }
    }
    return crc;
}

bool ATECC608BDriver::pollWakeResponse(uint8_t resp[4], int retries)
{
    // cryptoauthlib expected wake packet: 04 11 33 43
    static constexpr uint8_t kExpected[4] = {0x04, 0x11, 0x33, 0x43};

    for (int i = 0; i < retries; ++i) {
        memset(resp, 0, 4);
        if (m_i2c->receive(resp, 4, 40) == 0) {
            // ArduinoECCX08 checks status byte 0x11; cryptoauthlib matches full packet.
            if (resp[0] == kWakeStatusCount && resp[1] == 0x11) {
                return true;
            }
            if (memcmp(resp, kExpected, 4) == 0) {
                return true;
            }
            logger.LOGW(TAG, "Wake RX unexpected: %02X %02X %02X %02X",
                        resp[0], resp[1], resp[2], resp[3]);
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return false;
}

bool ATECC608BDriver::wakeI2cToken()
{
    // Match ArduinoECCX08::wakeup() / cryptoauthlib hal_i2c_wake:
    //   setClock(100kHz); beginTransmission(0x00); endTransmission(); // no data
    //   delayMicroseconds(1500); then read wake status from device addr.
    // transmitToAddress(0x00, nullptr, 0) forces 100 kHz + address-only.
    (void)m_i2c->transmitToAddress(kWakeTokenAddr, nullptr, 0, 50);
    esp_rom_delay_us(kWakeHighUs);

    uint8_t resp[4] = {};
    if (pollWakeResponse(resp, 20)) {
        logger.LOGI(TAG, "Wake OK %02X %02X %02X %02X",
                    resp[0], resp[1], resp[2], resp[3]);
        return true;
    }
    return false;
}

bool ATECC608BDriver::wake()
{
    if (!m_i2c || !m_i2c->is_ready()) return false;

    if (wakeI2cToken()) return true;

    // Slow path: some configs run self-test on wake (tWHIST up to ~20ms).
    (void)m_i2c->transmitToAddress(kWakeTokenAddr, nullptr, 0, 50);
    vTaskDelay(pdMS_TO_TICKS(25));
    uint8_t resp[4] = {};
    if (pollWakeResponse(resp, 15)) {
        logger.LOGI(TAG, "Wake OK (slow) %02X %02X %02X %02X",
                    resp[0], resp[1], resp[2], resp[3]);
        return true;
    }

    logger.LOGW(TAG, "Wake failed (no 0x04/0x11 token from 0x%02X)", m_i2c->address());
    return false;
}

bool ATECC608BDriver::idle()
{
    if (!m_i2c || !m_i2c->is_ready()) return false;
    const uint8_t pkt[] = {kWordIdle};
    return m_i2c->transmit(pkt, sizeof(pkt), 100) == 0;
}

bool ATECC608BDriver::sleep()
{
    if (!m_i2c || !m_i2c->is_ready()) return false;
    const uint8_t pkt[] = {kWordSleep};
    return m_i2c->transmit(pkt, sizeof(pkt), 100) == 0;
}

bool ATECC608BDriver::readResponse(uint8_t* response, size_t response_len, uint32_t timeout_ms)
{
    if (!response || response_len < 4) return false;

    const uint32_t start = xTaskGetTickCount();
    const uint32_t timeout_ticks = pdMS_TO_TICKS(timeout_ms == 0 ? 1 : timeout_ms);

    while ((xTaskGetTickCount() - start) <= timeout_ticks) {
        memset(response, 0, response_len);
        if (m_i2c->receive(response, response_len, 40) != 0) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        const uint8_t count = response[0];
        if (count < 4 || count > response_len || count > 155) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        const uint16_t expect = static_cast<uint16_t>(response[count - 2]) |
                                (static_cast<uint16_t>(response[count - 1]) << 8);
        const uint16_t got = crc16(response, count - 2);
        if (expect != got) {
            logger.LOGD(TAG, "CRC mismatch expect=0x%04X got=0x%04X (retry)", expect, got);
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        if (count == 4 && response[1] != kStatusSuccess) {
            logger.LOGW(TAG, "ATECC status error 0x%02X", response[1]);
            return false;
        }
        return true;
    }
    logger.LOGW(TAG, "Response timeout");
    return false;
}

bool ATECC608BDriver::sendCommand(uint8_t opcode, uint8_t param1, uint16_t param2,
                                  const uint8_t* data, size_t data_len,
                                  uint8_t* response, size_t response_len,
                                  uint32_t exec_ms)
{
    if (!m_i2c || !m_i2c->is_ready()) return false;

    const size_t body_len = 1 /*count*/ + 1 + 1 + 2 + data_len + 2;
    if (body_len + 1 > 64) return false;

    uint8_t pkt[64];
    pkt[0] = kWordCommand;
    pkt[1] = static_cast<uint8_t>(body_len);
    pkt[2] = opcode;
    pkt[3] = param1;
    pkt[4] = static_cast<uint8_t>(param2 & 0xFF);
    pkt[5] = static_cast<uint8_t>((param2 >> 8) & 0xFF);
    if (data_len && data) {
        memcpy(&pkt[6], data, data_len);
    }
    const uint16_t c = crc16(&pkt[1], body_len - 2);
    pkt[1 + body_len - 2] = static_cast<uint8_t>(c & 0xFF);
    pkt[1 + body_len - 1] = static_cast<uint8_t>((c >> 8) & 0xFF);

    if (m_i2c->transmit(pkt, 1 + body_len, 200) != 0) {
        logger.LOGW(TAG, "Command TX failed op=0x%02X", opcode);
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(exec_ms < 2 ? 2 : exec_ms));
    return readResponse(response, response_len, exec_ms + 50);
}

bool ATECC608BDriver::readRevision(uint8_t out[kRevisionLen])
{
    if (!out) return false;
    if (!wake()) return false;

    uint8_t resp[16] = {};
    const bool ok = sendCommand(kOpInfo, kInfoModeRevision, 0x0000, nullptr, 0, resp, sizeof(resp), 5);
    if (!ok) {
        (void)idle();
        return false;
    }

    if (resp[0] < 7) {
        (void)idle();
        return false;
    }
    memcpy(out, &resp[1], kRevisionLen);
    (void)idle();
    return true;
}

bool ATECC608BDriver::readSerial(uint8_t out[kSerialLen])
{
    if (!out) return false;
    if (!wake()) return false;

    uint8_t resp[40] = {};
    const uint8_t param1 = kZoneConfig | 0x80;
    const bool ok = sendCommand(kOpRead, param1, 0x0000, nullptr, 0, resp, sizeof(resp), 10);
    if (!ok || resp[0] < 35) {
        (void)idle();
        return false;
    }

    const uint8_t* cfg = &resp[1];
    out[0] = cfg[0];
    out[1] = cfg[1];
    out[2] = cfg[2];
    out[3] = cfg[3];
    out[4] = cfg[8];
    out[5] = cfg[9];
    out[6] = cfg[10];
    out[7] = cfg[11];
    out[8] = cfg[12];

    (void)idle();
    return true;
}

bool ATECC608BDriver::random(uint8_t out[kRandomLen])
{
    if (!out) return false;
    if (!wake()) return false;

    uint8_t resp[40] = {};
    const bool ok = sendCommand(kOpRandom, 0x00, 0x0000, nullptr, 0, resp, sizeof(resp), 25);
    if (!ok || resp[0] < 35) {
        (void)idle();
        return false;
    }
    memcpy(out, &resp[1], kRandomLen);
    (void)idle();
    return true;
}

const char* ATECC608BDriver::revisionName(const uint8_t rev[kRevisionLen])
{
    if (!rev) return "unknown";
    if (rev[0] == 0x00 && rev[1] == 0x00 && rev[2] == 0x60) {
        if (rev[3] == 0x02) return "ATECC608A";
        if (rev[3] >= 0x03) return "ATECC608B";
        return "ATECC608";
    }
    if (rev[2] == 0x50) return "ATECC508A";
    return "CryptoAuth";
}
