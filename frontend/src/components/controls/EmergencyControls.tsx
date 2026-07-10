import { Panel } from '@/components/ui/Panel';
import { AlertButton } from '@/components/ui/AlertButton';
import {
  PersonIcon,
  FireIcon,
  ClearIcon,
  StopCircleIcon,
  ResetIcon,
} from '@/lib/constants/icons';
import type { AlertLevel } from '@/types/simulator';

interface EmergencyControlsProps {
  alert: AlertLevel;
  onPersonal: () => void;
  onClearPersonal: () => void;
  onFire: () => void;
  onClearFire: () => void;
  customActive?: boolean;
  customResetSeconds?: number;
  customRemainingMs?: number;
  onResetCustom?: () => void;
}

export function EmergencyControls({
  alert,
  onPersonal,
  onClearPersonal,
  onFire,
  onClearFire,
  customActive = false,
  customResetSeconds = 0,
  customRemainingMs = 0,
  onResetCustom,
}: EmergencyControlsProps) {
  return (
    <Panel eyebrow="Emergency Controls">
      <div className="action-stack">
        <div className="alert-group">
          <AlertButton
            kind="danger"
            armed={alert === 'fire'}
            disabled={alert === 'personal'}
            Icon={FireIcon}
            title="Simulate Fire Emergency"
            desc="Double-flash red · haptics · buzzer"
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
            title="Simulate Personal Alert"
            desc="Pulsing purple · haptics · buzzer"
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

        {customActive && onResetCustom && (
          <div className="custom-reset-card" role="status">
            <div className="custom-reset-meta">
              <div className="custom-reset-title">Custom pattern running</div>
              <div className="custom-reset-sub">
                Auto-resets in <strong>{customResetSeconds}s</strong>
              </div>
              <div
                className="custom-reset-bar"
                style={{
                  ['--reset-pct' as string]: `${Math.max(0, Math.min(100, (customRemainingMs / 30000) * 100))}%`,
                }}
              />
            </div>
            <button type="button" className="custom-reset-btn" onClick={onResetCustom}>
              <ResetIcon style={{ width: 15, height: 15 }} />
              Reset
            </button>
          </div>
        )}
      </div>
    </Panel>
  );
}
