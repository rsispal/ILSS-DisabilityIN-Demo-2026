import { useEffect, useRef, useState } from 'react';
import { Avatar } from '@/ds/forge';
import { BleMenuPanel } from '@/components/ble/BleMenuPanel';
import { useAppContext } from '@/context/AppContext';
import { useBleConnect } from '@/hooks/useBleConnect';
import { ChevIcon } from '@/lib/constants/icons';

export function UserAvatarMenu() {
  const { profile } = useAppContext();
  const ble = useBleConnect();
  const [open, setOpen] = useState(false);
  const wrapRef = useRef<HTMLDivElement>(null);
  const initials = (profile.initials || '').slice(0, 3).toUpperCase() || '?';

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

  return (
    <div className="user-avatar-menu" ref={wrapRef}>
      <button
        type="button"
        className="user-avatar-trigger"
        onClick={() => setOpen((o) => !o)}
        aria-haspopup="menu"
        aria-expanded={open}
        aria-label={`${profile.name} account menu`}
      >
        <span className="user-avatar-stack">
          <Avatar size="md" text={initials} alt={profile.name} />
          <span className={'user-avatar-ble-dot ' + ble.dotClass} aria-hidden />
        </span>
        <ChevIcon
          className={'user-avatar-chev' + (open ? ' up' : '')}
          style={{ width: 13, height: 13 }}
        />
      </button>

      {open && (
        <div className="user-avatar-pop" role="menu" aria-label="Account menu">
          <div className="user-avatar-pop-head">
            <Avatar size="md" text={initials} alt={profile.name} />
            <div className="user-avatar-pop-meta">
              <div className="user-avatar-pop-name">{profile.name}</div>
              <div className="user-avatar-pop-ble">
                <span className={'ble-dot ' + ble.dotClass} />
                {ble.chipText}
              </div>
            </div>
          </div>

          <div className="user-avatar-pop-divider" role="separator" />

          <BleMenuPanel
            status={ble.status}
            name={ble.name}
            msg={ble.msg}
            supported={ble.supported}
            onPair={ble.pair}
            onSimulate={ble.simulate}
            onDisconnect={ble.disconnect}
          />
        </div>
      )}
    </div>
  );
}
