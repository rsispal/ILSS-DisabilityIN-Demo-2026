import { useRef, type CSSProperties, type MouseEvent } from 'react';
import { useSwingPhysics } from './useSwingPhysics';
import type { HapticPattern, LedPattern, PressedButton } from '@/types/simulator';

interface LanyardProps {
  ledRgb: string;
  ledPattern: LedPattern;
  ledOn: boolean;
  ledBrightness?: number;
  hapticPattern: HapticPattern;
  buzzerActive: boolean;
  pressed: PressedButton;
  swingEnabled: boolean;
  introLedWait?: boolean;
  introPulse?: boolean;
  /** Brief green LED pulse when a heartbeat poll is received. */
  heartbeatPulse?: boolean;
  onPressPersonal: () => void;
  onPressFire: () => void;
}

export function Lanyard({
  ledRgb,
  ledPattern,
  ledOn,
  ledBrightness = 100,
  hapticPattern,
  buzzerActive,
  pressed,
  swingEnabled,
  introLedWait = false,
  introPulse = false,
  heartbeatPulse = false,
  onPressPersonal,
  onPressFire: _onPressFire,
}: LanyardProps) {
  const leftRef = useRef<HTMLButtonElement>(null);
  const rightRef = useRef<HTMLButtonElement>(null);
  const swayRef = useRef<HTMLDivElement>(null);

  const { feedMomentum, clearMomentum } = useSwingPhysics(swingEnabled, swayRef);

  const displayLedPattern = introLedWait ? 'off' : heartbeatPulse ? 'solid' : ledPattern;
  const idleBright = Math.max(0, Math.min(1, ledBrightness / 100));
  // Intro keeps --led-on at 0 (no face-glow); CSS owns SVG opacity for the pulse.
  const displayLedOn =
    introLedWait || introPulse
      ? false
      : heartbeatPulse
        ? true
        : ledOn && ledBrightness > 0;
  const displayLedRgb = heartbeatPulse ? '0, 220, 48' : ledRgb;
  const displayLedBright = heartbeatPulse ? 1 : idleBright;

  const sceneClasses = [
    'scene',
    buzzerActive ? 'buzz-on' : '',
    introPulse ? 'intro-pulse' : '',
    heartbeatPulse ? 'hb-pulse' : '',
  ]
    .filter(Boolean)
    .join(' ');

  // Buzz rings use a fixed CSS duration (--buzz-dur default 1.4s) — not tied to
  // audio cycle length, which is far too fast for patterns like bs-fast-sweep.
  const sceneStyle = {
    '--led-rgb': displayLedRgb,
    '--led-on': displayLedOn ? 1 : 0,
    '--led-bright': displayLedBright,
  } as CSSProperties;

  const hitButton = (x: number, y: number, pad = 12) => {
    for (const ref of [leftRef, rightRef]) {
      const el = ref.current;
      if (!el) continue;
      const r = el.getBoundingClientRect();
      if (x >= r.left - pad && x <= r.right + pad && y >= r.top - pad && y <= r.bottom + pad) {
        return true;
      }
    }
    return false;
  };

  const onSceneClick = (ev: MouseEvent<HTMLDivElement>) => {
    if (hitButton(ev.clientX, ev.clientY)) onPressPersonal();
  };

  const onSceneMove = (ev: MouseEvent<HTMLDivElement>) => {
    ev.currentTarget.style.cursor = hitButton(ev.clientX, ev.clientY, 6) ? 'pointer' : 'default';
    feedMomentum(ev.clientX);
  };

  const onSceneLeave = () => {
    clearMomentum();
  };

  const rectProps = {
    x: 6,
    y: 6,
    width: 220,
    height: 404,
    rx: 44,
    ry: 44,
    pathLength: 1000,
  };

  return (
    <div
      className={sceneClasses}
      style={sceneStyle}
      onClick={onSceneClick}
      onMouseMove={onSceneMove}
      onMouseLeave={onSceneLeave}
    >
      <div className="scene-halo" />
      <div className="buzz-waves">
        <div className="buzz-ring" />
        <div className="buzz-ring" />
        <div className="buzz-ring" />
      </div>
      <div className="sway" ref={swayRef}>
        <div className="haptic" data-hap={hapticPattern}>
          <div className="badge">
            <div className="ns-strand back" />
            <div className="depth" />
            <div className="side-wall" />
            <div className="clip" />
            <div className="ns-bridge" />
            <div className="ns-strand front" />
            <div className="body" />
            <div className="led-channel" />
            <svg className="led-svg" viewBox="0 0 232 416" data-pat={displayLedPattern}>
              <defs>
                <clipPath id="clipTop">
                  <rect x={0} y={0} width={232} height={208} />
                </clipPath>
                <clipPath id="clipBot">
                  <rect x={0} y={208} width={232} height={208} />
                </clipPath>
              </defs>
              <rect {...rectProps} className="led-bloom" />
              <rect {...rectProps} className="led-stroke" />
              <rect
                {...rectProps}
                className="led-stroke led-half-top"
                clipPath="url(#clipTop)"
              />
              <rect
                {...rectProps}
                className="led-stroke led-half-bot"
                clipPath="url(#clipBot)"
              />
            </svg>
            <div className="face">
              <div className="face-glow" />
              <div className="emboss">
                <div className="emboss-logo" role="img" aria-label="Honeywell" />
                <div className="emboss-ilss">ILSS</div>
              </div>
            </div>
            <button
              type="button"
              ref={leftRef}
              className={
                'side-btn left' +
                (pressed === 'personal' || pressed === 'left' ? ' press' : '') +
                (pressed === 'left' ? ' lit' : '')
              }
              aria-label="Wearer left button"
            />
            <button
              type="button"
              ref={rightRef}
              className={
                'side-btn right' +
                (pressed === 'personal' || pressed === 'right' ? ' press' : '') +
                (pressed === 'right' ? ' lit' : '')
              }
              aria-label="Wearer right button"
            />
            <div className="eq">
              <span />
              <span />
              <span />
              <span />
              <span />
              <span />
              <span />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
