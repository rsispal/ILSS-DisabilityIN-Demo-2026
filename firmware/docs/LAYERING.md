# Layering

Intended stack for the digital-twin firmware build:

```
main
  └─ application/          DigitalTwinApplication, IndicationController, BootSequence
       └─ features/        RGBLED, Buzzer, Haptics, SideButtons, BleTwin, USBCLI
            └─ lowlevel/   drivers (USB, I2C, BLE, buzzer PWM, DRV2605, NVS)
       └─ protocol/        PacketCodec, TwinState (no hardware deps)
       └─ state/           preferences + device identity
       └─ utils/           Logger, JSON, helpers
```

## Rules

1. **`main`** wires boot only (`BootSequence` + `DigitalTwinApplication`). No direct driver calls.
2. **`application/`** owns product semantics (twin apply, link LED modes, boot flourish). Talks to features, not `lowlevel/*Driver` headers.
3. **`features/`** wrap drivers with semantic APIs (`Haptics::play(TwinHaptic)`, `Buzzer::queueSiren`, …). They may use `LowLevel&` / driver accessors.
4. **`lowlevel/`** is ESP-IDF glue. Prefer accessors over public fields (`BluetoothLowLevelDriver::handleCmd()`, `setSerial()`, …).
5. **`protocol/`** stays free of FreeRTOS/hardware includes beyond what encoding needs.

## Retired

- `layers/` — removed (beacon stub was the last remnant).
- Wi-Fi / Azure IoT / beacon scan — not part of this twin build; leftover State NVS keys for Wi-Fi/persona remain for CLI/provisioning compatibility only.
