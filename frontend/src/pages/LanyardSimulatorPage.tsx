import { useEffect, useRef, useState } from 'react';
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
import { DeviceLogsSidebar } from '@/components/debug/DeviceLogsSidebar';
import { useFeatureFlags } from '@/hooks/useFeatureFlags';
import { useSimulatorState } from '@/hooks/useSimulatorState';
import { useAudioOutput } from '@/hooks/useAudioOutput';
import { useIsMobile } from '@/hooks/useIsMobile';
import { useLanyardScale } from '@/hooks/useLanyardScale';
import { useBleTwin } from '@/context/BleTwinContext';
import { COLORS } from '@/lib/constants/colors';
import { BUZZER_DUR } from '@/lib/constants/patterns';
import type { DeviceState, PressedButton } from '@/types/simulator';

export function LanyardSimulatorPage() {
  const isMobile = useIsMobile();
  const sceneRef = useRef<HTMLDivElement>(null);
  const { flags, setFlag, muted, setMuted } = useFeatureFlags();
  const { st, setCh, firePreset, personalPreset, clearFire, clearPersonal } =
    useSimulatorState();
  const ble = useBleTwin();
  const [pressed, setPressed] = useState<PressedButton>(null);
  const [expOpen, setExpOpen] = useState(false);
  const [introPhase, setIntroPhase] = useState<'wait' | 'play' | 'done'>('wait');
  const skipNextSend = useRef(false);
  const stRef = useRef(st);
  stRef.current = st;

  useEffect(() => {
    const playTimer = window.setTimeout(() => setIntroPhase('play'), 1000);
    const doneTimer = window.setTimeout(() => setIntroPhase('done'), 4300);
    return () => {
      window.clearTimeout(playTimer);
      window.clearTimeout(doneTimer);
    };
  }, []);

  useAudioOutput(st.buzzer, muted);
  useLanyardScale(sceneRef, isMobile);

  const { setOnRemoteState, sendTwinState, status: bleStatus, msg: bleMsg } = ble;

  // Inbound twin state from lanyard (e.g. button personal alert)
  useEffect(() => {
    setOnRemoteState((remote) => {
      const cur = stRef.current;
      if (cur.alert === 'fire' && remote.alert === 'personal') return;
      if (cur.alert === 'personal' && remote.alert === 'fire') return;
      skipNextSend.current = true;
      setCh(remote);
    });
    return () => setOnRemoteState(null);
  }, [setOnRemoteState, setCh]);

  // Outbound: push local state changes to device
  useEffect(() => {
    if (introPhase !== 'done') return;
    if (bleStatus !== 'connected') return;
    if (skipNextSend.current) {
      skipNextSend.current = false;
      return;
    }
    void sendTwinState(st);
  }, [st, sendTwinState, bleStatus, introPhase]);

  const press = (which: PressedButton, fn: () => void) => {
    fn();
    setPressed(which);
    setTimeout(() => setPressed(null), 220);
  };

  const onFire = () => {
    if (st.alert === 'personal') return;
    firePreset();
  };
  const onPersonal = () => {
    if (st.alert === 'fire') return;
    personalPreset();
  };

  const onChangeAdvanced = (patch: Partial<DeviceState>) => {
    if (st.alert !== 'none' && (patch.color !== undefined || patch.led !== undefined)) {
      // Advanced LED changes clear alert on web panels already via alert:'none'
    }
    setCh(patch);
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
      introLedWait={introPhase === 'wait'}
      introPulse={introPhase === 'play'}
      onPressPersonal={() =>
        press('personal', () => (st.alert === 'personal' ? clearPersonal() : onPersonal()))
      }
      onPressFire={() =>
        press('fire', () => (st.alert === 'fire' ? clearFire() : onFire()))
      }
    />
  );

  return (
    <AppShell
      alert={st.alert}
      sceneRef={sceneRef}
      header={<AppMenuBar />}
      banner={
        <>
          <AlertBanner level={st.alert} />
          {bleStatus === 'disconnected' && bleMsg ? (
            <div style={{ fontSize: 12, padding: '4px 12px', color: '#8b8d92' }}>{bleMsg}</div>
          ) : null}
        </>
      }
      left={
        <>
          <EmergencyControls
            alert={st.alert}
            onPersonal={onPersonal}
            onClearPersonal={clearPersonal}
            onFire={onFire}
            onClearFire={clearFire}
          />
          <DeviceTelemetry st={st} muted={muted} />
        </>
      }
      center={lanyard}
      right={
        <>
          <AdvancedLedPanel st={st} onChange={onChangeAdvanced} />
          <AdvancedHapticsPanel st={st} onChange={onChangeAdvanced} />
          <AdvancedBuzzerPanel
            st={st}
            muted={muted}
            onChange={onChangeAdvanced}
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
            onFire={onFire}
            onPersonal={onPersonal}
            onClearFire={clearFire}
            onClearPersonal={clearPersonal}
            onChange={onChangeAdvanced}
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
          <DeviceLogsSidebar
            open={flags.deviceLogs}
            onClose={() => setFlag('deviceLogs', false)}
          />
        </>
      }
    />
  );
}
