// ILSS buzzer synthesis — Web Audio. Mirrors the buzzer pattern names.
// Exposes window.ILSSAudio.set(pattern) / .setMuted(bool).
window.ILSSAudio = (function () {
  let ctx = null, osc = null, gain = null, timer = null;
  let pattern = 'off', muted = false, nextTime = 0;

  const VOL = 0.10;
  const isSilent = (p) => (p === 'off' || p === 'silent' || muted);

  function ensure() {
    if (!ctx) {
      const AC = window.AudioContext || window.webkitAudioContext;
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

  function gateOn(t)  { gain.gain.setValueAtTime(Math.max(gain.gain.value, 0.0001), t); gain.gain.linearRampToValueAtTime(VOL, t + 0.006); }
  function gateOff(t) { gain.gain.setValueAtTime(VOL, t); gain.gain.linearRampToValueAtTime(0.0001, t + 0.012); }
  const f = (hz, t) => osc.frequency.setValueAtTime(hz, t);
  const sweep = (a, b, t, d) => { osc.frequency.setValueAtTime(a, t); osc.frequency.linearRampToValueAtTime(b, t + d); };

  // program one cycle starting at audio-time t; return cycle duration (s)
  // Frequencies & timings mirror the device firmware (Buzzer.cpp).
  function programCycle(p, t) {
    switch (p) {
      case 'alternating': {        // playAlternating(1000, 740): 250 ms/tone, 2 Hz, continuous
        gateOn(t);
        f(1000, t);
        f(740, t + 0.25);
        return 0.5;
      }
      case 'bs-sweep': {           // playMediumSweep(800, 970): 1 Hz upward sawtooth, continuous
        gateOn(t);
        sweep(800, 970, t, 1.0);
        return 1.0;
      }
      case 'bs-fast-sweep': {      // fast upward sweep 800->970, continuous (~3.3 Hz)
        gateOn(t);
        sweep(800, 970, t, 0.3);
        return 0.3;
      }
      case 'siren': {              // playSiren(2700, 3500): 7-step up + 7-step down, ~7 Hz warble
        gateOn(t);
        sweep(2700, 3500, t, 0.07);
        sweep(3500, 2700, t + 0.07, 0.07);
        return 0.14;
      }
      case 'lf-buzz': {            // playLFBuzz(800, 970): harsh fire-horn buzz, 800/850 Hz toggled every 10 ms
        gateOn(t);
        for (let k = 0; k < 10; k++) f((k % 2) ? 850 : 800, t + k * 0.01);
        return 0.1;
      }
      case 'code3-beep': {         // playCode3Temporal(3000): Temporal-3 — 0.5 on/0.5 off x3, then 1.5 s
        f(3000, t);
        for (let i = 0; i < 3; i++) {
          const s = t + i * 1.0;   // ON_DURATION 500 + OFF_BETWEEN 500
          gateOn(s);
          gateOff(s + 0.5);
        }
        return 4.0;                // 3x(0.5+0.5) + OFF_AFTER_THIRD extra 1.0 = 4.0 s
      }
      case 'code3-sweep': {        // playCode3Sweep(2700, 3500): Temporal-3 with a rising sweep each pulse
        for (let i = 0; i < 3; i++) {
          const s = t + i * 1.0;
          gateOn(s);
          sweep(2700, 3500, s, 0.5);   // 500 ms upward sweep per pulse
          gateOff(s + 0.5);
        }
        return 4.0;
      }
      case 'code3-siren': {        // playCode3Siren(2700, 3500): Temporal-3 with siren-warble pulses
        for (let i = 0; i < 3; i++) {
          const s = t + i * 1.0;
          gateOn(s);
          for (let k = 0; k < 4; k++) {   // ~3.5 up/down warbles inside the 500 ms pulse
            const w = s + k * 0.14;
            sweep(2700, 3500, w, 0.07);
            sweep(3500, 2700, w + 0.07, 0.07);
          }
          gateOff(s + 0.5);
        }
        return 4.0;
      }
      default:
        gain.gain.setValueAtTime(0, t);
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
    if (timer) { clearInterval(timer); timer = null; }
    if (gain) { gain.gain.cancelScheduledValues(ctx.currentTime); gain.gain.setValueAtTime(0, ctx.currentTime); }
  }

  return {
    set(p) {
      pattern = p || 'off';
      if (isSilent(pattern)) { halt(); return; }
      ensure();
      // clear any events still scheduled from the previous pattern (Temporal-3 queues ~4 s ahead)
      gain.gain.cancelScheduledValues(ctx.currentTime);
      gain.gain.setValueAtTime(0, ctx.currentTime);
      osc.frequency.cancelScheduledValues(ctx.currentTime);
      nextTime = ctx.currentTime + 0.05;
      run();
    },
    setMuted(m) {
      muted = !!m;
      if (muted) halt();
      else if (!isSilent(pattern)) { ensure(); nextTime = ctx.currentTime + 0.05; run(); }
    },
  };
})();
