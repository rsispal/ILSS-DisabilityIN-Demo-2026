#pragma once

#include <cstdint>
#include <string>

/** Minimal stub — beacon scanning removed from digital-twin build. */
class BLEBeacon {
public:
    std::string getId() const { return id_; }
    int8_t getRssi() const { return rssi_; }
    uint32_t getLastSeenMs() const { return last_seen_ms_; }
    void setId(const std::string& id) { id_ = id; }
    void setRssi(int8_t rssi) { rssi_ = rssi; }
    void setLastSeenMs(uint32_t ms) { last_seen_ms_ = ms; }

private:
    std::string id_;
    int8_t rssi_ = 0;
    uint32_t last_seen_ms_ = 0;
};
