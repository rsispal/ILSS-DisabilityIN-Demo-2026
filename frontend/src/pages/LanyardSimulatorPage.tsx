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
import { IDLE, IDLE_DISPLAY_BRIGHTNESS } from '@/lib/constants/patterns';
import { twinStateKey } from '@/lib/ble/twinState';
import { ilssAnalytics } from '@/lib/analytics/ilssAnalytics';
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
  const [logsOpen, setLogsOpen] = useState(false);
  const [logsWidth, setLogsWidth] = useState(() => {
    try {
      const raw = localStorage.getItem('ilss.logsDrawerWidth');
      const n = raw ? Number(raw) : 360;
      return Number.isFinite(n) ? Math.min(640, Math.max(280, n)) : 360;
    } catch {
      return 360;
    }
  });
  const onLogsWidthChange = (w: number) => {
    setLogsWidth(w);
    try {
      localStorage.setItem('ilss.logsDrawerWidth', String(w));
    } catch {
      /* ignore */
    }
  };
  const [bleOpen, setBleOpen] = useState(false);
  const [hbPulse, setHbPulse] = useState(false);
  const [introPhase, setIntroPhase] = useState<'wait' | 'play' | 'done'>('wait');
  /** >0 → swallow that many outbound syncs (remote Event + Status can both apply). */
  const suppressOutbound = useRef(0);
  /** Last twin key applied remotely or sent — blocks echo of identical state. */
  const lastSyncedKey = useRef<string | null>(null);
  const stRef = useRef(st);
  stRef.current = st;

  useEffect(() => {
    const playTimer = window.setTimeout(() => setIntroPhase('play'), 1000);
    // Keep play long enough for 2.6s fade-up / settle before handing off to idle.
    const doneTimer = window.setTimeout(() => setIntroPhase('done'), 4000);
    return () => {
      window.clearTimeout(playTimer);
      window.clearTimeout(doneTimer);
    };
  }, []);

  useAudioOutput(st.buzzer, muted);
  useLanyardScale(sceneRef, isMobile);

  // Feature off → close the pane (flag enables the button, not the open state).
  useEffect(() => {
    if (!flags.deviceLogs) setLogsOpen(false);
  }, [flags.deviceLogs]);

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
      const key = twinStateKey(remote);
      // Already mirroring this exact twin snapshot (Status often duplicates Event).
      if (key === lastSyncedKey.current) return;
      if (remote.alert === 'personal' && cur.alert !== 'personal') {
        ilssAnalytics.personalAlertFromLanyard();
      }
      suppressOutbound.current += 1;
      lastSyncedKey.current = key;
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
    if (suppressOutbound.current > 0) {
      suppressOutbound.current -= 1;
      return;
    }
    const key = twinStateKey(st);
    if (key === lastSyncedKey.current) return;
    const t = window.setTimeout(() => {
      // Re-check after debounce — a remote apply may have landed meanwhile.
      if (suppressOutbound.current > 0) {
        suppressOutbound.current -= 1;
        return;
      }
      if (key !== twinStateKey(stRef.current)) return;
      if (key === lastSyncedKey.current) return;
      lastSyncedKey.current = key;
      void sendTwinState(stRef.current);
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
    ilssAnalytics.fireSimulation();
    firePreset();
  };
  const onPersonal = () => {
    if (st.alert === 'fire') return;
    ilssAnalytics.personalAlertSimulation();
    personalPreset();
  };

  const onMutedChange = (nextMuted: boolean) => {
    ilssAnalytics.audioIndication(!nextMuted);
    if (
      !nextMuted &&
      st.alert === 'none' &&
      st.buzzer !== 'silent' &&
      st.buzzer !== 'off'
    ) {
      ilssAnalytics.customSound({ pattern: st.buzzer });
    }
    setMuted(nextMuted);
  };

  const openLogs = () => {
    ilssAnalytics.logsViewed();
    setLogsOpen(true);
  };

  const onChangeAdvanced = (patch: Partial<DeviceState>) => {
    if (st.alert !== 'none' && (patch.color !== undefined || patch.led !== undefined)) {
      // Advanced LED changes clear alert on web panels already via alert:'none'
    }
    if (patch.color !== undefined || patch.led !== undefined) {
      ilssAnalytics.customLed({
        color: patch.color ?? st.color,
        pattern: patch.led ?? st.led,
        brightness: patch.brightness ?? st.brightness,
      });
    } else if (patch.brightness !== undefined) {
      // Brightness-only: track once per discrete change (slider steps of 10).
      ilssAnalytics.customLed({
        color: st.color,
        pattern: st.led,
        brightness: patch.brightness,
      });
    }
    if (patch.buzzer !== undefined) {
      ilssAnalytics.customBuzzer({ pattern: patch.buzzer });
      if (
        !muted &&
        patch.buzzer !== 'silent' &&
        patch.buzzer !== 'off'
      ) {
        ilssAnalytics.customSound({ pattern: patch.buzzer });
      }
    }
    setCh(patch);
  };

  const ledRgb = COLORS[st.color].rgb;
  const ledOn = st.led !== 'off';
  const buzzerActive = st.buzzer !== 'silent' && st.buzzer !== 'off';
  // Protocol idle is 10% for the lanyard; boost only the on-screen twin.
  const isIdleGreen =
    st.alert === 'none' &&
    st.color === 'green' &&
    st.led === 'solid' &&
    (st.brightness ?? 100) <= IDLE.brightness;
  const ledBrightness = isIdleGreen ? IDLE_DISPLAY_BRIGHTNESS : (st.brightness ?? 100);

  const lanyard = (
    <Lanyard
      ledRgb={ledRgb}
      ledPattern={st.led}
      ledOn={ledOn}
      ledBrightness={ledBrightness}
      hapticPattern={st.haptic}
      buzzerActive={buzzerActive}
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
          <LanyardDeviceSummary
            className="ble-device-table--rail"
            onManageConnection={() => setBleOpen(true)}
          />
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
            onMutedChange={onMutedChange}
          />
        </>
      }
      drawer={
        flags.deviceLogs && logsOpen && !isMobile ? (
          <DeviceLogsSidebar
            open
            onClose={() => setLogsOpen(false)}
            width={logsWidth}
            onWidthChange={onLogsWidthChange}
          />
        ) : undefined
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
            onMutedChange={onMutedChange}
            customActive={customActive}
            customResetSeconds={customResetSeconds}
            customRemainingMs={customRemainingMs}
            onResetCustom={resetToIdle}
            onOpenExperiments={() => setExpOpen(true)}
            onOpenLogs={flags.deviceLogs ? openLogs : undefined}
            onManageConnection={() => setBleOpen(true)}
          />
        ) : undefined
      }
      footer={
        <>
          {!isMobile && (
            <ExperimentsFab
              onOpen={() => setExpOpen(true)}
              showLogs={flags.deviceLogs}
              onOpenLogs={openLogs}
            />
          )}
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
          {flags.deviceLogs && isMobile ? (
            <DeviceLogsSidebar
              open={logsOpen}
              onClose={() => setLogsOpen(false)}
              width={logsWidth}
              onWidthChange={onLogsWidthChange}
              overlay
            />
          ) : null}
        </>
      }
    />
  );
}
