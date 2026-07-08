import { useEffect } from 'react';
import { AlertButton } from '@/components/ui/AlertButton';
import { AdvancedLedPanel } from '@/components/controls/AdvancedLedPanel';
import { AdvancedHapticsPanel } from '@/components/controls/AdvancedHapticsPanel';
import { AdvancedBuzzerPanel } from '@/components/controls/AdvancedBuzzerPanel';
import { DeviceTelemetry } from '@/components/controls/DeviceTelemetry';
import { Button } from '@/ds/forge';
import { useBottomSheet } from '@/hooks/useBottomSheet';
import {
  FireIcon,
  PersonIcon,
  ClearIcon,
  StopCircleIcon,
  ChevIcon,
  CloseIcon,
} from '@/lib/constants/icons';
import type { AlertLevel, DeviceState } from '@/types/simulator';

interface MobileControlsSheetProps {
  alert: AlertLevel;
  st: DeviceState;
  muted: boolean;
  onFire: () => void;
  onPersonal: () => void;
  onClearFire: () => void;
  onClearPersonal: () => void;
  onChange: (patch: Partial<DeviceState>) => void;
  onMutedChange: (muted: boolean) => void;
  onOpenExperiments: () => void;
}

export function MobileControlsSheet({
  alert,
  st,
  muted,
  onFire,
  onPersonal,
  onClearFire,
  onClearPersonal,
  onChange,
  onMutedChange,
  onOpenExperiments,
}: MobileControlsSheetProps) {
  const {
    height,
    collapsed,
    advancedOpen,
    dragging,
    handleProps,
    toggleAdvanced,
    collapse,
  } = useBottomSheet();

  useEffect(() => {
    const app = document.querySelector('.app--mobile');
    const scene = document.querySelector('.app--mobile .col-scene');
    if (app) {
      (app as HTMLElement).style.setProperty('--sheet-peek', `${height}px`);
    }
    if (scene) {
      scene.classList.toggle('sheet-dragging', dragging);
    }
  }, [height, dragging]);

  return (
    <>
      {!collapsed && (
        <button
          type="button"
          className="mobile-sheet-backdrop"
          aria-label="Dismiss controls"
          onClick={collapse}
        />
      )}

      <div
        className={`mobile-sheet${collapsed ? ' collapsed' : ''}${advancedOpen ? ' advanced' : ''}${dragging ? ' dragging' : ''}`}
        style={{ height }}
      >
        <div className="mobile-sheet-chrome">
          <div className="mobile-sheet-handle" {...handleProps}>
            <div className="mobile-sheet-grab" />
            {collapsed && (
              <span className="mobile-sheet-handle-hint">Swipe up for controls</span>
            )}
          </div>

          {!collapsed && (
            <button
              type="button"
              className="mobile-sheet-close"
              aria-label="Close controls"
              onClick={collapse}
            >
              <CloseIcon style={{ width: 14, height: 14 }} />
            </button>
          )}
        </div>

        <div className="mobile-sheet-body" aria-hidden={collapsed}>
          <div className="mobile-sheet-primary">
            <div className="alert-group">
              <AlertButton
                kind="danger"
                armed={alert === 'fire'}
                disabled={alert === 'personal'}
                Icon={FireIcon}
                title="Fire Emergency"
                desc="Double-flash red · evacuate"
                onClick={onFire}
              />
              <AlertButton
                kind="clear"
                disabled={alert !== 'fire'}
                Icon={alert === 'fire' ? StopCircleIcon : ClearIcon}
                title="Clear Fire Emergency"
                desc={
                  alert === 'fire'
                    ? 'Tap to stand down the wearable'
                    : 'No active fire emergency'
                }
                onClick={onClearFire}
              />
            </div>
            <div className="alert-group">
              <AlertButton
                kind="alert"
                armed={alert === 'personal'}
                disabled={alert === 'fire'}
                Icon={PersonIcon}
                title="Personal Alert"
                desc="Purple pulse · discreet assist"
                onClick={onPersonal}
              />
              <AlertButton
                kind="clear"
                disabled={alert !== 'personal'}
                Icon={alert === 'personal' ? StopCircleIcon : ClearIcon}
                title="Clear Personal Alert"
                desc={
                  alert === 'personal'
                    ? 'Tap to stand down the wearable'
                    : 'No active personal alert'
                }
                onClick={onClearPersonal}
              />
            </div>
          </div>

          <button
            type="button"
            className="mobile-sheet-advanced-toggle"
            aria-expanded={advancedOpen}
            onClick={toggleAdvanced}
          >
            <span>Advanced controls</span>
            <span className="mobile-sheet-advanced-meta">LED · haptics · buzzer</span>
            <ChevIcon className={'mobile-sheet-chev' + (advancedOpen ? ' up' : '')} />
          </button>

          {advancedOpen && (
            <div className="mobile-sheet-advanced">
              <DeviceTelemetry st={st} muted={muted} />
              <AdvancedLedPanel st={st} onChange={onChange} />
              <AdvancedHapticsPanel st={st} onChange={onChange} />
              <AdvancedBuzzerPanel
                st={st}
                muted={muted}
                onChange={onChange}
                onMutedChange={onMutedChange}
              />
              <Button variant="secondary" onClick={onOpenExperiments} style={{ width: '100%' }}>
                ⚗️ Experiments
              </Button>
            </div>
          )}
        </div>
      </div>
    </>
  );
}
