import { useCallback, useEffect, useRef, useState } from 'react';
import { IDLE } from '@/lib/constants/patterns';
import { isAdvancedPatch } from '@/lib/ble/twinState';
import type { DeviceState } from '@/types/simulator';

const CUSTOM_AUTORESET_MS = 30_000;

export function useSimulatorState() {
  const [st, setSt] = useState<DeviceState>(IDLE);
  const [customEndsAt, setCustomEndsAt] = useState<number | null>(null);
  const [customRemainingMs, setCustomRemainingMs] = useState(0);
  const stRef = useRef(st);
  stRef.current = st;

  const setCh = useCallback((patch: Partial<DeviceState>) => {
    setSt((s) => ({ ...s, ...patch }));
  }, []);

  const resetToIdle = useCallback(() => {
    setSt({ ...IDLE });
    setCustomEndsAt(null);
    setCustomRemainingMs(0);
  }, []);

  const firePreset = useCallback(() => {
    setCustomEndsAt(null);
    setCustomRemainingMs(0);
    setSt({
      color: 'red',
      led: 'double',
      haptic: 'long-pulses',
      buzzer: 'code3-sweep',
      alert: 'fire',
      brightness: 100,
    });
  }, []);

  const personalPreset = useCallback(() => {
    setCustomEndsAt(null);
    setCustomRemainingMs(0);
    setSt({
      color: 'purple',
      led: 'pulse',
      haptic: 'long-pulses',
      buzzer: 'code3-siren',
      alert: 'personal',
      brightness: 100,
    });
  }, []);

  const clearFire = useCallback(() => {
    setSt((s) => (s.alert === 'fire' ? { ...IDLE } : s));
  }, []);

  const clearPersonal = useCallback(() => {
    setSt((s) => (s.alert === 'personal' ? { ...IDLE } : s));
  }, []);

  // Arm / clear 30s autoreset for custom advanced patterns (not fire/personal).
  useEffect(() => {
    if (st.alert === 'fire' || st.alert === 'personal') {
      setCustomEndsAt(null);
      setCustomRemainingMs(0);
      return;
    }
    if (isAdvancedPatch(st)) {
      setCustomEndsAt(Date.now() + CUSTOM_AUTORESET_MS);
      setCustomRemainingMs(CUSTOM_AUTORESET_MS);
      return;
    }
    setCustomEndsAt(null);
    setCustomRemainingMs(0);
  }, [st]);

  useEffect(() => {
    if (customEndsAt == null) return undefined;
    const tick = window.setInterval(() => {
      const left = Math.max(0, customEndsAt - Date.now());
      setCustomRemainingMs(left);
      if (left <= 0) {
        // Only reset if still on a custom non-alert pattern.
        if (isAdvancedPatch(stRef.current) && stRef.current.alert === 'none') {
          setSt({ ...IDLE });
        }
        setCustomEndsAt(null);
        setCustomRemainingMs(0);
      }
    }, 200);
    return () => window.clearInterval(tick);
  }, [customEndsAt]);

  const customActive = customEndsAt != null && customRemainingMs > 0;

  return {
    st,
    setCh,
    firePreset,
    personalPreset,
    clearFire,
    clearPersonal,
    resetToIdle,
    customActive,
    customRemainingMs,
    customResetSeconds: Math.ceil(customRemainingMs / 1000),
  };
}
