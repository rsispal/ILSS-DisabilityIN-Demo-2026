/* global React, ReactDOM */
// BLE connection popout — invokes Web Bluetooth to pair/connect/disconnect an ILSS lanyard.
// Wrapped in an IIFE so its top-level consts don't collide with app.js in the shared global scope.
(function () {
  const { useState, useEffect, useRef, useCallback } = React;
  const e = React.createElement;

  const BtIcon = (p) => e('svg', { viewBox: '0 0 24 24', fill: 'none', stroke: 'currentColor',
    strokeWidth: 2, strokeLinecap: 'round', strokeLinejoin: 'round', ...p },
    e('path', { d: 'M7 8l10 8-5 4V4l5 4-10 8' }));

  const ChevIcon = (p) => e('svg', { viewBox: '0 0 24 24', fill: 'none', stroke: 'currentColor',
    strokeWidth: 2, strokeLinecap: 'round', strokeLinejoin: 'round', ...p },
    e('path', { d: 'M6 9l6 6 6-6' }));

  function BleConnect() {
    const F = window.ForgeCommon || {};
    const Button = F.Button;
    const [open, setOpen] = useState(false);
    const [status, setStatus] = useState('disconnected'); // disconnected | connecting | connected
    const [name, setName] = useState(null);
    const [msg, setMsg] = useState('');
    const deviceRef = useRef(null);
    const wrapRef = useRef(null);

    const supported = typeof navigator !== 'undefined' && !!navigator.bluetooth;

    useEffect(() => {
      if (!open) return undefined;
      const onDoc = (ev) => { if (wrapRef.current && !wrapRef.current.contains(ev.target)) setOpen(false); };
      const onKey = (ev) => { if (ev.key === 'Escape') setOpen(false); };
      document.addEventListener('mousedown', onDoc);
      document.addEventListener('keydown', onKey);
      return () => { document.removeEventListener('mousedown', onDoc); document.removeEventListener('keydown', onKey); };
    }, [open]);

    const onDisconnected = useCallback(() => {
      setStatus('disconnected'); setName(null); setMsg('Device disconnected');
    }, []);

    async function pair() {
      setMsg('');
      if (!navigator.bluetooth) { setMsg('Web Bluetooth isn’t available in this browser.'); return; }
      try {
        setStatus('connecting');
        const device = await navigator.bluetooth.requestDevice({
          // Surface every nearby device; the ILSS lanyard advertises these services
          acceptAllDevices: true,
          optionalServices: ['battery_service', 'device_information'],
        });
        deviceRef.current = device;
        device.addEventListener('gattserverdisconnected', onDisconnected);
        await device.gatt.connect();
        setName(device.name || 'ILSS Lanyard');
        setStatus('connected');
        setMsg('');
      } catch (err) {
        setStatus('disconnected');
        if (err && err.name === 'NotFoundError') setMsg('No device selected.');
        else if (err && err.name === 'SecurityError') setMsg('Bluetooth is blocked by the browser’s permissions here.');
        else setMsg((err && err.message) || 'Pairing failed.');
      }
    }

    function simulate() {
      setMsg('');
      setStatus('connecting');
      setTimeout(() => { setName('ILSS-LY · 04A7-22F1'); setStatus('connected'); }, 750);
    }

    function disconnect() {
      const d = deviceRef.current;
      try { if (d && d.gatt && d.gatt.connected) d.gatt.disconnect(); } catch (e2) { /* ignore */ }
      deviceRef.current = null;
      setStatus('disconnected'); setName(null); setMsg('');
    }

    const dotClass = status === 'connected' ? 'on' : status === 'connecting' ? 'busy' : 'off';
    const chipText = status === 'connected' ? 'BLE · Connected'
      : status === 'connecting' ? 'BLE · Connecting…'
      : 'BLE · Disconnected';

    // ---- popout body ----
    let body;
    if (status === 'connected') {
      body = e('div', null,
        e('div', { className: 'ble-device' },
          e('span', { className: 'ble-device-ic' }, e(BtIcon, { style: { width: 18, height: 18 } })),
          e('div', { className: 'ble-device-meta' },
            e('div', { className: 'ble-device-name' }, name || 'ILSS Lanyard'),
            e('div', { className: 'ble-device-sub' }, 'GATT server connected')),
          e('span', { className: 'ble-pill ok' }, 'Live')),
        e('div', { className: 'ble-actions' },
          Button
            ? e(Button, { variant: 'destructive-secondary', size: 'regular', onClick: disconnect, style: { width: '100%' } }, 'Disconnect')
            : e('button', { className: 'ble-btn danger', onClick: disconnect }, 'Disconnect')));
    } else {
      const connecting = status === 'connecting';
      body = e('div', null,
        e('p', { className: 'ble-lead' },
          'Pair your ILSS lanyard over Bluetooth Low Energy to mirror its live alert state.'),
        e('div', { className: 'ble-actions' },
          Button
            ? e(Button, { variant: 'primary', size: 'regular', onClick: pair, disabled: connecting, style: { width: '100%' } },
                connecting ? 'Connecting…' : 'Pair device')
            : e('button', { className: 'ble-btn primary', onClick: pair, disabled: connecting },
                connecting ? 'Connecting…' : 'Pair device')),
        !supported && e('div', { className: 'ble-fallback' },
          e('span', { className: 'ble-note' }, 'Web Bluetooth unavailable in this view.'),
          Button
            ? e(Button, { variant: 'quiet-secondary', size: 'regular', onClick: simulate, disabled: connecting, style: { width: '100%' } }, 'Simulate connection')
            : e('button', { className: 'ble-btn quiet', onClick: simulate, disabled: connecting }, 'Simulate connection')));
    }

    return e('div', { className: 'ble-wrap', ref: wrapRef },
      e('button', {
        className: 'ble-chip', onClick: () => setOpen((o) => !o),
        'aria-haspopup': 'dialog', 'aria-expanded': open,
      },
        e('span', { className: 'ble-dot ' + dotClass }),
        e('span', { className: 'ble-chip-text' }, chipText),
        e(ChevIcon, { className: 'ble-chev' + (open ? ' up' : ''), style: { width: 13, height: 13 } })),
      open && e('div', { className: 'ble-pop', role: 'dialog', 'aria-label': 'Bluetooth connection' },
        e('div', { className: 'ble-pop-head' },
          e('span', { className: 'ble-pop-ic' }, e(BtIcon, { style: { width: 16, height: 16 } })),
          e('div', null,
            e('div', { className: 'ble-pop-title' }, 'Bluetooth'),
            e('div', { className: 'ble-pop-sub' }, 'ILSS smart lanyard'))),
        body,
        msg && e('div', { className: 'ble-msg' }, msg)));
  }

  window.BleConnect = BleConnect;
})();
