import type { CSSProperties } from 'react';
import { Panel } from '@/components/ui/Panel';
import { Chips } from '@/components/ui/Chips';
import { COLORS } from '@/lib/constants/colors';
import { LED_PATTERNS } from '@/lib/constants/patterns';
import { clampBrightness } from '@/lib/ble/twinState';
import type { ColorKey, DeviceState, LedPattern } from '@/types/simulator';

const SELECTABLE_LED: LedPattern[] = [
  'solid',
  'flash',
  'double',
  'pulse',
  'alt',
  'half',
  'chase',
  'off',
];

interface AdvancedLedPanelProps {
  st: DeviceState;
  onChange: (patch: Partial<DeviceState>) => void;
}

export function AdvancedLedPanel({ st, onChange }: AdvancedLedPanelProps) {
  const brightness = clampBrightness(st.brightness ?? 100);

  return (
    <Panel eyebrow="Advanced · Edge LED">
      <div className="ctrl-row">
        <div className="ctrl-label">
          Colour <span className="lab-tag">RGB DIFFUSER</span>
        </div>
        <div className="swatches">
          {(Object.keys(COLORS) as ColorKey[]).map((k) => (
            <button
              key={k}
              type="button"
              className={'swatch' + (st.color === k ? ' sel' : '')}
              style={{ '--sw': COLORS[k].rgb } as CSSProperties}
              aria-label={COLORS[k].label}
              onClick={() =>
                onChange({
                  color: k,
                  led: st.led === 'off' ? 'solid' : st.led,
                  alert: 'none',
                })
              }
            />
          ))}
        </div>
      </div>
      <div className="ctrl-row">
        <div className="ctrl-label">Pattern</div>
        <Chips
          items={LED_PATTERNS}
          value={SELECTABLE_LED.includes(st.led) ? st.led : null}
          onChange={(v) => onChange({ led: v as LedPattern, alert: 'none' })}
        />
      </div>
      <div className="ctrl-row">
        <div className="ctrl-label brightness-label">
          <span>
            Brightness <span className="lab-tag">0–100</span>
          </span>
          <span className="brightness-value">{brightness}%</span>
        </div>
        <input
          className="brightness-slider"
          type="range"
          min={0}
          max={100}
          step={10}
          value={brightness}
          aria-label="LED brightness"
          onChange={(ev) =>
            onChange({
              brightness: clampBrightness(Number(ev.target.value)),
              alert: 'none',
              led: st.led === 'off' && Number(ev.target.value) > 0 ? 'solid' : st.led,
            })
          }
        />
        <div className="brightness-ticks" aria-hidden>
          {[0, 20, 40, 60, 80, 100].map((t) => (
            <span key={t}>{t}</span>
          ))}
        </div>
      </div>
    </Panel>
  );
}
