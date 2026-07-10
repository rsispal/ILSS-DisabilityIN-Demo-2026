# ILSS Lanyard Firmware (BLE Digital Twin)

ESP-IDF firmware for the ILSS smart lanyard demo. The device acts as a **BLE peripheral digital twin** of the web simulator: fire emergency, personal alert, and advanced LED/haptic/buzzer indications stay synchronized over a custom GATT service and packed packet protocol.

## Target

- MCU: ESP32-S3 (Seeed XIAO ESP32-S3 breakout by default — see `src/application/Hardware.h`)
- Framework: ESP-IDF 5.3+
- BLE: NimBLE peripheral (GATT metadata + twin control + log notify)

## Build & flash

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

## Factory provisioning (`ble_prov`)

Program **once per board** (not on every app flash). Uses a small Go CLI — no Python required.

```bash
./tools/provision-lanyard.sh -port /dev/ttyACM0 -serial ILSS-LY-0001
```

See **[docs/PROVISIONING.md](docs/PROVISIONING.md)** for when to re-run, flags, and verification.

## Boot flow

1. Init NVS, USB serial logger, LED boot cue
2. **2 second USB CLI window** — press any key for configure / hardware test / status / NVS reset
3. Power-up cue (star twinkle + soft haptics + ascending beeps), then `DigitalTwinApplication`

## Docs

- [docs/BLE_GATT.md](docs/BLE_GATT.md) — GATT services & characteristics
- [docs/PACKET_PROTOCOL.md](docs/PACKET_PROTOCOL.md) — packed bidirectional frames
- [docs/STATE_MACHINE.md](docs/STATE_MACHINE.md) — link/indication state machine + heartbeat
- [docs/PAIRING.md](docs/PAIRING.md) — pairing, secrets, encryption
- [docs/PROVISIONING.md](docs/PROVISIONING.md) — `ble_prov` / `provision-lanyard` usage

## Layout

```
firmware/
  main/                 ESP-IDF component + Kconfig
  src/
    main.cpp
    application/        DigitalTwinApplication, Hardware.h, IndicationController
    features/           rgb-led, buzzer, side-buttons, ble-twin, usb (slim CLI)
    lowlevel/           i2c, buzzer PWM, haptics, nvs, usb, bluetooth peripheral
    protocol/           PacketCodec + TwinState
    state/              NVS-backed device state
    utils/              Logger (USB serial + optional BLE log fan-out)
  tools/
    provision-lanyard/  Go CLI (preferred)
    provision-lanyard.sh
  docs/
```
