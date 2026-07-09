#pragma once

#include <cstdint>
#include <functional>
#include "../../protocol/PacketCodec.h"
#include "../../protocol/TwinState.h"
#include "../../utils/Logger.h"

class BluetoothLowLevelDriver;
class IndicationController;
class State;

/**
 * BleTwin - GATT twin control: packet RX/TX, status, pairing, log notify.
 */
class BleTwin {
    const char* TAG = "BleTwin";

public:
    using TwinStateHandler = std::function<bool(const ilss::TwinState& desired, ilss::TwinNakReason* reason)>;
    using DisconnectHandler = std::function<void()>;

    BleTwin(Logger* logger, BluetoothLowLevelDriver* ble, IndicationController* indications, State* state);

    bool begin(const char* adv_name);
    void process();  // drain log queue, etc.

    void setDeviceUuid(const uint8_t uuid[16]);
    void setMasterUuid(const uint8_t uuid[16]);
    void setTwinStateHandler(TwinStateHandler h) { on_twin_ = std::move(h); }
    void setDisconnectHandler(DisconnectHandler h) { on_disconnect_ = std::move(h); }

    void publishStatus(const ilss::TwinState& state);
    void emitTwinEvent(const ilss::TwinState& state);
    void notifyLogLine(const char* line);

    bool isPaired() const { return paired_; }
    bool isConnected() const;

    /** Pairing: op 0x01=pair_req, 0x02=pair_ok, 0x03=unpair, 0x10=dh_pub */
    void handlePairingWrite(const uint8_t* data, size_t len);

    // Session crypto (AES-256-GCM key after pair)
    bool encryptEnabled() const { return encrypt_enabled_ && paired_; }
    const uint8_t* sessionKey() const { return session_key_; }

private:
    Logger* logger_;
    BluetoothLowLevelDriver* ble_;
    IndicationController* indications_;
    State* state_;
    TwinStateHandler on_twin_;
    DisconnectHandler on_disconnect_;

    uint8_t device_uuid_[16]{};
    uint8_t master_uuid_[16]{};
    uint16_t next_msg_id_ = 1;
    bool paired_ = false;
    bool encrypt_enabled_ = false;
    uint8_t session_key_[32]{};
    uint8_t factory_secret_[32]{};
    bool has_factory_secret_ = false;

    void onWrite(uint16_t handle, const uint8_t* data, size_t len);
    void onConnection(bool connected, uint16_t conn);
    void handleCommandPacket(const ilss::Packet& pkt);
    void sendAck(const ilss::Packet& req, bool nak, uint8_t reason = 0);
    void sendPacket(ilss::Packet& pkt, bool as_event);
    bool loadProvisioning();
};
