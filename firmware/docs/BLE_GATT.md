# ILSS Lanyard BLE GATT

Advertising name: `ILSS-LY-<serial-suffix>`  
Filter Web Bluetooth `requestDevice` on the Twin Control service UUID.

Base UUID namespace (example — implemented in code as constants):

```
xxxxxxxx-494c-5353-4c59-0000000000yy
```

(`ILSS` / `LY` embedded in the UUID for uniqueness.)

## Product Metadata Service

UUID: `...0001`

| Characteristic | Props | Description |
|----------------|-------|-------------|
| SerialNumber | Read | UTF-8 serial |
| ModelVersion | Read | Hardware / model string |
| SoftwareVersion | Read | App version (`esp_app_desc`) |
| Brand | Read | `uint8` enum (`1` = Honeywell) |
| BatteryLevel | Read | `uint8` stub from State (0–100) |

## Twin Control Service

UUID: `...0010`

| Characteristic | Props | Description |
|----------------|-------|-------------|
| Command | Write / Write Without Response | Packed protocol frames (master → slave) |
| Event | Notify | Packed protocol frames (slave → master) |
| Status | Read + Notify | Current 6-byte `TwinState` |
| Pairing | Write + Notify | Pair / unpair / key exchange |
| Log | Notify | UTF-8 log lines mirrored from `Logger` (subscribe via CCCD) |

### Log characteristic

- Plain UTF-8 text (not packed protocol)
- Truncated to ATT MTU when needed
- Fan-out from `Logger` is non-blocking; drops under backpressure
- USB serial logging always continues regardless of BLE subscription
