#include "BleTwin.h"
#include "BleCrypto.h"
#include "../../lowlevel/bluetooth/BluetoothLowLevelDriver.h"
#include "../../application/IndicationController/IndicationController.h"
#include "../../state/State.h"
#include "../../lowlevel/LowLevel.h"
#include "esp_app_desc.h"
#include "esp_timer.h"
#include "esp_partition.h"
#include <cstring>

BleTwin::BleTwin(Logger* logger, LowLevel* lowLevel,
                 IndicationController* indications, State* state)
    : logger_(logger), lowLevel_(lowLevel),
      ble_(lowLevel ? &lowLevel->get_bluetooth() : nullptr),
      indications_(indications), state_(state) {
    log_queue_ = xQueueCreate(kLogQueueDepth, sizeof(LogLine));
}

BleTwin::~BleTwin() {
    if (log_queue_) {
        vQueueDelete(log_queue_);
        log_queue_ = nullptr;
    }
}

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
    ble_->setBrand(buf[52]);
    std::memcpy(factory_secret_, buf + 53, 32);
    has_factory_secret_ = true;
    ble_->setSerial(serial);
    logger_->LOGI(TAG, "Loaded provisioned serial=%s", ble_->serial());
    return true;
}

bool BleTwin::begin(const char* adv_name) {
    const bool provisioned = loadProvisioning();

    const esp_app_desc_t* app = esp_app_get_description();
    if (app) {
        ble_->setSwVersion(app->version);
    }
    ble_->setBattery(state_->getBatteryLevel() ? state_->getBatteryLevel() : 100);

    // Provisioned serial from ble_prov wins for Chrome picker / advertising name.
    // Only fall back to caller-supplied name when unprovisioned.
    if (!provisioned && adv_name && adv_name[0]) {
        ble_->setSerial(adv_name);
    }

    ble_->setWriteCallback([this](uint16_t h, const uint8_t* d, size_t n) { onWrite(h, d, n); });
    ble_->setConnectionCallback([this](bool c, uint16_t h) { onConnection(c, h); });

    if (!ble_->begin()) {
        logger_->LOGE(TAG, "BLE begin failed");
        return false;
    }
    if (!ble_->startAdvertising(ble_->serial())) {
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
    if (!ble_) return;

    // Rising edge on Status CCCD — push once from app task (never from GAP).
    if (ble_->statusNotifyEnabled() && !status_cccd_seen_) {
        status_cccd_seen_ = true;
        if (indications_) publishStatus(indications_->current());
    } else if (!ble_->statusNotifyEnabled()) {
        status_cccd_seen_ = false;
    }

    heartbeat_.tick(paired_ && isConnected(), nowMs(),
                    [this]() { emitHeartbeatPoll(); },
                    [this](const char* why) { failHeartbeat(why); });

    if (!log_queue_ || !ble_->logNotifyEnabled()) {
        // Drop queued lines while unsubscribed so we don't backlog forever.
        if (log_queue_ && !ble_->logNotifyEnabled()) {
            LogLine drop;
            while (xQueueReceive(log_queue_, &drop, 0) == pdTRUE) {
            }
        }
        return;
    }
    LogLine line;
    // Bound work per tick so we don't starve the event loop.
    for (int i = 0; i < 4; ++i) {
        if (xQueueReceive(log_queue_, &line, 0) != pdTRUE) break;
        ble_->notify(ble_->handleLog(), reinterpret_cast<const uint8_t*>(line.text), line.len);
    }
}

uint32_t BleTwin::nowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

void BleTwin::emitHeartbeatPoll() {
    if (!paired_ || !isConnected() || !ble_->eventNotifyEnabled()) return;
    ilss::Packet pkt;
    pkt.flags = ilss::FLAG_CMD;
    pkt.code = ilss::APP_CODE_HEARTBEAT;
    pkt.retries = 1;
    pkt.message_id = next_msg_id_++;
    pkt.data_len = ilss::TwinState::kSize;
    if (indications_) {
        indications_->current().pack(pkt.data);
    } else {
        ilss::TwinState::idle().pack(pkt.data);
    }
    heartbeat_.markPollSent(pkt.message_id, nowMs());
    sendPacket(pkt, true);
    logger_->LOGI(TAG, "Heartbeat poll TX msgId=%u", heartbeat_.pendingMsgId());
}

void BleTwin::failHeartbeat(const char* why) {
    logger_->LOGW(TAG, "Heartbeat failed — %s; disconnect → unpaired", why ? why : "unknown");
    heartbeat_.reset();
    paired_ = false;
    encrypt_enabled_ = false;
    if (ble_) ble_->disconnect();
}

void BleTwin::handleInboundAck(const ilss::Packet& pkt) {
    if (pkt.code != ilss::APP_CODE_HEARTBEAT) return;
    if (heartbeat_.onAck(pkt.code, pkt.message_id)) {
        logger_->LOGI(TAG, "Heartbeat ACK RX msgId=%u", pkt.message_id);
    }
}

void BleTwin::onConnection(bool connected, uint16_t /*conn*/) {
    if (!connected) {
        paired_ = false;
        encrypt_enabled_ = false;
        status_cccd_seen_ = false;
        heartbeat_.onDisconnected();
        if (on_disconnect_) on_disconnect_();
    } else if (on_connecting_) {
        on_connecting_();
    }
}

void BleTwin::publishStatus(const ilss::TwinState& state) {
    state.pack(ble_->statusBytes());
    if (ble_->statusNotifyEnabled()) {
        ble_->notify(ble_->handleStatus(), ble_->statusBytes(), ble_->statusBytesSize());
    }
}

void BleTwin::notifyLogLine(const char* line) {
    if (!line || !log_queue_) return;
    // Never GATT-notify from Logger/GAP context — queue for process().
    LogLine item{};
    size_t n = strlen(line);
    if (n > kLogLineMax - 1) n = kLogLineMax - 1;
    item.len = static_cast<uint16_t>(n);
    std::memcpy(item.text, line, n);
    item.text[n] = '\0';
    // Non-blocking; drop under backpressure (docs allow this).
    (void)xQueueSend(log_queue_, &item, 0);
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
    if (as_event && ble_->eventNotifyEnabled()) {
        ble_->notify(ble_->handleEvent(), buf, n);
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

void BleTwin::emitButtonEvent(uint8_t side, uint8_t action) {
    if (!paired_ || !isConnected()) return;
    ilss::Packet pkt;
    pkt.flags = ilss::FLAG_EVT;
    pkt.code = ilss::APP_CODE_BUTTON;
    pkt.retries = 1;
    pkt.message_id = next_msg_id_++;
    pkt.data_len = 2;
    pkt.data[0] = side;
    pkt.data[1] = action;
    sendPacket(pkt, true);
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
    // Handles may still be 0 if a write races host sync — refresh and retry match.
    if (ble_->handleCmd() == 0 || ble_->handlePairing() == 0) {
        ble_->refreshGattHandles();
    }

    if (handle == ble_->handlePairing()) {
        logger_->LOGI(TAG, "Pairing write %u bytes", static_cast<unsigned>(len));
        handlePairingWrite(data, len);
        return;
    }
    if (handle != ble_->handleCmd()) {
        logger_->LOGD(TAG, "Ignore write handle=%u (cmd=%u pairing=%u)",
                      handle, ble_->handleCmd(), ble_->handlePairing());
        return;
    }

    logger_->LOGI(TAG, "Command write %u bytes", static_cast<unsigned>(len));
    ilss::Packet pkt;
    if (!ilss::PacketCodec::decode(data, len, pkt)) {
        logger_->LOGW(TAG, "Bad packet (%u bytes)", static_cast<unsigned>(len));
        return;
    }
    logger_->LOGI(TAG, "RX flags=0x%02x code=0x%02x data_len=%u",
                  pkt.flags, pkt.code, pkt.data_len);

    // Web ACK to our outbound heartbeat poll.
    if (pkt.flags & (ilss::FLAG_ACK | ilss::FLAG_NAK)) {
        handleInboundAck(pkt);
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
        if (ble_->pairingNotifyEnabled()) {
            ble_->notify(ble_->handlePairing(), resp, sizeof(resp));
        }
        return;
    }
    if (op == 0x01 && len >= 17) {
        std::memcpy(master_uuid_, data + 1, 16);
        uint8_t nonce[16]{};
        if (len >= 33) std::memcpy(nonce, data + 17, 16);

        if (has_factory_secret_) {
            int rc = ilss::hkdfSha256(nonce, 16, factory_secret_, 32,
                                      reinterpret_cast<const uint8_t*>("ILSS-BLE"), 8,
                                      session_key_, 32);
            if (rc == 0) {
                encrypt_enabled_ = true;
                logger_->LOGI(TAG, "Session key derived");
            } else {
                logger_->LOGW(TAG, "HKDF failed %d — pairing without encrypt", rc);
            }
        }
        paired_ = true;
        heartbeat_.onPaired(nowMs());
        uint8_t resp[18];
        resp[0] = 0x02;  // pair_ok
        std::memcpy(resp + 1, device_uuid_, 16);
        if (ble_->pairingNotifyEnabled()) {
            ble_->notify(ble_->handlePairing(), resp, 17);
        }
        logger_->LOGI(TAG, "Paired with master");
        if (on_paired_) on_paired_();
    }
}
