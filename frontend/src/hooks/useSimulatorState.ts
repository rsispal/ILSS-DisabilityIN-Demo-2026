import { useCallback, useState } from 'react';
import { IDLE } from '@/lib/constants/patterns';
import type { DeviceState } from '@/types/simulator';

export function useSimulatorState() {
  const [st, setSt] = useState<DeviceState>(IDLE);

  const setCh = useCallback((patch: Partial<DeviceState>) => {
    setSt((s) => ({ ...s, ...patch }));
  }, []);

  const firePreset = useCallback(
    () =>
      setSt({
        color: 'red',
        led: 'double',
        haptic: 'continuous',
        buzzer: 'code3-sweep',
        alert: 'fire',
      }),
    [],
  );

  const personalPreset = useCallback(
    () =>
      setSt({
        color: 'purple',
        led: 'pulse',
        haptic: 'pulse2',
        buzzer: 'code3-siren',
        alert: 'personal',
      }),
    [],
  );

  const clearFire = useCallback(
    () => setSt((s) => (s.alert === 'fire' ? IDLE : s)),
    [],
  );

  const clearPersonal = useCallback(
    () => setSt((s) => (s.alert === 'personal' ? IDLE : s)),
    [],
  );

  return {
    st,
    setCh,
    firePreset,
    personalPreset,
    clearFire,
    clearPersonal,
  };
}
