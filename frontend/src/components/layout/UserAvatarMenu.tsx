import { useEffect, useRef, useState } from 'react';
import { Avatar } from '@/ds/forge';
import { BleConnectModal } from '@/components/ble/BleConnectModal';
import { useAppContext } from '@/context/AppContext';
import { useBleConnect } from '@/hooks/useBleConnect';
import { BtIcon, ChevIcon } from '@/lib/constants/icons';

export function UserAvatarMenu() {
  const { profile } = useAppContext();
  const ble = useBleConnect();
  const [open, setOpen] = useState(false);
  const [bleOpen, setBleOpen] = useState(false);
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

  const openBle = () => {
    setOpen(false);
    setBleOpen(true);
  };

  const connected = ble.status === 'connected';

  return (
    <>
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

            <button type="button" className="user-avatar-ble-row" onClick={openBle}>
              <span className={'user-avatar-ble-row-ic' + (connected ? ' live' : '')}>
                <BtIcon style={{ width: 15, height: 15 }} />
              </span>
              <span className="user-avatar-ble-row-copy">
                <span className="user-avatar-ble-row-title">
                  {connected ? 'Manage Bluetooth' : 'Connect device'}
                </span>
                <span className="user-avatar-ble-row-sub">
                  {connected
                    ? ble.name || 'ILSS lanyard linked'
                    : 'Pair an ILSS smart lanyard'}
                </span>
              </span>
              <ChevIcon className="user-avatar-ble-row-chev" style={{ width: 14, height: 14 }} />
            </button>
          </div>
        )}
      </div>

      {bleOpen && <BleConnectModal onClose={() => setBleOpen(false)} />}
    </>
  );
}
