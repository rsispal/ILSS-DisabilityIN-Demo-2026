import { useBleTwin } from '@/context/BleTwinContext';

interface LanyardDeviceSummaryProps {
  className?: string;
}

export function LanyardDeviceSummary({ className }: LanyardDeviceSummaryProps) {
  const ble = useBleTwin();
  const meta = ble.metadata;
  const linkClass =
    ble.status === 'connected' ? 'on' : ble.status === 'connecting' ? 'busy' : 'off';
  const linkLabel =
    ble.status === 'connected'
      ? 'Connected'
      : ble.status === 'connecting'
        ? 'Connecting'
        : 'Disconnected';

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
    </div>
  );
}
