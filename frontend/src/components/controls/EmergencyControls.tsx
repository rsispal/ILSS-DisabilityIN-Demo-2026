import { Panel } from '@/components/ui/Panel';
import { AlertButton } from '@/components/ui/AlertButton';
import {
  PersonIcon,
  FireIcon,
  ClearIcon,
  StopCircleIcon,
} from '@/lib/constants/icons';
import type { AlertLevel } from '@/types/simulator';

interface EmergencyControlsProps {
  alert: AlertLevel;
  onPersonal: () => void;
  onClearPersonal: () => void;
  onFire: () => void;
  onClearFire: () => void;
}

export function EmergencyControls({
  alert,
  onPersonal,
  onClearPersonal,
  onFire,
  onClearFire,
}: EmergencyControlsProps) {
  return (
    <Panel eyebrow="Emergency Controls">
      <div className="action-stack">
        <AlertButton
          kind="alert"
          armed={alert === 'personal'}
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
        <AlertButton
          kind="danger"
          armed={alert === 'fire'}
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
    </Panel>
  );
}
