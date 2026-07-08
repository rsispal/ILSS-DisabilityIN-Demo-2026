import { useState, useRef, useCallback } from 'react';

export type BleStatus = 'disconnected' | 'connecting' | 'connected';

export function useBleConnect() {
  const [status, setStatus] = useState<BleStatus>('disconnected');
  const [name, setName] = useState<string | null>(null);
  const [msg, setMsg] = useState('');
  const deviceRef = useRef<BluetoothDevice | null>(null);

  const supported = typeof navigator !== 'undefined' && !!navigator.bluetooth;

  const onDisconnected = useCallback(() => {
    setStatus('disconnected');
    setName(null);
    setMsg('Device disconnected');
  }, []);

  async function pair() {
    setMsg('');
    if (!navigator.bluetooth) {
      setMsg('Web Bluetooth isn’t available in this browser.');
      return;
    }
    try {
      setStatus('connecting');
      const device = await navigator.bluetooth.requestDevice({
        acceptAllDevices: true,
        optionalServices: ['battery_service', 'device_information'],
      });
      deviceRef.current = device;
      device.addEventListener('gattserverdisconnected', onDisconnected);
      await device.gatt!.connect();
      setName(device.name || 'ILSS Lanyard');
      setStatus('connected');
      setMsg('');
    } catch (err) {
      setStatus('disconnected');
      const error = err as Error & { name?: string };
      if (error?.name === 'NotFoundError') setMsg('No device selected.');
      else if (error?.name === 'SecurityError')
        setMsg('Bluetooth is blocked by the browser’s permissions here.');
      else setMsg(error?.message || 'Pairing failed.');
    }
  }

  function simulate() {
    setMsg('');
    setStatus('connecting');
    setTimeout(() => {
      setName('ILSS-LY · 04A7-22F1');
      setStatus('connected');
    }, 750);
  }

  function disconnect() {
    const d = deviceRef.current;
    try {
      if (d?.gatt?.connected) d.gatt.disconnect();
    } catch {
      /* ignore */
    }
    deviceRef.current = null;
    setStatus('disconnected');
    setName(null);
    setMsg('');
  }

  const dotClass = status === 'connected' ? 'on' : status === 'connecting' ? 'busy' : 'off';
  const chipText =
    status === 'connected'
      ? 'BLE · Connected'
      : status === 'connecting'
        ? 'BLE · Connecting…'
        : 'BLE · Disconnected';

  return {
    status,
    name,
    msg,
    supported,
    dotClass,
    chipText,
    pair,
    simulate,
    disconnect,
  };
}
