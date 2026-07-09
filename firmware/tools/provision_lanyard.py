#!/usr/bin/env python3
"""Provision ILSS lanyard ble_prov partition and optionally flash the app.

Layout of ble_prov (0x1000 bytes):
  0x00: magic "ILSS" (4)
  0x04: device UUIDv7 (16)
  0x14: serial UTF-8 (32, NUL-padded)
  0x34: brand uint8 (1 = Honeywell)
  0x35: factory secret (32)
  rest: 0xFF
"""

from __future__ import annotations

import argparse
import os
import secrets
import struct
import subprocess
import sys
import tempfile
import uuid
from pathlib import Path

MAGIC = b"ILSS"
PART_SIZE = 0x1000
DEFAULT_OFFSET = 0x1E0000


def uuidv7_bytes() -> bytes:
    """Generate a UUIDv7-ish value (time-ordered) as 16 raw bytes (RFC order)."""
    u = uuid.uuid4()
    # Prefer uuid7 if available (3.13+); else uuid4 with version nibble forced
    try:
        u = uuid.uuid7()  # type: ignore[attr-defined]
    except AttributeError:
        b = bytearray(u.bytes)
        b[6] = (b[6] & 0x0F) | 0x70
        b[8] = (b[8] & 0x3F) | 0x80
        return bytes(b)
    return u.bytes


def build_blob(serial: str, brand: int, secret: bytes, device_uuid: bytes | None = None) -> bytes:
    if len(secret) != 32:
        raise ValueError("secret must be 32 bytes")
    serial_b = serial.encode("utf-8")[:32].ljust(32, b"\0")
    uid = device_uuid or uuidv7_bytes()
    if len(uid) != 16:
        raise ValueError("uuid must be 16 bytes")
    blob = bytearray(b"\xff" * PART_SIZE)
    blob[0:4] = MAGIC
    blob[4:20] = uid
    blob[20:52] = serial_b
    blob[52] = brand & 0xFF
    blob[53:85] = secret
    return bytes(blob)


def main() -> int:
    p = argparse.ArgumentParser(description="Provision ILSS lanyard ble_prov partition")
    p.add_argument("--port", required=True, help="Serial port (e.g. /dev/ttyACM0)")
    p.add_argument("--serial", required=True, help="Device serial string")
    p.add_argument("--brand", type=int, default=1, help="Brand enum (1=Honeywell)")
    p.add_argument("--secret", default=None, help="64-char hex factory secret (random if omitted)")
    p.add_argument("--uuid", default=None, help="32-char hex device UUID (random UUIDv7 if omitted)")
    p.add_argument("--offset", type=lambda x: int(x, 0), default=DEFAULT_OFFSET)
    p.add_argument("--baud", type=int, default=921600)
    p.add_argument("--flash-app", action="store_true", help="Also run idf.py flash before writing prov")
    p.add_argument("--out", default=None, help="Write blob to file instead of/in addition to flash")
    args = p.parse_args()

    if args.secret:
        secret = bytes.fromhex(args.secret)
    else:
        secret = secrets.token_bytes(32)

    device_uuid = bytes.fromhex(args.uuid) if args.uuid else None
    blob = build_blob(args.serial, args.brand, secret, device_uuid)

    print(f"serial={args.serial}")
    print(f"uuid={blob[4:20].hex()}")
    print(f"secret={secret.hex()}")
    print(f"brand={args.brand}")

    if args.out:
        Path(args.out).write_bytes(blob)
        print(f"wrote {args.out}")

    if args.flash_app:
        fw = Path(__file__).resolve().parents[1]
        subprocess.check_call(["idf.py", "-p", args.port, "flash"], cwd=fw)

    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        f.write(blob)
        tmp = f.name

    try:
        cmd = [
            "esptool.py",
            "--chip",
            "esp32s3",
            "--port",
            args.port,
            "--baud",
            str(args.baud),
            "write_flash",
            hex(args.offset),
            tmp,
        ]
        print(" ".join(cmd))
        subprocess.check_call(cmd)
    finally:
        os.unlink(tmp)

    print("Provisioning complete.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
