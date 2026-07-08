import { useState } from 'react';
import { AppShell } from '@/components/layout/AppShell';
import { AppHeader } from '@/components/layout/AppHeader';
import { AlertBanner } from '@/components/layout/AlertBanner';
import { EmergencyControls } from '@/components/controls/EmergencyControls';
import { DeviceTelemetry } from '@/components/controls/DeviceTelemetry';
import { AdvancedLedPanel } from '@/components/controls/AdvancedLedPanel';
import { AdvancedHapticsPanel } from '@/components/controls/AdvancedHapticsPanel';
import { AdvancedBuzzerPanel } from '@/components/controls/AdvancedBuzzerPanel';
import { Lanyard } from '@/components/lanyard/Lanyard';
import { ExperimentsFab } from '@/components/experiments/ExperimentsFab';
import { ExperimentsModal } from '@/components/experiments/ExperimentsModal';
import { useFeatureFlags } from '@/hooks/useFeatureFlags';
import { useSimulatorState } from '@/hooks/useSimulatorState';
import { useAudioOutput } from '@/hooks/useAudioOutput';
import { COLORS } from '@/lib/constants/colors';
import { BUZZER_DUR } from '@/lib/constants/patterns';
import type { PressedButton } from '@/types/simulator';

export function LanyardSimulatorPage() {
  const { flags, setFlag, muted, setMuted } = useFeatureFlags();
  const { st, setCh, firePreset, personalPreset, clearFire, clearPersonal } =
    useSimulatorState();
  const [pressed, setPressed] = useState<PressedButton>(null);
  const [expOpen, setExpOpen] = useState(false);

  useAudioOutput(st.buzzer, muted);

  const press = (which: PressedButton, fn: () => void) => {
    fn();
    setPressed(which);
    setTimeout(() => setPressed(null), 220);
  };

  const ledRgb = COLORS[st.color].rgb;
  const ledOn = st.led !== 'off';
  const buzzerActive = st.buzzer !== 'silent' && st.buzzer !== 'off';

  return (
    <AppShell
      alert={st.alert}
      header={<AppHeader />}
      banner={<AlertBanner level={st.alert} />}
      left={
        <>
          <EmergencyControls
            alert={st.alert}
            onPersonal={personalPreset}
            onClearPersonal={clearPersonal}
            onFire={firePreset}
            onClearFire={clearFire}
          />
          <DeviceTelemetry st={st} muted={muted} />
        </>
      }
      center={
        <Lanyard
          ledRgb={ledRgb}
          ledPattern={st.led}
          ledOn={ledOn}
          hapticPattern={st.haptic}
          buzzerActive={buzzerActive}
          buzzerDur={BUZZER_DUR[st.buzzer] || 1.4}
          pressed={pressed}
          swingEnabled={flags.mouseSwing}
          onPressPersonal={() =>
            press(
              'personal',
              () => (st.alert === 'personal' ? clearPersonal() : personalPreset()),
            )
          }
          onPressFire={() =>
            press('fire', () => (st.alert === 'fire' ? clearFire() : firePreset()))
          }
        />
      }
      right={
        <>
          <AdvancedLedPanel st={st} onChange={setCh} />
          <AdvancedHapticsPanel st={st} onChange={setCh} />
          <AdvancedBuzzerPanel
            st={st}
            muted={muted}
            onChange={setCh}
            onMutedChange={setMuted}
          />
        </>
      }
      footer={
        <>
          <ExperimentsFab onOpen={() => setExpOpen(true)} />
          {expOpen && (
            <ExperimentsModal
              flags={flags}
              setFlag={setFlag}
              onClose={() => setExpOpen(false)}
            />
          )}
        </>
      }
    />
  );
}
