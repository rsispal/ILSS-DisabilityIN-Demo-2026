import { useState, useEffect, useRef, useCallback } from 'react';
import { Button } from '@/ds/forge';
import { BtIcon, ChevIcon } from '@/lib/constants/icons';

type BleStatus = 'disconnected' | 'connecting' | 'connected';

export function BleConnect() {
  const [open, setOpen] = useState(false);
  const [status, setStatus] = useState<BleStatus>('disconnected');
  const [name, setName] = useState<string | null>(null);
  const [msg, setMsg] = useState('');
  const deviceRef = useRef<BluetoothDevice | null>(null);
  const wrapRef = useRef<HTMLDivElement>(null);

  const supported = typeof navigator !== 'undefined' && !!navigator.bluetooth;

  useEffect(() => {
    if (!open) return undefined;
    const onDoc = (ev: MouseEvent) => {
      if (wrapRef.current && !wrapRef.current.contains(ev.target as Node)) setOpen(false);
    };
    const onKey = (ev: KeyboardEvent) => {
      if (ev.key === 'Escape') setOpen(false);
    };
    document.addEventListener('mousedown', onDoc);
    document.addEventListener('keydown', onKey);
    return () => {
      document.removeEventListener('mousedown', onDoc);
      document.removeEventListener('keydown', onKey);
    };
  }, [open]);

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

  let body;
  if (status === 'connected') {
    body = (
      <div>
        <div className="ble-device">
          <span className="ble-device-ic">
            <BtIcon style={{ width: 18, height: 18 }} />
          </span>
          <div className="ble-device-meta">
            <div className="ble-device-name">{name || 'ILSS Lanyard'}</div>
            <div className="ble-device-sub">GATT server connected</div>
          </div>
          <span className="ble-pill ok">Live</span>
        </div>
        <div className="ble-actions">
          <Button
            variant="destructive-secondary"
            size="regular"
            onClick={disconnect}
            style={{ width: '100%' }}
          >
            Disconnect
          </Button>
        </div>
      </div>
    );
  } else {
    const connecting = status === 'connecting';
    body = (
      <div>
        <p className="ble-lead">
          Pair your ILSS lanyard over Bluetooth Low Energy to mirror its live alert state.
        </p>
        <div className="ble-actions">
          <Button
            variant="primary"
            size="regular"
            onClick={pair}
            disabled={connecting}
            style={{ width: '100%' }}
          >
            {connecting ? 'Connecting…' : 'Pair device'}
          </Button>
        </div>
        {!supported && (
          <div className="ble-fallback">
            <span className="ble-note">Web Bluetooth unavailable in this view.</span>
            <Button
              variant="quiet-secondary"
              size="regular"
              onClick={simulate}
              disabled={connecting}
              style={{ width: '100%' }}
            >
              Simulate connection
            </Button>
          </div>
        )}
      </div>
    );
  }

  return (
    <div className="ble-wrap" ref={wrapRef}>
      <button
        type="button"
        className="ble-chip"
        onClick={() => setOpen((o) => !o)}
        aria-haspopup="dialog"
        aria-expanded={open}
      >
        <span className={'ble-dot ' + dotClass} />
        <span className="ble-chip-text">{chipText}</span>
        <ChevIcon
          className={'ble-chev' + (open ? ' up' : '')}
          style={{ width: 13, height: 13 }}
        />
      </button>
      {open && (
        <div className="ble-pop" role="dialog" aria-label="Bluetooth connection">
          <div className="ble-pop-head">
            <span className="ble-pop-ic">
              <BtIcon style={{ width: 16, height: 16 }} />
            </span>
            <div>
              <div className="ble-pop-title">Bluetooth</div>
              <div className="ble-pop-sub">ILSS smart lanyard</div>
            </div>
          </div>
          {body}
          {msg && <div className="ble-msg">{msg}</div>}
        </div>
      )}
    </div>
  );
}
