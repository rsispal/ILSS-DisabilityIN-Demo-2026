import { useRef, useState } from 'react';
import { AppShell } from '@/components/layout/AppShell';
import { AppMenuBar } from '@/components/layout/AppMenuBar';
import { AlertBanner } from '@/components/layout/AlertBanner';
import { EmergencyControls } from '@/components/controls/EmergencyControls';
import { DeviceTelemetry } from '@/components/controls/DeviceTelemetry';
import { AdvancedLedPanel } from '@/components/controls/AdvancedLedPanel';
import { AdvancedHapticsPanel } from '@/components/controls/AdvancedHapticsPanel';
import { AdvancedBuzzerPanel } from '@/components/controls/AdvancedBuzzerPanel';
import { Lanyard } from '@/components/lanyard/Lanyard';
import { MobileControlsSheet } from '@/components/mobile/MobileControlsSheet';
import { ExperimentsFab } from '@/components/experiments/ExperimentsFab';
import { ExperimentsModal } from '@/components/experiments/ExperimentsModal';
import { useFeatureFlags } from '@/hooks/useFeatureFlags';
import { useSimulatorState } from '@/hooks/useSimulatorState';
import { useAudioOutput } from '@/hooks/useAudioOutput';
import { useIsMobile } from '@/hooks/useIsMobile';
import { useLanyardScale } from '@/hooks/useLanyardScale';
import { COLORS } from '@/lib/constants/colors';
import { BUZZER_DUR } from '@/lib/constants/patterns';
import type { PressedButton } from '@/types/simulator';

export function LanyardSimulatorPage() {
  const isMobile = useIsMobile();
  const sceneRef = useRef<HTMLDivElement>(null);
  const { flags, setFlag, muted, setMuted } = useFeatureFlags();
  const { st, setCh, firePreset, personalPreset, clearFire, clearPersonal } =
    useSimulatorState();
  const [pressed, setPressed] = useState<PressedButton>(null);
  const [expOpen, setExpOpen] = useState(false);

  useAudioOutput(st.buzzer, muted);
  useLanyardScale(sceneRef, isMobile);

  const press = (which: PressedButton, fn: () => void) => {
    fn();
    setPressed(which);
    setTimeout(() => setPressed(null), 220);
  };

  const ledRgb = COLORS[st.color].rgb;
  const ledOn = st.led !== 'off';
  const buzzerActive = st.buzzer !== 'silent' && st.buzzer !== 'off';

  const lanyard = (
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
        press('personal', () => (st.alert === 'personal' ? clearPersonal() : personalPreset()))
      }
      onPressFire={() =>
        press('fire', () => (st.alert === 'fire' ? clearFire() : firePreset()))
      }
    />
  );

  return (
    <AppShell
      alert={st.alert}
      sceneRef={sceneRef}
      header={<AppMenuBar />}
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
      center={lanyard}
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
      mobileSheet={
        isMobile ? (
          <MobileControlsSheet
            alert={st.alert}
            st={st}
            muted={muted}
            onFire={firePreset}
            onPersonal={personalPreset}
            onClearFire={clearFire}
            onClearPersonal={clearPersonal}
            onChange={setCh}
            onMutedChange={setMuted}
            onOpenExperiments={() => setExpOpen(true)}
          />
        ) : undefined
      }
      footer={
        <>
          {!isMobile && <ExperimentsFab onOpen={() => setExpOpen(true)} />}
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
