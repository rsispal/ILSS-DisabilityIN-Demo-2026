// ILSS buzzer synthesis — Web Audio. Mirrors the buzzer pattern names.

let ctx: AudioContext | null = null;
let osc: OscillatorNode | null = null;
let gain: GainNode | null = null;
let timer: ReturnType<typeof setInterval> | null = null;
let pattern = 'off';
let muted = false;
let nextTime = 0;

const VOL = 0.1;
const isSilent = (p: string) => p === 'off' || p === 'silent' || muted;

function ensure() {
  if (!ctx) {
    const AC = window.AudioContext || (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
    ctx = new AC();
    gain = ctx.createGain();
    gain.gain.value = 0;
    gain.connect(ctx.destination);
    osc = ctx.createOscillator();
    osc.type = 'square';
    osc.frequency.value = 800;
    osc.connect(gain);
    osc.start();
  }
  if (ctx.state === 'suspended') ctx.resume();
}

function gateOn(t: number) {
  if (!gain) return;
  gain.gain.setValueAtTime(Math.max(gain.gain.value, 0.0001), t);
  gain.gain.linearRampToValueAtTime(VOL, t + 0.006);
}

function gateOff(t: number) {
  if (!gain) return;
  gain.gain.setValueAtTime(VOL, t);
  gain.gain.linearRampToValueAtTime(0.0001, t + 0.012);
}

function setFreq(hz: number, t: number) {
  osc?.frequency.setValueAtTime(hz, t);
}

function sweep(a: number, b: number, t: number, d: number) {
  if (!osc) return;
  osc.frequency.setValueAtTime(a, t);
  osc.frequency.linearRampToValueAtTime(b, t + d);
}

function programCycle(p: string, t: number): number {
  switch (p) {
    case 'alternating': {
      gateOn(t);
      setFreq(1000, t);
      setFreq(740, t + 0.25);
      return 0.5;
    }
    case 'bs-sweep': {
      gateOn(t);
      sweep(800, 970, t, 1.0);
      return 1.0;
    }
    case 'bs-fast-sweep': {
      gateOn(t);
      sweep(800, 970, t, 0.3);
      return 0.3;
    }
    case 'siren': {
      gateOn(t);
      sweep(2700, 3500, t, 0.07);
      sweep(3500, 2700, t + 0.07, 0.07);
      return 0.14;
    }
    case 'lf-buzz': {
      gateOn(t);
      for (let k = 0; k < 10; k++) setFreq(k % 2 ? 850 : 800, t + k * 0.01);
      return 0.1;
    }
    case 'code3-beep': {
      setFreq(3000, t);
      for (let i = 0; i < 3; i++) {
        const s = t + i * 1.0;
        gateOn(s);
        gateOff(s + 0.5);
      }
      return 4.0;
    }
    case 'code3-sweep': {
      for (let i = 0; i < 3; i++) {
        const s = t + i * 1.0;
        gateOn(s);
        sweep(2700, 3500, s, 0.5);
        gateOff(s + 0.5);
      }
      return 4.0;
    }
    case 'code3-siren': {
      for (let i = 0; i < 3; i++) {
        const s = t + i * 1.0;
        gateOn(s);
        for (let k = 0; k < 4; k++) {
          const w = s + k * 0.14;
          sweep(2700, 3500, w, 0.07);
          sweep(3500, 2700, w + 0.07, 0.07);
        }
        gateOff(s + 0.5);
      }
      return 4.0;
    }
    default:
      if (gain) gain.gain.setValueAtTime(0, t);
      return 0.3;
  }
}

function tick() {
  if (!ctx) return;
  while (nextTime < ctx.currentTime + 0.3) {
    nextTime += programCycle(pattern, nextTime);
  }
}

function run() {
  if (timer) return;
  timer = setInterval(tick, 60);
}

function halt() {
  if (timer) {
    clearInterval(timer);
    timer = null;
  }
  if (gain && ctx) {
    gain.gain.cancelScheduledValues(ctx.currentTime);
    gain.gain.setValueAtTime(0, ctx.currentTime);
  }
}

export const ilssAudio = {
  set(p: string) {
    pattern = p || 'off';
    if (isSilent(pattern)) {
      halt();
      return;
    }
    ensure();
    if (gain && ctx && osc) {
      gain.gain.cancelScheduledValues(ctx.currentTime);
      gain.gain.setValueAtTime(0, ctx.currentTime);
      osc.frequency.cancelScheduledValues(ctx.currentTime);
      nextTime = ctx.currentTime + 0.05;
      run();
    }
  },
  setMuted(m: boolean) {
    muted = !!m;
    if (muted) halt();
    else if (!isSilent(pattern) && ctx) {
      ensure();
      nextTime = ctx.currentTime + 0.05;
      run();
    }
  },
};
