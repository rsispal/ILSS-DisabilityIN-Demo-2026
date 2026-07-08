import type { AlertLevel } from '@/types/simulator';
import { FireIcon, PersonIcon } from '@/lib/constants/icons';

interface AlertBannerProps {
  level: AlertLevel;
}

export function AlertBanner({ level }: AlertBannerProps) {
  if (level === 'none') return null;

  const fire = level === 'fire';

  return (
    <div className={`alert-banner${fire ? ' fire' : ' personal'}`}>
      <div className="alert-banner-head">
        <span className="alert-banner-icon">
          {fire ? <FireIcon /> : <PersonIcon />}
        </span>
        <span className="alert-banner-title">
          {fire ? 'Fire emergency active' : 'Personal alert active'}
        </span>
        {!fire && <span className="alert-banner-badge">Assist!</span>}
      </div>
      <p className="alert-banner-desc">
        {fire
          ? 'Lanyard provides multi-sensory alerts to the user in addition to the building fire alarm'
          : 'Discreet assistance request raised from wearable'}
      </p>
    </div>
  );
}
