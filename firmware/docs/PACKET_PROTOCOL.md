# Bidirectional Master-Slave Packet Protocol

Independent-node model: web (master) and lanyard (slave) both send commands, events, replies, and acknowledgments. Application semantics live in a shared **`TwinState`** payload — there are no per-action codes such as “personal alert raised locally”.

## Packet structure

| Field | Size | Description |
|-------|------|-------------|
| Frame Header | 1 | `0xAA` |
| Packet Length | 1 | Total bytes including header and CRC |
| Packet Flags | 1 | CMD / RPL / EVT / ACK / FRAG / FIN / NAK |
| From Address | 16 | Sender UUIDv7, LSB-first |
| To Address | 16 | Receiver UUIDv7, LSB-first; all-zero = broadcast (master only) |
| Cmd/Event Code | 1 | Application code |
| Retries Remaining | 1 | Countdown (0 in ACK/NAK) |
| Message ID | 2 | LSB-first |
| Fragment Index | 1 | Zero-based |
| Total Fragments | 1 | Total count |
| Data Length | 2 | LSB-first |
| Data | 0–192 | Payload |
| CRC-16-ARC | 2 | Entire packet except CRC |

Max packet size: **240** bytes. Protocol overhead: **45** bytes (43 fixed + 2 CRC).

## Packet flags

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | CMD | Command |
| 1 | RPL | Reply |
| 2 | EVT | Event |
| 3 | ACK | Acknowledgment |
| 4 | FRAG | Fragment |
| 5 | FIN | Final fragment |
| 6 | NAK | Negative acknowledgment |
| 7 | — | Reserved |

Exactly one of CMD, RPL, EVT, ACK, or NAK must be set.

## Application codes

| Code | Flags | Meaning |
|------|-------|---------|
| `0x01` | CMD | Apply / request twin state (`TwinState` in Data) |
| `0x01` | EVT | Announce twin state change |
| `0x02` | CMD | Heartbeat / poll (reply carries current `TwinState`) |
| `0x40` | CMD/EVT | Pairing / unpair / DH |

ACK / NAK / RPL use **flags**, not extra codes. NAK Data may carry a reason byte.

## TwinState payload (6 bytes)

| Offset | Field | Values |
|--------|-------|--------|
| 0 | alert | `0=none`, `1=personal`, `2=fire` |
| 1 | color | enum (red, green, blue, teal, purple, yellow, orange) |
| 2 | led | enum (solid, flash, pulse, double, alt, half, chase, off) |
| 3 | haptic | enum (solid, pulse1, pulse2, continuous, click, off) |
| 4 | buzzer | enum matching web patterns |
| 5 | flags | bit0 = advanced override |

Each node enforces mutual exclusion (fire vs personal) and “advanced only when idle” locally before applying or emitting.

## CRC-16-ARC

- Polynomial `0x8005`, init `0x0000`, reflect in/out, final XOR `0x0000`
- Covers Frame Header through end of Data
