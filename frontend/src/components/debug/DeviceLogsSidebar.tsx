import { useEffect, useRef, useState } from 'react';
import { useBleTwin } from '@/context/BleTwinContext';

interface DeviceLogsSidebarProps {
  open: boolean;
  onClose: () => void;
}

export function DeviceLogsSidebar({ open, onClose }: DeviceLogsSidebarProps) {
  const { logs, clearLogs, status, name, msg } = useBleTwin();
  const bodyRef = useRef<HTMLDivElement>(null);
  const [copied, setCopied] = useState(false);
  const stickBottom = useRef(true);

  useEffect(() => {
    if (!open || !bodyRef.current || !stickBottom.current) return;
    bodyRef.current.scrollTop = bodyRef.current.scrollHeight;
  }, [logs, open]);

  if (!open) return null;

  const onScroll = () => {
    const el = bodyRef.current;
    if (!el) return;
    const nearBottom = el.scrollHeight - el.scrollTop - el.clientHeight < 48;
    stickBottom.current = nearBottom;
  };

  const copyAll = async () => {
    const text = logs.map((l) => {
      const t = new Date(l.ts).toISOString();
      return `${t}  ${l.text}`;
    }).join('\n');
    try {
      await navigator.clipboard.writeText(text || '(empty)');
      setCopied(true);
      window.setTimeout(() => setCopied(false), 1500);
    } catch {
      /* ignore */
    }
  };

  return (
    <aside className="device-logs-sidebar" aria-label="Device logs">
      <div className="device-logs-head">
        <div>
          <div className="device-logs-title">BLE / device logs</div>
          <div className="device-logs-sub">
            {status === 'connected' ? name || 'Connected' : 'Waiting for BLE…'} · {logs.length} lines
          </div>
        </div>
        <div className="device-logs-actions">
          <button type="button" className="device-logs-btn" onClick={copyAll}>
            {copied ? 'Copied' : 'Copy'}
          </button>
          <button type="button" className="device-logs-btn" onClick={clearLogs}>
            Clear
          </button>
          <button type="button" className="device-logs-btn" onClick={onClose}>
            Close
          </button>
        </div>
      </div>
      {msg ? <div className="device-logs-banner">{msg}</div> : null}
      <div className="device-logs-body" ref={bodyRef} onScroll={onScroll}>
        {logs.length === 0 ? (
          <div className="device-logs-empty">
            No log lines yet. Pair a lanyard — client GATT steps and firmware Log notifies stream here.
          </div>
        ) : (
          logs.map((l) => (
            <div key={l.id} className={`device-logs-line level-${(l.text[0] || 'I').toLowerCase()}`}>
              <span className="device-logs-ts">
                {new Date(l.ts).toLocaleTimeString(undefined, { hour12: false })}
                .{String(l.ts % 1000).padStart(3, '0')}
              </span>
              <span className="device-logs-text">{l.text}</span>
            </div>
          ))
        )}
      </div>
    </aside>
  );
}
