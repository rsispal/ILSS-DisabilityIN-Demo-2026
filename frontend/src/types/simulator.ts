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
  | 'solid'
  | 'pulse1'
  | 'pulse2'
  | 'continuous'
  | 'click'
  | 'off';

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

export type PressedButton = 'personal' | 'fire' | null;
