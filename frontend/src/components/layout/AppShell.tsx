import type { CSSProperties, ReactNode } from 'react';
import type { AlertLevel } from '@/types/simulator';

interface AppShellProps {
  alert: AlertLevel;
  header: ReactNode;
  banner: ReactNode;
  left: ReactNode;
  center: ReactNode;
  right: ReactNode;
  footer?: ReactNode;
}

export function AppShell({ alert, header, banner, left, center, right, footer }: AppShellProps) {
  const alertRgb =
    alert === 'fire' ? '255,49,49' : alert === 'personal' ? '178,84,255' : '255,49,49';

  return (
    <div className="app">
      <div className="space-bg" />
      <div className="grid-floor" />
      <div
        className={'edge-glow' + (alert !== 'none' ? ' on' : '')}
        style={{ '--alert-rgb': alertRgb } as CSSProperties}
      />
      {header}
      {banner}
      <div className="stage">
        <div className="col-left">{left}</div>
        <div className="col-scene">{center}</div>
        <div className="col-right">{right}</div>
      </div>
      {footer}
    </div>
  );
}
