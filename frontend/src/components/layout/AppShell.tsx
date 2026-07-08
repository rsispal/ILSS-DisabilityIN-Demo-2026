import type { CSSProperties, ReactNode, RefObject } from 'react';
import type { AlertLevel } from '@/types/simulator';

interface AppShellProps {
  alert: AlertLevel;
  header: ReactNode;
  banner: ReactNode;
  left: ReactNode;
  center: ReactNode;
  right: ReactNode;
  footer?: ReactNode;
  mobileSheet?: ReactNode;
  sceneRef?: RefObject<HTMLDivElement>;
}

export function AppShell({
  alert,
  header,
  banner,
  left,
  center,
  right,
  footer,
  mobileSheet,
  sceneRef,
}: AppShellProps) {
  const alertRgb =
    alert === 'fire' ? '255,49,49' : alert === 'personal' ? '178,84,255' : '255,49,49';

  return (
    <div className={'app' + (mobileSheet ? ' app--mobile' : '')}>
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
        <div className="col-scene" ref={sceneRef}>
          {center}
        </div>
        <div className="col-right">{right}</div>
      </div>
      {mobileSheet}
      {footer}
    </div>
  );
}
