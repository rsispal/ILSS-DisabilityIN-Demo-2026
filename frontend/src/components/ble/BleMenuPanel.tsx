import { Button } from '@/ds/forge';
import { BtIcon } from '@/lib/constants/icons';
import type { BleStatus } from '@/hooks/useBleConnect';

interface BleMenuPanelProps {
  status: BleStatus;
  name: string | null;
  msg: string;
  supported: boolean;
  onPair: () => void;
  onSimulate: () => void;
  onDisconnect: () => void;
}

export function BleMenuPanel({
  status,
  name,
  msg,
  supported,
  onPair,
  onSimulate,
  onDisconnect,
}: BleMenuPanelProps) {
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
            onClick={onDisconnect}
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
            onClick={onPair}
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
              onClick={onSimulate}
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
    <div className="ble-menu-panel">
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
  );
}
