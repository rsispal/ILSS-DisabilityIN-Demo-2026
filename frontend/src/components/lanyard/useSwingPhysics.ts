import { useEffect, useRef, type RefObject } from 'react';

interface PhysicsState {
  pos: number;
  vel: number;
  lastX: number | null;
  lastT: number;
  raf: number;
}

export function useSwingPhysics(swingEnabled: boolean, swayRef: RefObject<HTMLDivElement | null>) {
  const phys = useRef<PhysicsState>({ pos: 0, vel: 0, lastX: null, lastT: 0, raf: 0 });
  const swingOn = useRef(swingEnabled);
  swingOn.current = swingEnabled;

  useEffect(() => {
    const sway = swayRef.current;
    if (!sway) return;

    if (!swingEnabled) {
      cancelAnimationFrame(phys.current.raf);
      phys.current.pos = 0;
      phys.current.vel = 0;
      phys.current.lastX = null;
      sway.style.animation = '';
      sway.style.transform = '';
      return;
    }

    const STIFFNESS = 9;
    const DAMPING = 0.9;
    const MAX_ANGLE = 26;
    const MAX_VEL = 280;
    sway.style.animation = 'none';
    let prev = performance.now();

    const tick = (now: number) => {
      const p = phys.current;
      let dt = (now - prev) / 1000;
      prev = now;
      if (dt > 0.05) dt = 0.05;
      const acc = -STIFFNESS * p.pos;
      p.vel += acc * dt;
      p.vel *= Math.pow(DAMPING, dt * 60);
      p.pos += p.vel * dt;
      if (p.pos > MAX_ANGLE) {
        p.pos = MAX_ANGLE;
        if (p.vel > 0) p.vel = 0;
      }
      if (p.pos < -MAX_ANGLE) {
        p.pos = -MAX_ANGLE;
        if (p.vel < 0) p.vel = 0;
      }
      if (p.vel > MAX_VEL) p.vel = MAX_VEL;
      if (p.vel < -MAX_VEL) p.vel = -MAX_VEL;
      sway.style.transform = `translateY(8px) rotate(${p.pos.toFixed(3)}deg)`;
      p.raf = requestAnimationFrame(tick);
    };

    phys.current.raf = requestAnimationFrame(tick);
    return () => cancelAnimationFrame(phys.current.raf);
  }, [swingEnabled, swayRef]);

  const feedMomentum = (clientX: number) => {
    if (!swingOn.current) return;
    const p = phys.current;
    if (p.lastX !== null) {
      let dx = clientX - p.lastX;
      if (dx > 50) dx = 50;
      else if (dx < -50) dx = -50;
      p.vel += dx * 0.28;
    }
    p.lastX = clientX;
  };

  const clearMomentum = () => {
    phys.current.lastX = null;
  };

  return { feedMomentum, clearMomentum, phys };
}
