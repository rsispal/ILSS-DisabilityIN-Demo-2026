import { useState } from 'react';
import { Modal, ModalContent, Button, Control, Label, Input } from '@/ds/forge';
import { useAppContext } from '@/context/AppContext';
import { FlagRow } from './FlagRow';
import type { FeatureFlags } from '@/types/simulator';

interface ExperimentsModalProps {
  flags: FeatureFlags;
  setFlag: (key: keyof FeatureFlags, val: boolean) => void;
  onClose: () => void;
}

export function ExperimentsModal({ flags, setFlag, onClose }: ExperimentsModalProps) {
  const { profile, setProfile } = useAppContext();
  const [name, setName] = useState(profile.name);
  const [initials, setInitials] = useState(profile.initials);

  const handleDone = () => {
    const trimName = name.trim();
    const trimInit = initials.trim().slice(0, 3).toUpperCase();
    if (trimName || trimInit) {
      setProfile({
        name: trimName || profile.name,
        initials: trimInit || profile.initials,
      });
    }
    onClose();
  };

  return (
    <Modal
      backdrop
      label="Feature flags"
      title="Experiments"
      onModalClose={onClose}
      onBackdropClick={onClose}
      footerRight={
        <Button variant="primary" onClick={handleDone}>
          Done
        </Button>
      }
    >
      <ModalContent padded style={{ overflowY: 'auto', maxHeight: '72vh' }}>
        <div
          style={{
            minWidth: 440,
            maxWidth: 500,
            display: 'flex',
            flexDirection: 'column',
            gap: 0,
          }}
        >
          <p style={{ margin: '0 0 12px', fontSize: 13, color: '#8b8d92' }}>
            Opt-in behaviours that are still being trialled. Changes are saved to this browser.
          </p>
          <FlagRow
            id="flag-swing"
            title="Mouse swing momentum"
            badge="Physics"
            desc="Sweep the cursor back and forth across the lanyard to build swing momentum. It tops out at a sensible maximum and gravity settles it back to rest."
            checked={flags.mouseSwing}
            onChange={(v) => setFlag('mouseSwing', v)}
          />
          <div className="flag-sep" />
          <FlagRow
            id="flag-mute"
            title="Mute audio by default"
            desc="Start with the buzzer output muted on load — the user must switch Audio output on themselves."
            checked={flags.muteByDefault}
            onChange={(v) => setFlag('muteByDefault', v)}
          />
          <div className="flag-sep" />
          <FlagRow
            id="flag-device-logs"
            title="BLE / device logs sidebar"
            badge="Debug"
            desc="Live sidebar of client GATT steps plus firmware Log notifies (UUID_LOG) after pairing."
            checked={flags.deviceLogs}
            onChange={(v) => setFlag('deviceLogs', v)}
          />
          <div className="flag-sep" style={{ margin: '4px 0' }} />
          <div className="flag-section-head">User profile</div>
          <div style={{ display: 'flex', gap: 12, marginTop: 8 }}>
            <div style={{ flex: 1 }}>
              <Control>
                <Label htmlFor="prof-name">Display name</Label>
                <Input
                  id="prof-name"
                  value={name}
                  placeholder="e.g. A. Morgan"
                  onChange={(ev) => setName(ev.target.value)}
                />
              </Control>
            </div>
            <div style={{ width: 100 }}>
              <Control>
                <Label htmlFor="prof-init">Initials</Label>
                <Input
                  id="prof-init"
                  value={initials}
                  placeholder="AM"
                  maxLength={3}
                  onChange={(ev) => setInitials(ev.target.value.toUpperCase())}
                />
              </Control>
            </div>
          </div>
        </div>
      </ModalContent>
    </Modal>
  );
}
