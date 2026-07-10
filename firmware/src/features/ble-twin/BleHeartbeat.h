#pragma once

#include <cstdint>
#include <functional>

/**
 * Device→web heartbeat poll / ACK timeout state machine.
 * Owned by BleTwin; call tick() from process().
 */
class BleHeartbeat {
public:
    using EmitPollFn = std::function<void()>;
    using FailFn = std::function<void(const char* why)>;

    static constexpr uint32_t kIntervalMs = 10000;
    static constexpr uint32_t kAckTimeoutMs = 4000;

    void reset();
    void onPaired(uint32_t now_ms);
    void onDisconnected();

    /** Start waiting for ACK after a poll was transmitted. */
    void markPollSent(uint16_t msg_id, uint32_t now_ms);

    /** Clear await if ACK matches pending msg id. Returns true if matched. */
    bool onAck(uint16_t code, uint16_t msg_id);

    /**
     * Drive interval + timeout. Calls emit_poll when due; fail_fn on timeout.
     * @param active true when paired && connected
     */
    void tick(bool active, uint32_t now_ms, const EmitPollFn& emit_poll, const FailFn& fail_fn);

    bool awaiting() const { return awaiting_ack_; }
    uint16_t pendingMsgId() const { return pending_msg_id_; }

private:
    uint32_t last_tx_ms_ = 0;
    uint32_t await_since_ms_ = 0;
    bool awaiting_ack_ = false;
    uint16_t pending_msg_id_ = 0;
};
