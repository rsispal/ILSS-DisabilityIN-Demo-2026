import {
  useCallback,
  useEffect,
  useLayoutEffect,
  useRef,
  useState,
  type PointerEvent as ReactPointerEvent,
  type UIEvent,
} from 'react';
import { useBleTwin } from '@/context/BleTwinContext';
import { directionLabel } from '@/lib/ble/parseDeviceLog';

interface DeviceLogsSidebarProps {
  open: boolean;
  onClose: () => void;
  width: number;
  onWidthChange: (w: number) => void;
  /** When true, overlay full width (mobile) instead of in-flow drawer */
  overlay?: boolean;
}

const MIN_W = 280;
const MAX_W = 640;
/** Distance from bottom that still counts as “pinned” for auto-scroll. */
const NEAR_BOTTOM_PX = 48;

function formatStamp(ts: number): string {
  const d = new Date(ts);
  const date = d.toLocaleDateString(undefined, {
    month: 'short',
    day: '2-digit',
  });
  const time = d.toLocaleTimeString(undefined, {
    hour12: false,
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
  const ms = String(ts % 1000).padStart(3, '0');
  return `${date} ${time}.${ms}`;
}

export function DeviceLogsSidebar({
  open,
  onClose,
  width,
  onWidthChange,
  overlay = false,
}: DeviceLogsSidebarProps) {
  const { logs, clearLogs, status, name, msg } = useBleTwin();
  const bodyRef = useRef<HTMLDivElement>(null);
  const [copied, setCopied] = useState(false);
  const [helpOpen, setHelpOpen] = useState(false);
  /** Auto-follow new logs; paused when the user scrolls up. */
  const [pinnedToBottom, setPinnedToBottom] = useState(true);
  const ignoreScrollRef = useRef(false);
  const dragRef = useRef<{ startX: number; startW: number } | null>(null);

  const scrollToLatest = useCallback(() => {
    const el = bodyRef.current;
    if (!el) return;
    ignoreScrollRef.current = true;
    el.scrollTop = el.scrollHeight;
    setPinnedToBottom(true);
    requestAnimationFrame(() => {
      ignoreScrollRef.current = false;
    });
  }, []);

  useEffect(() => {
    if (open) setPinnedToBottom(true);
  }, [open]);

  useLayoutEffect(() => {
    if (!open || !pinnedToBottom) return;
    const el = bodyRef.current;
    if (!el) return;
    ignoreScrollRef.current = true;
    el.scrollTop = el.scrollHeight;
    requestAnimationFrame(() => {
      ignoreScrollRef.current = false;
    });
  }, [logs, open, pinnedToBottom]);

  const onScroll = (ev: UIEvent<HTMLDivElement>) => {
    if (ignoreScrollRef.current) return;
    const el = ev.currentTarget;
    const distanceFromBottom = el.scrollHeight - el.scrollTop - el.clientHeight;
    const nearBottom = distanceFromBottom < NEAR_BOTTOM_PX;
    setPinnedToBottom(nearBottom);
  };

  const copyAll = async () => {
    const text = logs
      .map((l) => {
        const dir = directionLabel(l.direction);
        const ids =
          l.msgId != null
            ? ` msg#${l.msgId}${l.reply ? ` ${l.reply.toUpperCase()}` : ''}`
            : '';
        return `${new Date(l.ts).toISOString()}  ${dir}  ${l.eventType}  ${l.summary}${ids}  |  ${l.text}`;
      })
      .join('\n');
    try {
      await navigator.clipboard.writeText(text || '(empty)');
      setCopied(true);
      window.setTimeout(() => setCopied(false), 1500);
    } catch {
      /* ignore */
    }
  };

  const onResizePointerDown = useCallback(
    (ev: ReactPointerEvent<HTMLDivElement>) => {
      if (overlay) return;
      ev.preventDefault();
      dragRef.current = { startX: ev.clientX, startW: width };
      ev.currentTarget.setPointerCapture(ev.pointerId);
    },
    [overlay, width],
  );

  const onResizePointerMove = useCallback(
    (ev: ReactPointerEvent<HTMLDivElement>) => {
      const drag = dragRef.current;
      if (!drag) return;
      // Handle is on the left edge — drag left = wider
      const next = Math.min(MAX_W, Math.max(MIN_W, drag.startW + (drag.startX - ev.clientX)));
      onWidthChange(next);
    },
    [onWidthChange],
  );

  const onResizePointerUp = useCallback((ev: ReactPointerEvent<HTMLDivElement>) => {
    if (!dragRef.current) return;
    dragRef.current = null;
    try {
      ev.currentTarget.releasePointerCapture(ev.pointerId);
    } catch {
      /* ignore */
    }
  }, []);

  if (!open) return null;

  return (
    <aside
      className={
        'device-logs-sidebar' + (overlay ? ' device-logs-sidebar--overlay' : '')
      }
      style={overlay ? undefined : { width }}
      aria-label="Device logs"
    >
      {!overlay && (
        <div
          className="device-logs-resize"
          role="separator"
          aria-orientation="vertical"
          aria-label="Resize logs drawer"
          aria-valuenow={Math.round(width)}
          aria-valuemin={MIN_W}
          aria-valuemax={MAX_W}
          onPointerDown={onResizePointerDown}
          onPointerMove={onResizePointerMove}
          onPointerUp={onResizePointerUp}
          onPointerCancel={onResizePointerUp}
        />
      )}
      <div className="device-logs-head">
        <div>
          <div className="device-logs-title">Device logs</div>
          <div className="device-logs-sub">
            {status === 'connected' ? name || 'Connected' : 'Waiting for BLE…'} ·{' '}
            {logs.length}
          </div>
        </div>
        <div className="device-logs-actions">
          <button
            type="button"
            className={'device-logs-btn' + (helpOpen ? ' is-active' : '')}
            onClick={() => setHelpOpen((v) => !v)}
            aria-expanded={helpOpen}
            aria-controls="device-logs-help"
          >
            Help
          </button>
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
      <div className="device-logs-body-wrap">
        <div className="device-logs-body" ref={bodyRef} onScroll={onScroll}>
          {logs.length === 0 ? (
            <div className="device-logs-empty">
              No events yet. Pair a lanyard — GATT steps, packets, and firmware logs stream here.
            </div>
          ) : (
            logs.map((l) => (
              <article
                key={l.id}
                className={`device-logs-card level-${l.level.toLowerCase()} dir-${l.direction}`}
                title={l.text}
              >
                <div className="device-logs-card-top">
                  <time className="device-logs-card-ts" dateTime={new Date(l.ts).toISOString()}>
                    {formatStamp(l.ts)}
                  </time>
                  <span className={`device-logs-dir dir-${l.direction}`}>
                    {directionLabel(l.direction)}
                  </span>
                  <span className="device-logs-type">{l.eventType}</span>
                </div>
                <div className="device-logs-card-bot">
                  <span className="device-logs-summary">{l.summary}</span>
                  <span className="device-logs-ids">
                    {l.msgId != null ? (
                      <span className="device-logs-chip msg">#{l.msgId}</span>
                    ) : null}
                    {l.reply ? (
                      <span className={`device-logs-chip reply-${l.reply}`}>
                        {l.reply.toUpperCase()}
                      </span>
                    ) : null}
                    {l.codeHex ? (
                      <span className="device-logs-chip code">0x{l.codeHex}</span>
                    ) : null}
                  </span>
                </div>
              </article>
            ))
          )}
        </div>
        {!pinnedToBottom && !helpOpen ? (
          <button
            type="button"
            className="device-logs-jump-latest"
            onClick={scrollToLatest}
          >
            Return to latest logs
          </button>
        ) : null}
        {helpOpen ? (
          <div
            id="device-logs-help"
            className="device-logs-help"
            role="dialog"
            aria-label="How to read device logs"
          >
            <div className="device-logs-help-head">
              <div className="device-logs-help-title">How to read these logs</div>
              <button
                type="button"
                className="device-logs-btn"
                onClick={() => setHelpOpen(false)}
              >
                Done
              </button>
            </div>
            <div className="device-logs-help-body">
              <p className="device-logs-help-lead">
                Three layers show up in names and tags — they are not the same thing.
              </p>

              <section className="device-logs-help-section">
                <h3>Stack</h3>
                <dl className="device-logs-help-dl">
                  <div>
                    <dt>DigitalTwinApp</dt>
                    <dd>
                      Firmware app on the lanyard: buttons, pairing policy, “alive” heartbeat.
                      Orchestrates everything else.
                    </dd>
                  </div>
                  <div>
                    <dt>BleTwin</dt>
                    <dd>
                      BLE transport on the device (and <code>(ble)</code> on the web): GATT,
                      pairing, encrypt/decrypt, command/event packets, keep-alive polls.
                    </dd>
                  </div>
                  <div>
                    <dt>TwinState</dt>
                    <dd>
                      The shared 7-byte desired state: alert, color, LED mode, haptic, buzzer,
                      brightness. What the web sends when you change the twin UI.
                    </dd>
                  </div>
                  <div>
                    <dt>Indication</dt>
                    <dd>
                      Applies TwinState to LEDs, buzzer, and haptics on the hardware.
                    </dd>
                  </div>
                </dl>
              </section>

              <section className="device-logs-help-section">
                <h3>Directions</h3>
                <dl className="device-logs-help-dl">
                  <div>
                    <dt>WEB → LY</dt>
                    <dd>Browser sent something to the lanyard.</dd>
                  </div>
                  <div>
                    <dt>LY → WEB</dt>
                    <dd>Lanyard notified or replied to the browser.</dd>
                  </div>
                  <div>
                    <dt>LOCAL</dt>
                    <dd>Browser-only step (picker, GATT connect, client parse).</dd>
                  </div>
                </dl>
              </section>

              <section className="device-logs-help-section">
                <h3>Common event types</h3>
                <dl className="device-logs-help-dl">
                  <div>
                    <dt>GATT / PAIR</dt>
                    <dd>Connect, characteristics, and session pairing.</dd>
                  </div>
                  <div>
                    <dt>TWIN</dt>
                    <dd>Web is pushing a TwinState (desired look/feel).</dd>
                  </div>
                  <div>
                    <dt>STATUS</dt>
                    <dd>Device publishing its current TwinState back.</dd>
                  </div>
                  <div>
                    <dt>ACK / NAK</dt>
                    <dd>Reply to a numbered command (<code>msg#</code>).</dd>
                  </div>
                  <div>
                    <dt>HB</dt>
                    <dd>Keep-alive poll — not a TwinState change.</dd>
                  </div>
                  <div>
                    <dt>FW</dt>
                    <dd>
                      Raw firmware log line; tag in parentheses is the source module
                      (BleTwin, DigitalTwinApp, Indication…).
                    </dd>
                  </div>
                </dl>
              </section>

              <p className="device-logs-help-foot">
                Tip: hover a card for the full raw line. Scroll up to pause auto-follow; use
                “Return to latest logs” to catch up again.
              </p>
            </div>
          </div>
        ) : null}
      </div>
    </aside>
  );
}
