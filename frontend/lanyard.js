/* global React */
// ILSS smart lanyard — 3D-style CSS/SVG rendering with controllable edge LED,
// haptic shake and buzzer audio-wave visualisation.

function Lanyard({ ledRgb, ledPattern, ledOn, hapticPattern, buzzerActive, buzzerDur,
                   onPressPersonal, onPressFire, pressed, swingEnabled }) {
  const e = React.createElement;
  const { useRef, useEffect } = React;
  const leftRef = useRef(null), rightRef = useRef(null);
  const swayRef = useRef(null);

  // ---- mouse-driven swing physics (pendulum: gravity restoring force + damping) ----
  const phys = useRef({ pos: 0, vel: 0, lastX: null, lastT: 0, raf: 0 });
  const swingOn = useRef(swingEnabled);
  swingOn.current = swingEnabled;

  useEffect(() => {
    const sway = swayRef.current;
    if (!sway) return;
    if (!swingEnabled) {
      // hand control back to the gentle CSS idle animation
      cancelAnimationFrame(phys.current.raf);
      phys.current.pos = 0; phys.current.vel = 0; phys.current.lastX = null;
      sway.style.animation = '';
      sway.style.transform = '';
      return;
    }
    const STIFFNESS = 9;      // gravity / restoring strength (deg/s² per deg)
    const DAMPING = 0.9;      // energy loss toward rest (per ~16ms)
    const MAX_ANGLE = 26;     // sensible maximum swing amplitude (deg)
    const MAX_VEL = 280;      // cap angular velocity (deg/s)
    sway.style.animation = 'none';
    let prev = performance.now();
    const tick = (now) => {
      const p = phys.current;
      let dt = (now - prev) / 1000; prev = now;
      if (dt > 0.05) dt = 0.05;
      const acc = -STIFFNESS * p.pos;          // gravity pulls it back to centre
      p.vel += acc * dt;
      p.vel *= Math.pow(DAMPING, dt * 60);     // friction (frame-rate independent)
      p.pos += p.vel * dt;
      if (p.pos > MAX_ANGLE) { p.pos = MAX_ANGLE; if (p.vel > 0) p.vel = 0; }
      if (p.pos < -MAX_ANGLE) { p.pos = -MAX_ANGLE; if (p.vel < 0) p.vel = 0; }
      if (p.vel > MAX_VEL) p.vel = MAX_VEL;
      if (p.vel < -MAX_VEL) p.vel = -MAX_VEL;
      sway.style.transform = `translateY(8px) rotate(${p.pos.toFixed(3)}deg)`;
      p.raf = requestAnimationFrame(tick);
    };
    phys.current.raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(phys.current.raf);
  }, [swingEnabled]);

  const sceneStyle = {
    '--led-rgb': ledRgb,
    '--led-on': ledOn ? 1 : 0,
    '--buzz-dur': buzzerDur + 's',
  };

  // Resolve clicks at the scene level against the buttons' real (projected) screen rects.
  const hitButton = (x, y, pad = 12) => {
    for (const ref of [leftRef, rightRef]) {
      const el = ref.current; if (!el) continue;
      const r = el.getBoundingClientRect();
      if (x >= r.left - pad && x <= r.right + pad && y >= r.top - pad && y <= r.bottom + pad) return true;
    }
    return false;
  };
  const onSceneClick = (ev) => { if (hitButton(ev.clientX, ev.clientY)) onPressPersonal(); };
  // pointer cursor over the buttons + feed swing momentum from recurrent mouse motion
  const onSceneMove = (ev) => {
    ev.currentTarget.style.cursor = hitButton(ev.clientX, ev.clientY, 6) ? 'pointer' : 'default';
    if (!swingOn.current) return;
    const p = phys.current;
    if (p.lastX !== null) {
      let dx = ev.clientX - p.lastX;
      if (dx > 50) dx = 50; else if (dx < -50) dx = -50;
    p.vel += dx * 0.28;   // gentle nudge per pass — builds slowly with recurrent motion
    }
    p.lastX = ev.clientX;
  };
  const onSceneLeave = () => { phys.current.lastX = null; };

  // LED layers — one SVG, viewBox 0 0 232 416, rect normalised to pathLength 1000
  const rectProps = { x: 6, y: 6, width: 220, height: 404, rx: 44, ry: 44, pathLength: 1000 };

  const led = e('svg', { className: 'led-svg', viewBox: '0 0 232 416', 'data-pat': ledPattern },
    e('defs', null,
      e('clipPath', { id: 'clipTop' }, e('rect', { x: 0, y: 0, width: 232, height: 208 })),
      e('clipPath', { id: 'clipBot' }, e('rect', { x: 0, y: 208, width: 232, height: 208 }))
    ),
    // bloom (blurred) + core
    e('rect', { ...rectProps, className: 'led-bloom' }),
    e('rect', { ...rectProps, className: 'led-stroke' }),
    // half-half layers
    e('rect', { ...rectProps, className: 'led-stroke led-half-top', clipPath: 'url(#clipTop)' }),
    e('rect', { ...rectProps, className: 'led-stroke led-half-bot', clipPath: 'url(#clipBot)' })
  );

  return e('div', { className: 'scene' + (buzzerActive ? ' buzz-on' : ''), style: sceneStyle, onClick: onSceneClick, onMouseMove: onSceneMove, onMouseLeave: onSceneLeave },
    e('div', { className: 'scene-halo' }),
    // buzzer audio waves
    e('div', { className: 'buzz-waves' },
      e('div', { className: 'buzz-ring' }),
      e('div', { className: 'buzz-ring' }),
      e('div', { className: 'buzz-ring' })
    ),
    e('div', { className: 'sway', ref: swayRef },
      e('div', { className: 'haptic', 'data-hap': hapticPattern },
        e('div', { className: 'badge' },
          // back strand of the neck loop — passes BEHIND the hook, seen through the cutout
          e('div', { className: 'ns-strand back' }),
          // extruded depth so the device reads as a 3D block
          e('div', { className: 'depth' }),
          e('div', { className: 'side-wall' }),
          // the hook: a single rectangle with a transparent cutout the loop threads through
          e('div', { className: 'clip' }),
          // the doubled strap segment passing THROUGH the cutout (behind the frame, fills the hole)
          e('div', { className: 'ns-bridge' }),
          // front strand of the loop — passes OVER the hook's top bar
          e('div', { className: 'ns-strand front' }),
          e('div', { className: 'body' }),
          e('div', { className: 'led-channel' }),
          led,
          e('div', { className: 'face' },
            e('div', { className: 'face-glow' }),
            // Honeywell wordmark embossed into the device, ILSS beneath
            e('div', { className: 'emboss' },
              e('div', { className: 'emboss-logo', role: 'img', 'aria-label': 'Honeywell' }),
              e('div', { className: 'emboss-ilss' }, 'ILSS')
            )
          ),
          // edge buttons — visual only; clicks/cursor resolved by scene handlers above
          e('button', {
            ref: leftRef, className: 'side-btn left' + (pressed === 'personal' ? ' press' : ''),
            'aria-label': 'Personal alert button'
          }),
          e('button', {
            ref: rightRef, className: 'side-btn right' + (pressed === 'personal' ? ' press' : ''),
            'aria-label': 'Personal alert button'
          }),
          // equaliser bars (buzzer)
          e('div', { className: 'eq' },
            e('span'), e('span'), e('span'), e('span'), e('span'), e('span'), e('span')
          )
        )
      )
    )
  );
}

window.Lanyard = Lanyard;
