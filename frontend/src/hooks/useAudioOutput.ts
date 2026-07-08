import { useEffect } from 'react';
import { ilssAudio } from '@/lib/audio/ilssAudio';
import type { BuzzerPattern } from '@/types/simulator';

export function useAudioOutput(buzzer: BuzzerPattern, muted: boolean) {
  useEffect(() => {
    ilssAudio.set(buzzer);
  }, [buzzer]);

  useEffect(() => {
    ilssAudio.setMuted(muted);
  }, [muted]);
}
