# Pairing & encryption

## Factory provisioning

Partition `ble_prov` (see `partitions.csv`) stores:

- Device UUIDv7 (16 bytes)
- Serial string
- Brand enum
- Long-term secret (32 bytes)

Program with:

```bash
python tools/provision_lanyard.py --port PORT --serial SERIAL [--secret HEX64]
```

## Pairing flow

1. Web connects over Web Bluetooth (link-layer may be unencrypted initially).
2. Write Pairing characteristic: exchange device UUIDv7 + ephemeral Curve25519 public keys (or session nonce fallback).
3. Derive session key via HKDF → AES-256-GCM.
4. Encrypt packed-protocol **Data** fields with the session key (headers remain clear for routing).
5. Unpair clears NVS bond (`ilss_ble` namespace) and stops indications if required.

## Fallback

If browser Web Crypto / Web Bluetooth constraints block full DH, use the factory-provisioned shared secret with a random session nonce exchanged on pair. Firmware still implements Curve25519 for native clients.
