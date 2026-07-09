# Pairing & encryption

## Factory provisioning

Before pairing can use a long-term secret, program the `ble_prov` partition once per unit.

**Full when/how guide:** [PROVISIONING.md](PROVISIONING.md)

```bash
# From firmware/
./tools/provision-lanyard.sh -port /dev/ttyACM0 -serial ILSS-LY-0001
```

That writes device UUIDv7, serial, brand, and a 32-byte factory secret. Day-to-day app flashes do not wipe this partition.

## Pairing flow

1. Web connects over Web Bluetooth (link-layer may be unencrypted initially).
2. Write Pairing characteristic: exchange device UUIDv7 + session nonce (and optional Curve25519 material).
3. Derive session key via HKDF(factory_secret, nonce) → AES-256-GCM key material.
4. Encrypt packed-protocol **Data** fields with the session key (headers remain clear for routing).
5. Unpair clears the session / NVS bond (`ilss_ble`) as implemented on the device.

## Fallback

If `ble_prov` is empty, firmware boots with defaults and can still sync twin state without encryption. Prefer provisioning for demos that need a stable serial and paired secret.
