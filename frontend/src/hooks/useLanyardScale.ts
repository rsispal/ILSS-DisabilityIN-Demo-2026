import { useEffect, type RefObject } from 'react';

const BASE_W = 340;
const BASE_H = 500;
const DEFAULT_SCALE = 1.15;

/** Fit the 3D lanyard scene inside its container on small screens. */
export function useLanyardScale(
  containerRef: RefObject<HTMLDivElement>,
  enabled: boolean,
) {
  useEffect(() => {
    if (!enabled) return;
    const el = containerRef.current;
    if (!el) return;

    const update = () => {
      const w = el.clientWidth;
      const h = el.clientHeight;
      if (!w || !h) return;
      const contentW = BASE_W * DEFAULT_SCALE;
      const contentH = BASE_H * DEFAULT_SCALE;
      const scale = Math.min(w / contentW, h / contentH, DEFAULT_SCALE) * 0.92;
      el.style.setProperty('--lanyard-scale', scale.toFixed(3));
    };

    update();
    const ro = new ResizeObserver(update);
    ro.observe(el);
    window.addEventListener('orientationchange', update);
    return () => {
      ro.disconnect();
      window.removeEventListener('orientationchange', update);
      el.style.removeProperty('--lanyard-scale');
    };
  }, [containerRef, enabled]);
}
