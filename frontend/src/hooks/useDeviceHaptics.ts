import { useEffect } from 'react';
import { playDeviceVibration, stopDeviceVibration } from '@/lib/haptics/deviceVibration';
import type { HapticPattern } from '@/types/simulator';

/** Mirror simulator haptic patterns to the device motor when supported (Android; not iOS Safari). */
export function useDeviceHaptics(haptic: HapticPattern) {
  useEffect(() => {
    playDeviceVibration(haptic);
    return () => stopDeviceVibration();
  }, [haptic]);
}
