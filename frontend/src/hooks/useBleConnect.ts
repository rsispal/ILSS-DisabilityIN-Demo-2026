import { useBleTwin, type BleTwinStatus } from '@/context/BleTwinContext';

export type BleStatus = BleTwinStatus;

/** Thin adapter so existing BLE menu panels keep working. */
export function useBleConnect() {
  const ble = useBleTwin();
  const dotClass = ble.status === 'connected' ? 'on' : ble.status === 'connecting' ? 'busy' : 'off';
  const chipText =
    ble.status === 'connected'
      ? 'BLE · Connected'
      : ble.status === 'connecting'
        ? 'BLE · Connecting…'
        : 'BLE · Disconnected';

  return {
    status: ble.status,
    name: ble.name,
    msg: ble.msg,
    supported: ble.supported,
    dotClass,
    chipText,
    pair: ble.pair,
    simulate: ble.simulate,
    disconnect: ble.disconnect,
    paired: ble.paired,
    metadata: ble.metadata,
    unpair: ble.unpair,
  };
}
