import { Panel } from '@/components/ui/Panel';
import { COLORS } from '@/lib/constants/colors';
import { LED_LABELS, HAPTIC_LABELS, BUZZER_LABELS } from '@/lib/constants/patterns';
import type { DeviceState } from '@/types/simulator';

interface DeviceTelemetryProps {
  st: DeviceState;
  muted: boolean;
}

export function DeviceTelemetry({ st, muted }: DeviceTelemetryProps) {
  const ledRgb = COLORS[st.color].rgb;
  const ledOn = st.led !== 'off';
  const buzzerActive = st.buzzer !== 'silent' && st.buzzer !== 'off';

  return (
    <Panel eyebrow="Device Telemetry">
      <div className="hud-grid">
        <div className="hud-cell">
          <div className="hud-label">State</div>
          <div
            className="hud-value"
            style={{
              color:
                st.alert === 'fire'
                  ? '#ff6a60'
                  : st.alert === 'personal'
                    ? '#c98bff'
                    : '#00dc30',
            }}
          >
            {st.alert === 'fire' ? 'Fire' : st.alert === 'personal' ? 'Alert' : 'Standby'}
          </div>
        </div>
        <div className="hud-cell">
          <div className="hud-label">Edge LED</div>
          <div className="hud-value">
            <span
              className="hud-swatch"
              style={{
                color: `rgb(${ledRgb})`,
                background: ledOn ? `rgb(${ledRgb})` : '#333',
              }}
            />
            {ledOn ? COLORS[st.color].label : 'Off'}
          </div>
        </div>
        <div className="hud-cell">
          <div className="hud-label">Pattern</div>
          <div className="hud-value" style={{ fontSize: 13 }}>
            {LED_LABELS[st.led]}
          </div>
        </div>
        <div className="hud-cell">
          <div className="hud-label">Brightness</div>
          <div className="hud-value" style={{ fontSize: 13 }}>
            {st.brightness ?? 100}%
          </div>
        </div>
        <div className="hud-cell">
          <div className="hud-label">Haptic</div>
          <div className="hud-value" style={{ fontSize: 13 }}>
            {HAPTIC_LABELS[st.haptic]}
          </div>
        </div>
        <div className="hud-cell" style={{ gridColumn: '1 / -1' }}>
          <div className="hud-label">Buzzer</div>
          <div className="hud-value" style={{ fontSize: 13, justifyContent: 'space-between' }}>
            {BUZZER_LABELS[st.buzzer]}
            <span
              style={{
                fontSize: 10.5,
                letterSpacing: 1,
                color: buzzerActive && !muted ? '#00dc30' : '#57628a',
              }}
            >
              {muted ? 'MUTED' : buzzerActive ? 'SOUNDING' : 'QUIET'}
            </span>
          </div>
        </div>
      </div>
    </Panel>
  );
}
