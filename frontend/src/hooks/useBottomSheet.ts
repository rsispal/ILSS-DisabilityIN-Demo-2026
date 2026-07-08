import { useCallback, useEffect, useRef, useState, type PointerEvent as ReactPointerEvent } from 'react';

export type SheetSnap = 'collapsed' | 'open' | 'expanded';

const COLLAPSED_H = 56;
const OPEN_H = 380;

function expandedHeight() {
  return Math.min(Math.round(window.innerHeight * 0.84), 720);
}

function nearestSnap(height: number): SheetSnap {
  const expanded = expandedHeight();
  const distances: [SheetSnap, number][] = [
    ['collapsed', Math.abs(height - COLLAPSED_H)],
    ['open', Math.abs(height - OPEN_H)],
    ['expanded', Math.abs(height - expanded)],
  ];
  distances.sort((a, b) => a[1] - b[1]);
  return distances[0][0];
}

function heightForSnap(snap: SheetSnap): number {
  if (snap === 'collapsed') return COLLAPSED_H;
  if (snap === 'expanded') return expandedHeight();
  return OPEN_H;
}

export function useBottomSheet() {
  const [snap, setSnap] = useState<SheetSnap>('open');
  const [height, setHeight] = useState(OPEN_H);
  const [advancedOpen, setAdvancedOpen] = useState(false);
  const [dragging, setDragging] = useState(false);
  const drag = useRef({ active: false, startY: 0, startH: OPEN_H, moved: false });

  const applySnap = useCallback((next: SheetSnap) => {
    const adv = next === 'expanded';
    setAdvancedOpen(adv);
    setSnap(next);
    setHeight(heightForSnap(next));
  }, []);

  const onPointerDown = useCallback((e: ReactPointerEvent<HTMLDivElement>) => {
    drag.current = { active: true, startY: e.clientY, startH: height, moved: false };
    setDragging(true);
    e.currentTarget.setPointerCapture(e.pointerId);
  }, [height]);

  const onPointerMove = useCallback((e: ReactPointerEvent<HTMLDivElement>) => {
    if (!drag.current.active) return;
    const dy = drag.current.startY - e.clientY;
    if (Math.abs(dy) > 4) drag.current.moved = true;
    const max = expandedHeight();
    const next = Math.max(COLLAPSED_H, Math.min(max, drag.current.startH + dy));
    setHeight(next);
  }, []);

  const onPointerUp = useCallback(() => {
    if (!drag.current.active) return;
    drag.current.active = false;
    setDragging(false);
    applySnap(nearestSnap(height));
  }, [height, applySnap]);

  const onHandleClick = useCallback(() => {
    if (drag.current.moved) return;
    if (snap === 'collapsed') applySnap('open');
  }, [snap, applySnap]);

  const toggleAdvanced = useCallback(() => {
    if (advancedOpen) {
      applySnap('open');
    } else {
      applySnap('expanded');
    }
  }, [advancedOpen, applySnap]);

  const collapse = useCallback(() => applySnap('collapsed'), [applySnap]);

  useEffect(() => {
    const onResize = () => {
      if (snap === 'expanded') setHeight(expandedHeight());
    };
    window.addEventListener('resize', onResize);
    return () => window.removeEventListener('resize', onResize);
  }, [snap]);

  return {
    snap,
    height,
    advancedOpen,
    dragging,
    collapsed: snap === 'collapsed',
    handleProps: {
      onPointerDown,
      onPointerMove,
      onPointerUp,
      onPointerCancel: onPointerUp,
      onClick: onHandleClick,
    },
    toggleAdvanced,
    collapse,
  };
}
