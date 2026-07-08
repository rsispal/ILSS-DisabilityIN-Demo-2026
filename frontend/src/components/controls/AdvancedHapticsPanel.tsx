import { Panel } from '@/components/ui/Panel';
import { Chips } from '@/components/ui/Chips';
import { HAPTIC_PATTERNS } from '@/lib/constants/patterns';
import type { DeviceState, HapticPattern } from '@/types/simulator';

interface AdvancedHapticsPanelProps {
  st: DeviceState;
  onChange: (patch: Partial<DeviceState>) => void;
}

export function AdvancedHapticsPanel({ st, onChange }: AdvancedHapticsPanelProps) {
  return (
    <Panel eyebrow="Advanced · Haptics">
      <div className="ctrl-label">
        Vibration pattern <span className="lab-tag">LRA MOTOR</span>
      </div>
      <Chips
        items={HAPTIC_PATTERNS}
        value={HAPTIC_PATTERNS.some((p) => p.v === st.haptic) ? st.haptic : null}
        onChange={(v) => onChange({ haptic: v as HapticPattern })}
      />
    </Panel>
  );
}
