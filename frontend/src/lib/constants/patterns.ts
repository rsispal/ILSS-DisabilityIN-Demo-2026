import type { BuzzerPattern, HapticPattern, LedPattern } from '@/types/simulator';

export const LED_PATTERNS = [
  { v: 'solid' as const, l: 'Solid' },
  { v: 'flash' as const, l: 'Single flash' },
  { v: 'alt' as const, l: 'Alternating' },
  { v: 'half' as const, l: 'Half-half' },
  { v: 'chase' as const, l: 'Chasing' },
  { v: 'off' as const, l: 'Off' },
];

export const LED_LABELS: Record<LedPattern, string> = {
  solid: 'Solid',
  flash: 'Single flash',
  alt: 'Alternating',
  half: 'Half-half',
  chase: 'Chasing',
  off: 'Off',
  pulse: 'Pulsing',
  double: 'Double flash',
};

export const HAPTIC_PATTERNS = [
  { v: 'off' as const, l: 'Off' },
  { v: 'click' as const, l: 'Click' },
  { v: 'short-pulse' as const, l: 'Short pulse' },
  { v: 'long-pulse' as const, l: 'Long pulse' },
  { v: 'short-pulses' as const, l: 'Short pulses' },
  { v: 'long-pulses' as const, l: 'Long pulses' },
  { v: 'continuous' as const, l: 'Strong continuous' },
  { v: 'ramp' as const, l: 'Ramp up' },
];

export const HAPTIC_LABELS: Record<HapticPattern, string> = {
  off: 'Off',
  click: 'Click',
  'short-pulse': 'Short pulse',
  'long-pulse': 'Long pulse',
  'short-pulses': 'Short pulses',
  'long-pulses': 'Long pulses',
  continuous: 'Strong continuous',
  ramp: 'Ramp up',
};

export const BUZZER_PATTERNS = [
  { v: 'alternating' as const, l: 'Alternating' },
  { v: 'silent' as const, l: 'Silent' },
  { v: 'bs-sweep' as const, l: 'BS sweep' },
  { v: 'bs-fast-sweep' as const, l: 'BS fast sweep' },
  { v: 'lf-buzz' as const, l: 'LF buzz' },
  { v: 'siren' as const, l: 'Siren' },
  { v: 'code3-beep' as const, l: 'Code-3 beep' },
  { v: 'code3-sweep' as const, l: 'Code-3 sweep' },
  { v: 'code3-siren' as const, l: 'Code-3 siren' },
  { v: 'off' as const, l: 'Off' },
];

export const BUZZER_LABELS: Record<BuzzerPattern, string> = {
  alternating: 'Alternating',
  silent: 'Silent',
  'bs-sweep': 'BS sweep',
  'bs-fast-sweep': 'BS fast sweep',
  'lf-buzz': 'LF buzz',
  siren: 'Siren',
  'code3-beep': 'Code-3 beep',
  'code3-sweep': 'Code-3 sweep',
  'code3-siren': 'Code-3 siren',
  off: 'Off',
};

export const BUZZER_DUR: Partial<Record<BuzzerPattern, number>> = {
  alternating: 0.5,
  'bs-sweep': 1.0,
  'bs-fast-sweep': 0.3,
  'lf-buzz': 0.5,
  siren: 0.28,
  'code3-beep': 1.0,
  'code3-sweep': 1.0,
  'code3-siren': 1.0,
  silent: 1.4,
  off: 1.4,
};

export const IDLE = {
  color: 'green' as const,
  led: 'solid' as const,
  haptic: 'off' as const,
  buzzer: 'silent' as const,
  alert: 'none' as const,
  brightness: 40,
};
