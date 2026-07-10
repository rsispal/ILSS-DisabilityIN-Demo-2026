import { useEffect, useRef, useState } from 'react';
import { AppShell } from '@/components/layout/AppShell';
import { AppMenuBar } from '@/components/layout/AppMenuBar';
import { AlertBanner } from '@/components/layout/AlertBanner';
import { EmergencyControls } from '@/components/controls/EmergencyControls';
import { DeviceTelemetry } from '@/components/controls/DeviceTelemetry';
import { LanyardDeviceSummary } from '@/components/ble/LanyardDeviceSummary';
import { AdvancedLedPanel } from '@/components/controls/AdvancedLedPanel';
import { AdvancedHapticsPanel } from '@/components/controls/AdvancedHapticsPanel';
import { AdvancedBuzzerPanel } from '@/components/controls/AdvancedBuzzerPanel';
import { Lanyard } from '@/components/lanyard/Lanyard';
import { MobileControlsSheet } from '@/components/mobile/MobileControlsSheet';
import { ExperimentsFab } from '@/components/experiments/ExperimentsFab';
import { ExperimentsModal } from '@/components/experiments/ExperimentsModal';
import { DeviceLogsSidebar } from '@/components/debug/DeviceLogsSidebar';
import { LinkLostModal } from '@/components/ble/LinkLostModal';
import { BleConnectModal } from '@/components/ble/BleConnectModal';
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
  const {
    st,
    setCh,
    firePreset,
    personalPreset,
    clearFire,
    clearPersonal,
    resetToIdle,
    customActive,
    customRemainingMs,
    customResetSeconds,
  } = useSimulatorState();
  const ble = useBleTwin();
  const [pressed, setPressed] = useState<PressedButton>(null);
  const [expOpen, setExpOpen] = useState(false);
  const [bleOpen, setBleOpen] = useState(false);
  const [hbPulse, setHbPulse] = useState(false);
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

  const {
    setOnRemoteState,
    setOnButtonPress,
    setOnHeartbeat,
    sendTwinState,
    status: bleStatus,
    msg: bleMsg,
    linkLost,
    clearLinkLost,
  } = ble;

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

  // Momentary physical side-button → purple flash on the virtual lanyard
  useEffect(() => {
    setOnButtonPress((side) => {
      setPressed(side);
      window.setTimeout(() => setPressed((p) => (p === side ? null : p)), 280);
    });
    return () => setOnButtonPress(null);
  }, [setOnButtonPress]);

  // Heartbeat / poll alive → brief green LED pulse on the virtual lanyard
  useEffect(() => {
    setOnHeartbeat(() => {
      setHbPulse(true);
      window.setTimeout(() => setHbPulse(false), 550);
    });
    return () => setOnHeartbeat(null);
  }, [setOnHeartbeat]);

  // Outbound: push local state changes to device (wait for pairing CCCD settle)
  useEffect(() => {
    if (introPhase !== 'done') return;
    if (bleStatus !== 'connected') return;
    if (skipNextSend.current) {
      skipNextSend.current = false;
      return;
    }
    const t = window.setTimeout(() => {
      void sendTwinState(st);
    }, 120);
    return () => window.clearTimeout(t);
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
      ledBrightness={st.brightness}
      hapticPattern={st.haptic}
      buzzerActive={buzzerActive}
      buzzerDur={BUZZER_DUR[st.buzzer] || 1.4}
      pressed={pressed}
      swingEnabled={flags.mouseSwing}
      introLedWait={introPhase === 'wait'}
      introPulse={introPhase === 'play'}
      heartbeatPulse={hbPulse}
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
            customActive={customActive}
            customResetSeconds={customResetSeconds}
            customRemainingMs={customRemainingMs}
            onResetCustom={resetToIdle}
          />
          <DeviceTelemetry st={st} muted={muted} />
          <LanyardDeviceSummary className="ble-device-table--rail" />
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
            customActive={customActive}
            customResetSeconds={customResetSeconds}
            customRemainingMs={customRemainingMs}
            onResetCustom={resetToIdle}
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
          {linkLost && (
            <LinkLostModal
              onClose={clearLinkLost}
              onReconnect={() => {
                clearLinkLost();
                setBleOpen(true);
              }}
            />
          )}
          {bleOpen && <BleConnectModal onClose={() => setBleOpen(false)} />}
          <DeviceLogsSidebar
            open={flags.deviceLogs}
            onClose={() => setFlag('deviceLogs', false)}
          />
        </>
      }
    />
  );
}
