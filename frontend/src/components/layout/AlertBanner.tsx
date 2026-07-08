import type { AlertLevel } from '@/types/simulator';
import { FireIcon, PersonIcon } from '@/lib/constants/icons';

interface AlertBannerProps {
  level: AlertLevel;
}

export function AlertBanner({ level }: AlertBannerProps) {
  if (level === 'none') return null;

  const fire = level === 'fire';
  const rgb = fire ? '255,49,49' : '178,84,255';
  const style = {
    display: 'flex',
    alignItems: 'center',
    gap: 14,
    padding: '13px 24px',
    background: `linear-gradient(90deg, rgba(${rgb},0.32), rgba(${rgb},0.12) 60%, rgba(${rgb},0.04))`,
    borderBottom: `1px solid rgba(${rgb},0.6)`,
    color: '#fff',
    position: 'relative' as const,
    zIndex: 25,
    animation: 'edgePulse 1.6s ease-in-out infinite',
    '--alert-rgb': rgb,
  };

  return (
    <div className="alert-banner" style={style}>
      <span
        style={{
          width: 30,
          height: 30,
          borderRadius: 8,
          display: 'grid',
          placeItems: 'center',
          background: `rgba(${rgb},0.25)`,
          color: `rgb(${rgb})`,
          flex: 'none',
        }}
      >
        {fire ? (
          <FireIcon style={{ width: 18, height: 18 }} />
        ) : (
          <PersonIcon style={{ width: 18, height: 18 }} />
        )}
      </span>
      <span style={{ fontWeight: 700, fontSize: 14, letterSpacing: 0.3 }}>
        {fire ? 'FIRE EMERGENCY ACTIVE' : 'PERSONAL ALERT ACTIVE'}
      </span>
      <span style={{ fontSize: 12.5, color: 'rgba(255,255,255,0.72)' }}>
        {fire
          ? '· Evacuation signalling engaged on wearable · responders notified'
          : '· Discreet assistance request raised from wearable'}
      </span>
      <span
        style={{
          marginLeft: 'auto',
          fontSize: 11,
          letterSpacing: 1.5,
          color: `rgb(${rgb})`,
          fontWeight: 700,
        }}
      >
        {fire ? 'CODE RED' : 'ASSIST'}
      </span>
    </div>
  );
}
