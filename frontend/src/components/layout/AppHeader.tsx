import { Header, Logo, Avatar } from '@/ds/forge';
import { useAppContext } from '@/context/AppContext';
import { BleConnect } from '@/components/ble/BleConnect';

export function AppHeader() {
  const { profile } = useAppContext();
  const initials = (profile.initials || '').slice(0, 3).toUpperCase() || '?';

  return (
    <div className="app-header">
      <Header
        logo={
          <div className="brand-line">
            <Logo icon iconVariant="color" style={{ transform: 'scale(0.95)' }} />
            <div className="brand-divider" />
            <div>
              <div className="brand-title">Inclusive Life Safety System</div>
              <div className="brand-sub">Smart Lanyard · Simulator</div>
            </div>
          </div>
        }
        rightSlot={
          <div className="header-right">
            <BleConnect />
            <Avatar size="md" text={initials} alt={profile.name} interactive />
          </div>
        }
      />
    </div>
  );
}
