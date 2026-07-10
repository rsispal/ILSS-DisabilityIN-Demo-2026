export type ColorKey = 'red' | 'green' | 'blue' | 'teal' | 'purple' | 'yellow' | 'orange';

export type AlertLevel = 'none' | 'personal' | 'fire';

export type LedPattern =
  | 'solid'
  | 'flash'
  | 'pulse'
  | 'double'
  | 'alt'
  | 'half'
  | 'chase'
  | 'off';

export type HapticPattern =
  | 'off'
  | 'click'
  | 'short-pulse'
  | 'long-pulse'
  | 'short-pulses'
  | 'long-pulses'
  | 'continuous'
  | 'ramp';

export type BuzzerPattern =
  | 'alternating'
  | 'silent'
  | 'bs-sweep'
  | 'bs-fast-sweep'
  | 'lf-buzz'
  | 'siren'
  | 'code3-beep'
  | 'code3-sweep'
  | 'code3-siren'
  | 'off';

export interface DeviceState {
  color: ColorKey;
  led: LedPattern;
  haptic: HapticPattern;
  buzzer: BuzzerPattern;
  alert: AlertLevel;
  /** LED brightness 0–100 in steps of 10. */
  brightness: number;
}

export interface FeatureFlags {
  mouseSwing: boolean;
  muteByDefault: boolean;
  /** Secret unlock: stream device logs over BLE into a sidebar */
  deviceLogs: boolean;
}

export interface UserProfile {
  name: string;
  initials: string;
}

export type PressedButton = 'personal' | 'fire' | 'left' | 'right' | null;
