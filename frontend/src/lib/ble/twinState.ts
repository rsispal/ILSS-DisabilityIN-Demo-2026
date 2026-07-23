import type {
  AlertLevel,
  BuzzerPattern,
  ColorKey,
  DeviceState,
  HapticPattern,
  LedPattern,
} from '@/types/simulator';
import { IDLE } from '@/lib/constants/patterns';

export const APP_CODE_TWIN_STATE = 0x01;
export const APP_CODE_HEARTBEAT = 0x02;
/** Side-button UI cue: data[0]=0 left / 1 right, data[1]=0 press. */
export const APP_CODE_BUTTON = 0x10;
export const APP_CODE_PAIRING = 0x40;

export const BUTTON_SIDE_LEFT = 0;
export const BUTTON_SIDE_RIGHT = 1;

export const TWIN_FLAG_ADVANCED = 0x01;

const COLORS: ColorKey[] = ['red', 'green', 'blue', 'teal', 'purple', 'yellow', 'orange'];
const LEDS: LedPattern[] = ['solid', 'flash', 'pulse', 'double', 'alt', 'half', 'chase', 'off'];
const HAPTICS: HapticPattern[] = [
  'off',
  'click',
  'short-pulse',
  'long-pulse',
  'short-pulses',
  'long-pulses',
  'continuous',
  'ramp',
];
const BUZZERS: BuzzerPattern[] = [
  'alternating',
  'silent',
  'bs-sweep',
  'bs-fast-sweep',
  'lf-buzz',
  'siren',
  'code3-beep',
  'code3-sweep',
  'code3-siren',
  'off',
];
const ALERTS: AlertLevel[] = ['none', 'personal', 'fire'];

export function clampBrightness(v: number): number {
  const n = Math.max(0, Math.min(100, Math.round(v)));
  return Math.round(n / 10) * 10;
}

export function packTwinState(st: DeviceState, advanced = false): Uint8Array {
  const out = new Uint8Array(7);
  out[0] = Math.max(0, ALERTS.indexOf(st.alert));
  out[1] = Math.max(0, COLORS.indexOf(st.color));
  out[2] = Math.max(0, LEDS.indexOf(st.led));
  out[3] = Math.max(0, HAPTICS.indexOf(st.haptic));
  out[4] = Math.max(0, BUZZERS.indexOf(st.buzzer));
  out[5] = advanced ? TWIN_FLAG_ADVANCED : 0;
  out[6] = clampBrightness(st.brightness ?? 100);
  return out;
}

export function unpackTwinState(data: Uint8Array): DeviceState {
  if (data.length < 6) return { ...IDLE };
  return {
    alert: ALERTS[data[0]] ?? 'none',
    color: COLORS[data[1]] ?? 'green',
    led: LEDS[data[2]] ?? 'solid',
    haptic: HAPTICS[data[3]] ?? 'off',
    buzzer: BUZZERS[data[4]] ?? 'silent',
    brightness: data.length >= 7 ? clampBrightness(data[6]) : 100,
  };
}

export function isAdvancedPatch(st: DeviceState): boolean {
  return (
    st.alert === 'none' &&
    (st.led !== 'solid' ||
      st.color !== 'green' ||
      st.haptic !== 'off' ||
      (st.buzzer !== 'silent' && st.buzzer !== 'off') ||
      clampBrightness(st.brightness ?? 100) !== 10)
  );
}

/** Stable key for twin-state equality (outbound dedupe / echo suppression). */
export function twinStateKey(st: DeviceState): string {
  return Array.from(packTwinState(st, isAdvancedPatch(st))).join(',');
}
