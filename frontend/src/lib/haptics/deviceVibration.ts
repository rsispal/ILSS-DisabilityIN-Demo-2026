import type { HapticPattern } from '@/types/simulator';

export function isVibrationSupported(): boolean {
  return typeof navigator !== 'undefined' && typeof navigator.vibrate === 'function';
}

/** One-shot vibration patterns (ms on/off). */
const PATTERN_CYCLES: Record<Exclude<HapticPattern, 'off'>, number[]> = {
  solid: [120],
  continuous: [80, 40, 80, 40, 80, 40, 80, 40],
  pulse1: [180, 700],
  pulse2: [120, 180, 120, 700],
  click: [45],
};

const LOOP_MS: Record<Exclude<HapticPattern, 'off' | 'click'>, number> = {
  solid: 140,
  continuous: 400,
  pulse1: 900,
  pulse2: 1200,
};

let loopTimer: ReturnType<typeof setInterval> | null = null;

function pulse(pattern: number[]) {
  if (!isVibrationSupported()) return;
  navigator.vibrate(pattern);
}

export function stopDeviceVibration() {
  if (loopTimer) {
    clearInterval(loopTimer);
    loopTimer = null;
  }
  if (isVibrationSupported()) navigator.vibrate(0);
}

export function playDeviceVibration(pattern: HapticPattern) {
  stopDeviceVibration();
  if (pattern === 'off' || !isVibrationSupported()) return;

  const cycle = PATTERN_CYCLES[pattern];
  pulse(cycle);

  if (pattern === 'click') return;

  const interval = LOOP_MS[pattern as keyof typeof LOOP_MS];
  if (interval) {
    loopTimer = setInterval(() => pulse(cycle), interval);
  }
}
