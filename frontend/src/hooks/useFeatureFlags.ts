import { useCallback, useState } from 'react';
import { loadFlags, saveFlags } from '@/lib/storage';
import type { FeatureFlags } from '@/types/simulator';

export function useFeatureFlags() {
  const [flags, setFlags] = useState(loadFlags);
  const [muted, setMuted] = useState(() => loadFlags().muteByDefault);

  const setFlag = useCallback((key: keyof FeatureFlags, val: boolean) => {
    setFlags((f) => {
      const next = { ...f, [key]: val };
      saveFlags(next);
      return next;
    });
    if (key === 'muteByDefault' && val) setMuted(true);
  }, []);

  return { flags, setFlag, muted, setMuted };
}
