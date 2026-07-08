import { useState, useEffect, useRef } from 'react';
import { Button } from '@/ds/forge';
import { BtIcon, ChevIcon } from '@/lib/constants/icons';
import { useBleConnect } from '@/hooks/useBleConnect';

/** Standalone BLE chip — kept for layout experiments; header uses UserAvatarMenu. */
export function BleConnect() {
  const [open, setOpen] = useState(false);
  const wrapRef = useRef<HTMLDivElement>(null);
  const ble = useBleConnect();

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

  let body;
  if (ble.status === 'connected') {
    body = (
      <div>
        <div className="ble-device">
          <span className="ble-device-ic">
            <BtIcon style={{ width: 18, height: 18 }} />
          </span>
          <div className="ble-device-meta">
            <div className="ble-device-name">{ble.name || 'ILSS Lanyard'}</div>
            <div className="ble-device-sub">GATT server connected</div>
          </div>
          <span className="ble-pill ok">Live</span>
        </div>
        <div className="ble-actions">
          <Button
            variant="destructive-secondary"
            size="regular"
            onClick={ble.disconnect}
            style={{ width: '100%' }}
          >
            Disconnect
          </Button>
        </div>
      </div>
    );
  } else {
    const connecting = ble.status === 'connecting';
    body = (
      <div>
        <p className="ble-lead">
          Pair your ILSS lanyard over Bluetooth Low Energy to mirror its live alert state.
        </p>
        <div className="ble-actions">
          <Button
            variant="primary"
            size="regular"
            onClick={ble.pair}
            disabled={connecting}
            style={{ width: '100%' }}
          >
            {connecting ? 'Connecting…' : 'Pair device'}
          </Button>
        </div>
        {!ble.supported && (
          <div className="ble-fallback">
            <span className="ble-note">Web Bluetooth unavailable in this view.</span>
            <Button
              variant="quiet-secondary"
              size="regular"
              onClick={ble.simulate}
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
        <span className={'ble-dot ' + ble.dotClass} />
        <span className="ble-chip-text">{ble.chipText}</span>
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
          {ble.msg && <div className="ble-msg">{ble.msg}</div>}
        </div>
      )}
    </div>
  );
}
