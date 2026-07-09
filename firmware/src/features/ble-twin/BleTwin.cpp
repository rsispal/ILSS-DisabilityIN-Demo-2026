#include "BleTwin.h"
#include "../../lowlevel/bluetooth/BluetoothLowLevelDriver.h"
#include "../../application/IndicationController/IndicationController.h"
#include "../../state/State.h"
#include "../../lowlevel/nvs/NVSLowLevelDriver.h"
#include "../../lowlevel/LowLevel.h"
#include "esp_app_desc.h"
#include "mbedtls/md.h"
#include "mbedtls/hkdf.h"
#include "esp_partition.h"
#include <cstring>

BleTwin::BleTwin(Logger* logger, BluetoothLowLevelDriver* ble,
                 IndicationController* indications, State* state)
    : logger_(logger), ble_(ble), indications_(indications), state_(state) {}

bool BleTwin::loadProvisioning() {
    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, static_cast<esp_partition_subtype_t>(0x40), "ble_prov");
    if (!part) {
        logger_->LOGW(TAG, "ble_prov partition not found — using defaults");
        // Deterministic demo UUID from device id string
        const auto id = state_->getDeviceId();
        std::memset(device_uuid_, 0, 16);
        std::memcpy(device_uuid_, id.data(), id.size() < 16 ? id.size() : 16);
        return false;
    }

    uint8_t buf[128]{};
    if (esp_partition_read(part, 0, buf, sizeof(buf)) != ESP_OK) {
        logger_->LOGE(TAG, "Failed to read ble_prov");
        return false;
    }
    // Layout: magic "ILSS" (4) | uuid(16) | serial(32) | brand(1) | secret(32) | reserved
    if (std::memcmp(buf, "ILSS", 4) != 0) {
        logger_->LOGW(TAG, "ble_prov not programmed");
        return false;
    }
    std::memcpy(device_uuid_, buf + 4, 16);
    char serial[33]{};
    std::memcpy(serial, buf + 20, 32);
    ble_->brand_ = buf[52];
    std::memcpy(factory_secret_, buf + 53, 32);
    has_factory_secret_ = true;
    strncpy(ble_->serial_, serial, sizeof(ble_->serial_) - 1);
    logger_->LOGI(TAG, "Loaded provisioned serial=%s", ble_->serial_);
    return true;
}

bool BleTwin::begin(const char* adv_name) {
    loadProvisioning();

    const esp_app_desc_t* app = esp_app_get_description();
    if (app) {
        strncpy(ble_->sw_version_, app->version, sizeof(ble_->sw_version_) - 1);
    }
    ble_->battery_ = state_->getBatteryLevel() ? state_->getBatteryLevel() : 100;

    if (adv_name && adv_name[0]) {
        strncpy(ble_->serial_, adv_name, sizeof(ble_->serial_) - 1);
    }

    ble_->setWriteCallback([this](uint16_t h, const uint8_t* d, size_t n) { onWrite(h, d, n); });
    ble_->setConnectionCallback([this](bool c, uint16_t h) { onConnection(c, h); });

    if (!ble_->begin()) {
        logger_->LOGE(TAG, "BLE begin failed");
        return false;
    }
    if (!ble_->startAdvertising(ble_->serial_)) {
        logger_->LOGE(TAG, "Advertising failed");
        return false;
    }
    publishStatus(indications_->current());
    logger_->LOGI(TAG, "BleTwin started");
    return true;
}

void BleTwin::setDeviceUuid(const uint8_t uuid[16]) {
    std::memcpy(device_uuid_, uuid, 16);
}

void BleTwin::setMasterUuid(const uint8_t uuid[16]) {
    std::memcpy(master_uuid_, uuid, 16);
}

bool BleTwin::isConnected() const {
    return ble_ && ble_->isConnected();
}

void BleTwin::process() {
    // reserved for future TX queues
}

void BleTwin::onConnection(bool connected, uint16_t /*conn*/) {
    if (!connected) {
        paired_ = false;
        encrypt_enabled_ = false;
        if (on_disconnect_) on_disconnect_();
    } else {
        publishStatus(indications_->current());
    }
}

void BleTwin::publishStatus(const ilss::TwinState& state) {
    state.pack(ble_->status_bytes_);
    if (ble_->status_notify_enabled_) {
        ble_->notify(ble_->handle_status_, ble_->status_bytes_, sizeof(ble_->status_bytes_));
    }
}

void BleTwin::notifyLogLine(const char* line) {
    if (!ble_ || !ble_->log_notify_enabled_ || !line) return;
    size_t n = strlen(line);
    if (n > 180) n = 180;
    ble_->notify(ble_->handle_log_, reinterpret_cast<const uint8_t*>(line), n);
}

void BleTwin::sendPacket(ilss::Packet& pkt, bool as_event) {
    std::memcpy(pkt.from, device_uuid_, 16);
    if (ilss::PacketCodec::isNullUuid(pkt.to)) {
        std::memcpy(pkt.to, master_uuid_, 16);
    }
    uint8_t buf[ilss::PACKET_MAX_SIZE];
    size_t n = ilss::PacketCodec::encode(pkt, buf, sizeof(buf));
    if (!n) {
        logger_->LOGE(TAG, "encode failed");
        return;
    }
    if (as_event && ble_->event_notify_enabled_) {
        ble_->notify(ble_->handle_event_, buf, n);
    }
}

void BleTwin::sendAck(const ilss::Packet& req, bool nak, uint8_t reason) {
    ilss::Packet ack;
    ack.flags = nak ? ilss::FLAG_NAK : ilss::FLAG_ACK;
    ack.code = req.code;
    ack.retries = 0;
    ack.message_id = req.message_id;
    ack.fragment_index = 0;
    ack.total_fragments = 1;
    std::memcpy(ack.to, req.from, 16);
    if (nak) {
        ack.data_len = 1;
        ack.data[0] = reason;
    }
    sendPacket(ack, true);
}

void BleTwin::emitTwinEvent(const ilss::TwinState& state) {
    ilss::Packet pkt;
    pkt.flags = ilss::FLAG_EVT;
    pkt.code = ilss::APP_CODE_TWIN_STATE;
    pkt.retries = 3;
    pkt.message_id = next_msg_id_++;
    pkt.data_len = ilss::TwinState::kSize;
    state.pack(pkt.data);
    sendPacket(pkt, true);
    publishStatus(state);
}

void BleTwin::handleCommandPacket(const ilss::Packet& pkt) {
    if (pkt.flags & ilss::FLAG_CMD) {
        if (pkt.code == ilss::APP_CODE_HEARTBEAT) {
            ilss::Packet rpl;
            rpl.flags = ilss::FLAG_RPL;
            rpl.code = ilss::APP_CODE_HEARTBEAT;
            rpl.message_id = pkt.message_id;
            rpl.data_len = ilss::TwinState::kSize;
            indications_->current().pack(rpl.data);
            std::memcpy(rpl.to, pkt.from, 16);
            sendPacket(rpl, true);
            sendAck(pkt, false);
            return;
        }
        if (pkt.code == ilss::APP_CODE_TWIN_STATE) {
            auto desired = ilss::TwinState::unpack(pkt.data, pkt.data_len);
            ilss::TwinNakReason reason = ilss::TwinNakReason::None;
            bool ok = false;
            if (on_twin_) {
                ok = on_twin_(desired, &reason);
            } else {
                ok = indications_->apply(desired, &reason);
            }
            sendAck(pkt, !ok, static_cast<uint8_t>(reason));
            if (ok) {
                publishStatus(indications_->current());
                std::memcpy(master_uuid_, pkt.from, 16);
            }
            return;
        }
        if (pkt.code == ilss::APP_CODE_PAIRING) {
            handlePairingWrite(pkt.data, pkt.data_len);
            sendAck(pkt, false);
            return;
        }
    }
    sendAck(pkt, true, static_cast<uint8_t>(ilss::TwinNakReason::InvalidPayload));
}

void BleTwin::onWrite(uint16_t handle, const uint8_t* data, size_t len) {
    if (handle == ble_->handle_pairing_) {
        handlePairingWrite(data, len);
        return;
    }
    if (handle != ble_->handle_cmd_) return;

    ilss::Packet pkt;
    if (!ilss::PacketCodec::decode(data, len, pkt)) {
        logger_->LOGW(TAG, "Bad packet (%u bytes)", static_cast<unsigned>(len));
        return;
    }
    // Broadcast: process but do not ACK
    const bool broadcast = ilss::PacketCodec::isBroadcast(pkt.to);
    if (pkt.flags & (ilss::FLAG_CMD | ilss::FLAG_EVT)) {
        if (pkt.code == ilss::APP_CODE_TWIN_STATE || pkt.code == ilss::APP_CODE_HEARTBEAT ||
            pkt.code == ilss::APP_CODE_PAIRING) {
            if (broadcast && (pkt.flags & ilss::FLAG_CMD)) {
                auto desired = ilss::TwinState::unpack(pkt.data, pkt.data_len);
                ilss::TwinNakReason reason;
                if (on_twin_) on_twin_(desired, &reason);
                else indications_->apply(desired, &reason);
                return;
            }
            handleCommandPacket(pkt);
        }
    }
}

void BleTwin::handlePairingWrite(const uint8_t* data, size_t len) {
    if (!data || len < 1) return;
    const uint8_t op = data[0];
    // 0x01 pair_req: [op][master_uuid 16][optional nonce 16]
    // 0x03 unpair
    if (op == 0x03) {
        paired_ = false;
        encrypt_enabled_ = false;
        std::memset(session_key_, 0, sizeof(session_key_));
        logger_->LOGI(TAG, "Unpaired");
        uint8_t resp[] = {0x03, 0x01};
        if (ble_->pairing_notify_enabled_) {
            ble_->notify(ble_->handle_pairing_, resp, sizeof(resp));
        }
        return;
    }
    if (op == 0x01 && len >= 17) {
        std::memcpy(master_uuid_, data + 1, 16);
        uint8_t nonce[16]{};
        if (len >= 33) std::memcpy(nonce, data + 17, 16);

        if (has_factory_secret_) {
            // HKDF(factory_secret, nonce) -> session_key
            const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
            int rc = mbedtls_hkdf(md, nonce, 16, factory_secret_, 32,
                                 reinterpret_cast<const unsigned char*>("ILSS-BLE"), 8,
                                 session_key_, 32);
            if (rc == 0) {
                encrypt_enabled_ = true;
                logger_->LOGI(TAG, "Session key derived");
            } else {
                logger_->LOGW(TAG, "HKDF failed %d — pairing without encrypt", rc);
            }
        }
        paired_ = true;
        uint8_t resp[18];
        resp[0] = 0x02;  // pair_ok
        std::memcpy(resp + 1, device_uuid_, 16);
        if (ble_->pairing_notify_enabled_) {
            ble_->notify(ble_->handle_pairing_, resp, 17);
        }
        logger_->LOGI(TAG, "Paired with master");
    }
}
