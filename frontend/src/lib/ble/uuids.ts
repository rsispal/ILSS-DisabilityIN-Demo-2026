/** ILSS BLE GATT UUIDs — must match firmware BluetoothLowLevelDriver make_uuid().
 *
 * Firmware stores ble_uuid128_t little-endian as:
 *   [svc, 00,00,00, 00,00, 59,4c, 53,53,4c,49, 00,00,00,00]
 * Web Bluetooth UUID string (MSB-first) is the reverse:
 *   00000000-494c-5353-4c59-0000000000{svc}
 */

function uuidFor(svcByte: number): string {
  const hex = svcByte.toString(16).padStart(2, '0');
  return `00000000-494c-5353-4c59-0000000000${hex}`;
}

export const UUID_META_SVC = uuidFor(0x01);
export const UUID_SERIAL = uuidFor(0x02);
export const UUID_MODEL = uuidFor(0x03);
export const UUID_SWVER = uuidFor(0x04);
export const UUID_BRAND = uuidFor(0x05);
export const UUID_BATT = uuidFor(0x06);

export const UUID_TWIN_SVC = uuidFor(0x10);
export const UUID_CMD = uuidFor(0x11);
export const UUID_EVENT = uuidFor(0x12);
export const UUID_STATUS = uuidFor(0x13);
export const UUID_PAIRING = uuidFor(0x14);
export const UUID_LOG = uuidFor(0x15);

export const OPTIONAL_SERVICES = [UUID_META_SVC, UUID_TWIN_SVC];
