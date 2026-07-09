#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace ilss {

static constexpr uint8_t PACKET_HEADER = 0xAA;
static constexpr size_t PACKET_OVERHEAD = 45;  // 43 fixed fields + 2 CRC
static constexpr size_t PACKET_MAX_SIZE = 240;
static constexpr size_t PACKET_MAX_DATA = 192;
static constexpr size_t UUID_SIZE = 16;

enum PacketFlag : uint8_t {
    FLAG_CMD = 1 << 0,
    FLAG_RPL = 1 << 1,
    FLAG_EVT = 1 << 2,
    FLAG_ACK = 1 << 3,
    FLAG_FRAG = 1 << 4,
    FLAG_FIN = 1 << 5,
    FLAG_NAK = 1 << 6,
};

struct Packet {
    uint8_t flags = 0;
    uint8_t from[UUID_SIZE]{};
    uint8_t to[UUID_SIZE]{};
    uint8_t code = 0;
    uint8_t retries = 0;
    uint16_t message_id = 0;
    uint8_t fragment_index = 0;
    uint8_t total_fragments = 1;
    uint16_t data_len = 0;
    uint8_t data[PACKET_MAX_DATA]{};
};

class PacketCodec {
public:
    static uint16_t crc16Arc(const uint8_t* data, size_t len);

    static bool isBroadcast(const uint8_t uuid[UUID_SIZE]);
    static bool isNullUuid(const uint8_t uuid[UUID_SIZE]);
    static bool validateUuidv7(const uint8_t uuid[UUID_SIZE], bool allow_null);

    /** Encode packet into out; returns total length or 0 on failure. */
    static size_t encode(const Packet& pkt, uint8_t* out, size_t out_cap);

    /** Decode buffer into pkt; returns true on success. */
    static bool decode(const uint8_t* in, size_t in_len, Packet& pkt);

    static bool flagsValid(uint8_t flags);
};

}  // namespace ilss
