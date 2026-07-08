import { Panel } from '@/components/ui/Panel';
import { Chips } from '@/components/ui/Chips';
import { Toggle } from '@/ds/forge';
import { BUZZER_PATTERNS } from '@/lib/constants/patterns';
import { SoundOnIcon, SoundOffIcon } from '@/lib/constants/icons';
import type { BuzzerPattern, DeviceState } from '@/types/simulator';

interface AdvancedBuzzerPanelProps {
  st: DeviceState;
  muted: boolean;
  onChange: (patch: Partial<DeviceState>) => void;
  onMutedChange: (muted: boolean) => void;
}

export function AdvancedBuzzerPanel({
  st,
  muted,
  onChange,
  onMutedChange,
}: AdvancedBuzzerPanelProps) {
  return (
    <Panel eyebrow="Advanced · Buzzer">
      <div className="ctrl-label">
        Acoustic pattern <span className="lab-tag">PIEZO SOUNDER</span>
      </div>
      <Chips
        items={BUZZER_PATTERNS}
        value={st.buzzer}
        onChange={(v) => onChange({ buzzer: v as BuzzerPattern })}
      />
      <div className="sound-toggle" style={{ marginTop: 14 }}>
        <span className="st-left">
          {muted ? <SoundOffIcon /> : <SoundOnIcon />}
          Audio output
        </span>
        <Toggle
          id="snd"
          checked={!muted}
          onChange={(ev) => onMutedChange(!ev.target.checked)}
        />
      </div>
    </Panel>
  );
}
