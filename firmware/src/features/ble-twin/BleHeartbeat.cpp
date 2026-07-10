#include "BleHeartbeat.h"

void BleHeartbeat::reset() {
    last_tx_ms_ = 0;
    await_since_ms_ = 0;
    awaiting_ack_ = false;
    pending_msg_id_ = 0;
}

void BleHeartbeat::onPaired(uint32_t now_ms) {
    last_tx_ms_ = now_ms;  // grace period before first poll
    awaiting_ack_ = false;
    pending_msg_id_ = 0;
}

void BleHeartbeat::onDisconnected() {
    reset();
}

void BleHeartbeat::markPollSent(uint16_t msg_id, uint32_t now_ms) {
    pending_msg_id_ = msg_id;
    awaiting_ack_ = true;
    await_since_ms_ = now_ms;
    last_tx_ms_ = now_ms;
}

bool BleHeartbeat::onAck(uint16_t /*code*/, uint16_t msg_id) {
    if (!awaiting_ack_) return false;
    if (msg_id != pending_msg_id_) return false;
    awaiting_ack_ = false;
    return true;
}

void BleHeartbeat::tick(bool active, uint32_t now_ms,
                        const EmitPollFn& emit_poll, const FailFn& fail_fn) {
    if (!active) {
        awaiting_ack_ = false;
        return;
    }
    if (awaiting_ack_) {
        if ((now_ms - await_since_ms_) >= kAckTimeoutMs) {
            if (fail_fn) fail_fn("ACK timeout (4s)");
        }
        return;
    }
    if (last_tx_ms_ == 0 || (now_ms - last_tx_ms_) >= kIntervalMs) {
        if (emit_poll) emit_poll();
    }
}
