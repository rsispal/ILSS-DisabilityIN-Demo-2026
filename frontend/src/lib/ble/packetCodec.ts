export const PACKET_HEADER = 0xaa;
export const PACKET_OVERHEAD = 45; // 43 fixed fields + 2 CRC
export const PACKET_MAX_DATA = 192;
export const UUID_SIZE = 16;

export const FLAG_CMD = 1 << 0;
export const FLAG_RPL = 1 << 1;
export const FLAG_EVT = 1 << 2;
export const FLAG_ACK = 1 << 3;
export const FLAG_FRAG = 1 << 4;
export const FLAG_FIN = 1 << 5;
export const FLAG_NAK = 1 << 6;

export interface Packet {
  flags: number;
  from: Uint8Array;
  to: Uint8Array;
  code: number;
  retries: number;
  messageId: number;
  fragmentIndex: number;
  totalFragments: number;
  data: Uint8Array;
}

export function crc16Arc(data: Uint8Array): number {
  let crc = 0;
  for (let i = 0; i < data.length; i++) {
    crc ^= data[i];
    for (let b = 0; b < 8; b++) {
      crc = crc & 1 ? (crc >>> 1) ^ 0xa001 : crc >>> 1;
    }
  }
  return crc & 0xffff;
}

export function nullUuid(): Uint8Array {
  return new Uint8Array(UUID_SIZE);
}

export function isNullUuid(u: Uint8Array): boolean {
  return u.every((b) => b === 0);
}

function flagsValid(flags: number): boolean {
  const type = flags & (FLAG_CMD | FLAG_RPL | FLAG_EVT | FLAG_ACK | FLAG_NAK);
  return (
    type === FLAG_CMD ||
    type === FLAG_RPL ||
    type === FLAG_EVT ||
    type === FLAG_ACK ||
    type === FLAG_NAK
  );
}

export function encodePacket(pkt: Packet): Uint8Array | null {
  if (!flagsValid(pkt.flags) || pkt.data.length > PACKET_MAX_DATA) return null;
  const total = PACKET_OVERHEAD + pkt.data.length;
  const out = new Uint8Array(total);
  let o = 0;
  out[o++] = PACKET_HEADER;
  out[o++] = total;
  out[o++] = pkt.flags;
  out.set(pkt.from, o);
  o += UUID_SIZE;
  out.set(pkt.to, o);
  o += UUID_SIZE;
  out[o++] = pkt.code;
  out[o++] = pkt.retries;
  out[o++] = pkt.messageId & 0xff;
  out[o++] = (pkt.messageId >> 8) & 0xff;
  out[o++] = pkt.fragmentIndex;
  out[o++] = pkt.totalFragments;
  out[o++] = pkt.data.length & 0xff;
  out[o++] = (pkt.data.length >> 8) & 0xff;
  out.set(pkt.data, o);
  o += pkt.data.length;
  const crc = crc16Arc(out.subarray(0, o));
  out[o++] = crc & 0xff;
  out[o++] = (crc >> 8) & 0xff;
  return out;
}

export function decodePacket(buf: Uint8Array): Packet | null {
  if (buf.length < PACKET_OVERHEAD || buf[0] !== PACKET_HEADER) return null;
  const total = buf[1];
  if (total < PACKET_OVERHEAD || total > buf.length) return null;
  if (!flagsValid(buf[2])) return null;
  const expect = buf[total - 2] | (buf[total - 1] << 8);
  const got = crc16Arc(buf.subarray(0, total - 2));
  if (expect !== got) return null;

  let o = 2;
  const flags = buf[o++];
  const from = buf.slice(o, o + UUID_SIZE);
  o += UUID_SIZE;
  const to = buf.slice(o, o + UUID_SIZE);
  o += UUID_SIZE;
  const code = buf[o++];
  const retries = buf[o++];
  const messageId = buf[o] | (buf[o + 1] << 8);
  o += 2;
  const fragmentIndex = buf[o++];
  const totalFragments = buf[o++];
  const dataLen = buf[o] | (buf[o + 1] << 8);
  o += 2;
  if (dataLen > PACKET_MAX_DATA || o + dataLen + 2 !== total) return null;
  const data = buf.slice(o, o + dataLen);
  return { flags, from, to, code, retries, messageId, fragmentIndex, totalFragments, data };
}

export function randomUuidBytes(): Uint8Array {
  const u = new Uint8Array(16);
  crypto.getRandomValues(u);
  // version 7-ish / variant bits
  u[6] = (u[6] & 0x0f) | 0x70;
  u[8] = (u[8] & 0x3f) | 0x80;
  return u;
}
