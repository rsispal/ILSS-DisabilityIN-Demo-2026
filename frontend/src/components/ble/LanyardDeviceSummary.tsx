import { Button } from '@/ds/forge';
import { useBleTwin } from '@/context/BleTwinContext';

interface LanyardDeviceSummaryProps {
  className?: string;
  /** Opens the BLE pair / unpair modal (rail only — omit inside the modal itself). */
  onManageConnection?: () => void;
}

export function LanyardDeviceSummary({ className, onManageConnection }: LanyardDeviceSummaryProps) {
  const ble = useBleTwin();
  const meta = ble.metadata;
  const connected = ble.status === 'connected';
  const connecting = ble.status === 'connecting';
  const linkClass = connected ? 'on' : connecting ? 'busy' : 'off';
  const linkLabel = connected ? 'Connected' : connecting ? 'Connecting' : 'Disconnected';
  const actionLabel = connected ? 'Disconnect' : connecting ? 'Manage' : 'Connect';

  return (
    <div
      className={'ble-device-table' + (className ? ` ${className}` : '')}
      aria-label="Device details"
    >
      <div className="ble-device-table-row">
        <span className="ble-device-table-k">Connection</span>
        <span className="ble-device-table-v">
          <span className={'ble-dot ' + linkClass} />
          {linkLabel}
        </span>
      </div>
      <div className="ble-device-table-row">
        <span className="ble-device-table-k">Model</span>
        <span className="ble-device-table-v mono">{meta?.model || '—'}</span>
      </div>
      <div className="ble-device-table-row">
        <span className="ble-device-table-k">Serial</span>
        <span className="ble-device-table-v mono">{meta?.serial || ble.name || '—'}</span>
      </div>
      <div className="ble-device-table-row">
        <span className="ble-device-table-k">Firmware</span>
        <span className="ble-device-table-v mono">{meta?.software || '—'}</span>
      </div>
      {onManageConnection ? (
        <div className="ble-device-table-row ble-device-table-row--action">
          <Button
            variant={connected ? 'destructive-secondary' : connecting ? 'secondary' : 'primary'}
            size="regular"
            onClick={onManageConnection}
            style={{ width: '100%' }}
          >
            {actionLabel}
          </Button>
        </div>
      ) : null}
    </div>
  );
}
