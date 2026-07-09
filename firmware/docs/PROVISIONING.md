# Factory provisioning (`ble_prov`)

## When to use this

| Situation | Re-provision? |
|-----------|----------------|
| First bring-up of a new board | **Yes** — once |
| Day-to-day `idf.py flash` of the app | **No** — `ble_prov` is a separate partition |
| Full chip erase / `esptool erase_flash` | **Yes** — partition was wiped |
| Replacing the ESP32 module | **Yes** — new silicon |
| Demo without pairing encryption | Optional — firmware falls back to defaults if `ble_prov` is empty |

You do **not** need Python. Use the **Go CLI** (or the bash wrapper that runs it).

## What gets written

Partition name: `ble_prov` (see [`partitions.csv`](../partitions.csv))  
Offset: `0x1E0000` · Size: `0x1000` (4 KiB)

| Offset | Field | Size |
|--------|-------|------|
| 0x00 | Magic `ILSS` | 4 |
| 0x04 | Device UUIDv7 | 16 |
| 0x14 | Serial (UTF-8, NUL-padded) | 32 |
| 0x34 | Brand enum (`1` = Honeywell) | 1 |
| 0x35 | Factory secret | 32 |
| rest | `0xFF` | — |

Firmware reads this at boot ([`BleTwin::loadProvisioning`](../src/features/ble-twin/BleTwin.cpp)). The secret is used to derive the BLE session key after pairing (see [PAIRING.md](PAIRING.md)).

## How to use

### Prerequisites

- ESP-IDF environment (`esptool.py` / `esptool` on `PATH`)
- Go 1.21+ ([go.dev/dl](https://go.dev/dl/))
- Board connected over USB serial

### Recommended: bash wrapper

From the `firmware/` directory:

```bash
chmod +x tools/provision-lanyard.sh

# Program identity + random secret onto the board
./tools/provision-lanyard.sh -port /dev/ttyACM0 -serial ILSS-LY-0001

# Flash the application first, then provision
./tools/provision-lanyard.sh -port /dev/ttyACM0 -serial ILSS-LY-0001 -flash-app

# Build the 4 KiB image only (no serial flash)
./tools/provision-lanyard.sh -serial ILSS-LY-0001 -out-only -out ble_prov.bin
```

### Equivalent: Go directly

```bash
cd firmware
go run ./tools/provision-lanyard -port /dev/ttyACM0 -serial ILSS-LY-0001
```

### Useful flags

| Flag | Meaning |
|------|---------|
| `-port` | Serial device (required unless `-out-only`) |
| `-serial` | Human-readable serial (required) |
| `-secret` | Optional 64-char hex; random if omitted — **save it** if you need to re-derive keys offline |
| `-uuid` | Optional 32-char hex; random UUIDv7 if omitted |
| `-brand` | Default `1` (Honeywell) |
| `-flash-app` | Run `idf.py flash` before writing `ble_prov` |
| `-out` / `-out-only` | Write blob to a file (CI / offline) |

The tool prints `serial`, `uuid`, and `secret` on success. Store the secret somewhere safe if you care about re-pairing without re-flashing the partition.

### Verify

1. `idf.py -p PORT monitor`
2. Look for a log line like `Loaded provisioned serial=ILSS-LY-0001`
3. BLE should advertise using that serial / `ILSS-LY-…` name
4. On the web app, pair the device; Pairing characteristic should complete

## Typical factory sequence

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
./tools/provision-lanyard.sh -port /dev/ttyACM0 -serial ILSS-LY-0042 -flash-app
idf.py -p /dev/ttyACM0 monitor
```

## Related

- [PAIRING.md](PAIRING.md) — runtime pair / unpair / encryption
- [BLE_GATT.md](BLE_GATT.md) — metadata characteristics populated from this data
