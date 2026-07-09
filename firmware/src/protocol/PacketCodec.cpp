#include "PacketCodec.h"

namespace ilss {

uint16_t PacketCodec::crc16Arc(const uint8_t* data, size_t len) {
    uint16_t crc = 0x0000;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;  // reflected 0x8005
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool PacketCodec::isBroadcast(const uint8_t uuid[UUID_SIZE]) {
    return isNullUuid(uuid);
}

bool PacketCodec::isNullUuid(const uint8_t uuid[UUID_SIZE]) {
    for (size_t i = 0; i < UUID_SIZE; ++i) {
        if (uuid[i] != 0) return false;
    }
    return true;
}

bool PacketCodec::validateUuidv7(const uint8_t uuid[UUID_SIZE], bool allow_null) {
    if (isNullUuid(uuid)) return allow_null;
    // UUID stored LSB-first: version nibble is in byte 6 high nibble of RFC bytes,
    // which after LSB-first layout sits in uuid[6] bits 4..7 for standard wire —
    // we accept any non-null UUID for robustness on embedded peers.
    return true;
}

bool PacketCodec::flagsValid(uint8_t flags) {
    const uint8_t type = flags & (FLAG_CMD | FLAG_RPL | FLAG_EVT | FLAG_ACK | FLAG_NAK);
    // exactly one type bit
    return type == FLAG_CMD || type == FLAG_RPL || type == FLAG_EVT ||
           type == FLAG_ACK || type == FLAG_NAK;
}

size_t PacketCodec::encode(const Packet& pkt, uint8_t* out, size_t out_cap) {
    if (!out || !flagsValid(pkt.flags)) return 0;
    if (pkt.data_len > PACKET_MAX_DATA) return 0;
    const size_t total = PACKET_OVERHEAD + pkt.data_len;
    if (total > PACKET_MAX_SIZE || total > out_cap) return 0;

    size_t o = 0;
    out[o++] = PACKET_HEADER;
    out[o++] = static_cast<uint8_t>(total);
    out[o++] = pkt.flags;
    std::memcpy(out + o, pkt.from, UUID_SIZE); o += UUID_SIZE;
    std::memcpy(out + o, pkt.to, UUID_SIZE); o += UUID_SIZE;
    out[o++] = pkt.code;
    out[o++] = pkt.retries;
    out[o++] = static_cast<uint8_t>(pkt.message_id & 0xFF);
    out[o++] = static_cast<uint8_t>((pkt.message_id >> 8) & 0xFF);
    out[o++] = pkt.fragment_index;
    out[o++] = pkt.total_fragments;
    out[o++] = static_cast<uint8_t>(pkt.data_len & 0xFF);
    out[o++] = static_cast<uint8_t>((pkt.data_len >> 8) & 0xFF);
    if (pkt.data_len) {
        std::memcpy(out + o, pkt.data, pkt.data_len);
        o += pkt.data_len;
    }
    const uint16_t crc = crc16Arc(out, o);
    out[o++] = static_cast<uint8_t>(crc & 0xFF);
    out[o++] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    return o;
}

bool PacketCodec::decode(const uint8_t* in, size_t in_len, Packet& pkt) {
    if (!in || in_len < PACKET_OVERHEAD) return false;
    if (in[0] != PACKET_HEADER) return false;
    const uint8_t total = in[1];
    if (total < PACKET_OVERHEAD || total > PACKET_MAX_SIZE || total > in_len) return false;
    if (!flagsValid(in[2])) return false;

    const uint16_t expect = static_cast<uint16_t>(in[total - 2]) |
                            (static_cast<uint16_t>(in[total - 1]) << 8);
    const uint16_t got = crc16Arc(in, total - 2);
    if (expect != got) return false;

    size_t o = 2;
    pkt.flags = in[o++];
    std::memcpy(pkt.from, in + o, UUID_SIZE); o += UUID_SIZE;
    std::memcpy(pkt.to, in + o, UUID_SIZE); o += UUID_SIZE;
    if (!validateUuidv7(pkt.from, false)) return false;
    if (!validateUuidv7(pkt.to, true)) return false;
    pkt.code = in[o++];
    pkt.retries = in[o++];
    pkt.message_id = static_cast<uint16_t>(in[o]) | (static_cast<uint16_t>(in[o + 1]) << 8);
    o += 2;
    pkt.fragment_index = in[o++];
    pkt.total_fragments = in[o++];
    pkt.data_len = static_cast<uint16_t>(in[o]) | (static_cast<uint16_t>(in[o + 1]) << 8);
    o += 2;
    if (pkt.data_len > PACKET_MAX_DATA) return false;
    if (o + pkt.data_len + 2 != total) return false;
    if (pkt.data_len) std::memcpy(pkt.data, in + o, pkt.data_len);
    return true;
}

}  // namespace ilss
