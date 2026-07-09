import { useBleTwin } from '@/context/BleTwinContext';

interface DeviceLogsSidebarProps {
  open: boolean;
  onClose: () => void;
}

export function DeviceLogsSidebar({ open, onClose }: DeviceLogsSidebarProps) {
  const { logs, clearLogs, status, name } = useBleTwin();

  if (!open) return null;

  return (
    <aside className="device-logs-sidebar" aria-label="Device logs">
      <div className="device-logs-head">
        <div>
          <div className="device-logs-title">Device logs</div>
          <div className="device-logs-sub">
            {status === 'connected' ? name || 'Connected' : 'Waiting for BLE…'}
          </div>
        </div>
        <div className="device-logs-actions">
          <button type="button" className="device-logs-btn" onClick={clearLogs}>
            Clear
          </button>
          <button type="button" className="device-logs-btn" onClick={onClose}>
            Close
          </button>
        </div>
      </div>
      <div className="device-logs-body">
        {logs.length === 0 ? (
          <div className="device-logs-empty">No log lines yet. Connect a lanyard and subscribe to stream.</div>
        ) : (
          logs.map((l) => (
            <div key={l.id} className={`device-logs-line level-${(l.text[0] || 'I').toLowerCase()}`}>
              <span className="device-logs-ts">
                {new Date(l.ts).toLocaleTimeString(undefined, { hour12: false })}
              </span>
              <span className="device-logs-text">{l.text}</span>
            </div>
          ))
        )}
      </div>
    </aside>
  );
}
