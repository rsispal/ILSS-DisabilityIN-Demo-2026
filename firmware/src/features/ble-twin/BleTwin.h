#pragma once

#include "../../utils/Logger.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <cstdint>
#include <functional>

#include "../../protocol/PacketCodec.h"
#include "../../protocol/TwinState.h"
#include "BleHeartbeat.h"

class BluetoothLowLevelDriver;
class IndicationController;
class State;
class LowLevel;

/**
 * BleTwin - GATT twin control: packet RX/TX, status, pairing, log notify.
 */
class BleTwin {
    const char* TAG = "BleTwin";

public:
    using TwinStateHandler = std::function<bool(const ilss::TwinState& desired, ilss::TwinNakReason* reason)>;
    using DisconnectHandler = std::function<void()>;
    using LinkHandler = std::function<void()>;

    BleTwin(Logger* logger, LowLevel* lowLevel, IndicationController* indications, State* state);
    ~BleTwin();

    bool begin(const char* adv_name);
    /** Drain deferred TX (log lines). Call from app task — never from NimBLE host. */
    void process();

    void setDeviceUuid(const uint8_t uuid[16]);
    void setMasterUuid(const uint8_t uuid[16]);
    void setTwinStateHandler(TwinStateHandler h) { on_twin_ = std::move(h); }
    void setDisconnectHandler(DisconnectHandler h) { on_disconnect_ = std::move(h); }
    void setConnectingHandler(LinkHandler h) { on_connecting_ = std::move(h); }
    void setPairedHandler(LinkHandler h) { on_paired_ = std::move(h); }

    void publishStatus(const ilss::TwinState& state);
    void emitTwinEvent(const ilss::TwinState& state);
    /** Momentary side-button cue for the web twin (left/right press). */
    void emitButtonEvent(uint8_t side, uint8_t action = ilss::BUTTON_ACTION_PRESS);
    /** Device→web alive poll (CMD HEARTBEAT on Event notify). */
    void emitHeartbeatPoll();
    /** Queue a log line for BLE notify from process() (safe from any context). */
    void notifyLogLine(const char* line);

    bool isPaired() const { return paired_; }
    bool isConnected() const;

    static constexpr uint32_t kHeartbeatIntervalMs = BleHeartbeat::kIntervalMs;
    static constexpr uint32_t kHeartbeatAckTimeoutMs = BleHeartbeat::kAckTimeoutMs;

    /** Pairing: op 0x01=pair_req, 0x02=pair_ok, 0x03=unpair, 0x10=dh_pub */
    void handlePairingWrite(const uint8_t* data, size_t len);

    // Session crypto (AES-256-GCM key after pair)
    bool encryptEnabled() const { return encrypt_enabled_ && paired_; }
    const uint8_t* sessionKey() const { return session_key_; }

private:
    static constexpr size_t kLogLineMax = 180;
    static constexpr size_t kLogQueueDepth = 16;

    struct LogLine {
        uint16_t len;
        char text[kLogLineMax];
    };

    Logger* logger_;
    LowLevel* lowLevel_;
    BluetoothLowLevelDriver* ble_;
    IndicationController* indications_;
    State* state_;
    TwinStateHandler on_twin_;
    DisconnectHandler on_disconnect_;
    LinkHandler on_connecting_;
    LinkHandler on_paired_;

    uint8_t device_uuid_[16]{};
    uint8_t master_uuid_[16]{};
    uint16_t next_msg_id_ = 1;
    bool paired_ = false;
    bool encrypt_enabled_ = false;
    uint8_t session_key_[32]{};
    uint8_t factory_secret_[32]{};
    bool has_factory_secret_ = false;

    QueueHandle_t log_queue_ = nullptr;
    bool status_cccd_seen_ = false;
    BleHeartbeat heartbeat_;

    /** Twin-state apply deferred off NimBLE GATT/host onto process()/app task. */
    volatile bool pending_twin_apply_ = false;
    ilss::TwinState pending_twin_{};
    ilss::Packet pending_twin_req_{};

    void onWrite(uint16_t handle, const uint8_t* data, size_t len);
    void onConnection(bool connected, uint16_t conn);
    void handleCommandPacket(const ilss::Packet& pkt);
    void handleInboundAck(const ilss::Packet& pkt);
    void drainPendingTwinApply();
    void failHeartbeat(const char* why);
    void sendAck(const ilss::Packet& req, bool nak, uint8_t reason = 0);
    void sendPacket(ilss::Packet& pkt, bool as_event);
    bool loadProvisioning();
    static uint32_t nowMs();
};
