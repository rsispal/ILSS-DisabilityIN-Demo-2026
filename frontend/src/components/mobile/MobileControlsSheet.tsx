import { useState } from 'react';
import { AlertButton } from '@/components/ui/AlertButton';
import { AdvancedLedPanel } from '@/components/controls/AdvancedLedPanel';
import { AdvancedHapticsPanel } from '@/components/controls/AdvancedHapticsPanel';
import { AdvancedBuzzerPanel } from '@/components/controls/AdvancedBuzzerPanel';
import { DeviceTelemetry } from '@/components/controls/DeviceTelemetry';
import {
  FireIcon,
  PersonIcon,
  StopCircleIcon,
  ChevIcon,
} from '@/lib/constants/icons';
import { isVibrationSupported } from '@/lib/haptics/deviceVibration';
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
}: MobileControlsSheetProps) {
  const [advancedOpen, setAdvancedOpen] = useState(false);
  const vibrationReady = isVibrationSupported();

  return (
    <div className={`mobile-sheet${advancedOpen ? ' expanded' : ''}`}>
      <div className="mobile-sheet-grab" aria-hidden />
      <div className="mobile-sheet-body">
        <div className="mobile-sheet-primary">
          <AlertButton
            kind="danger"
            armed={alert === 'fire'}
            Icon={FireIcon}
            title="Fire Emergency"
            desc="Double-flash red · evacuate"
            onClick={onFire}
          />
          <AlertButton
            kind="alert"
            armed={alert === 'personal'}
            Icon={PersonIcon}
            title="Personal Alert"
            desc="Purple pulse · discreet assist"
            onClick={onPersonal}
          />
        </div>

        {alert === 'fire' && (
          <AlertButton
            kind="clear"
            Icon={StopCircleIcon}
            title="Clear Fire Emergency"
            desc="Tap to stand down the wearable"
            onClick={onClearFire}
          />
        )}
        {alert === 'personal' && (
          <AlertButton
            kind="clear"
            Icon={StopCircleIcon}
            title="Clear Personal Alert"
            desc="Tap to stand down the wearable"
            onClick={onClearPersonal}
          />
        )}

        <button
          type="button"
          className="mobile-sheet-advanced-toggle"
          aria-expanded={advancedOpen}
          onClick={() => setAdvancedOpen((o) => !o)}
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
            <p className="mobile-sheet-hint">
              {vibrationReady
                ? 'Device vibration mirrors the lanyard haptic pattern on supported phones.'
                : 'Device vibration is unavailable in this browser (common on iOS Safari). Visual haptics still run on the badge.'}
            </p>
          </div>
        )}
      </div>
    </div>
  );
}
