import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from 'react';
import type { DeviceState } from '@/types/simulator';
import {
  UUID_TWIN_SVC,
  UUID_META_SVC,
  UUID_CMD,
  UUID_EVENT,
  UUID_STATUS,
  UUID_PAIRING,
  UUID_LOG,
  UUID_SERIAL,
  UUID_MODEL,
  UUID_SWVER,
  UUID_BRAND,
  UUID_BATT,
  OPTIONAL_SERVICES,
} from '@/lib/ble/uuids';
import {
  decodePacket,
  encodePacket,
  FLAG_CMD,
  FLAG_EVT,
  FLAG_ACK,
  FLAG_NAK,
  FLAG_RPL,
  nullUuid,
  randomUuidBytes,
  type Packet,
} from '@/lib/ble/packetCodec';
import {
  APP_CODE_BUTTON,
  APP_CODE_HEARTBEAT,
  APP_CODE_TWIN_STATE,
  BUTTON_SIDE_LEFT,
  packTwinState,
  unpackTwinState,
  isAdvancedPatch,
} from '@/lib/ble/twinState';
import { parseDeviceLog, type ParsedDeviceLog } from '@/lib/ble/parseDeviceLog';
import { ilssAnalytics } from '@/lib/analytics/ilssAnalytics';

export type BleTwinStatus = 'disconnected' | 'connecting' | 'connected';

export interface DeviceMetadata {
  serial: string;
  model: string;
  software: string;
  brand: number;
  battery: number;
}

export interface DeviceLogLine extends ParsedDeviceLog {
  id: number;
  ts: number;
  /** Raw line (copy / fallback) */
  text: string;
}

interface BleTwinContextValue {
  status: BleTwinStatus;
  supported: boolean;
  name: string | null;
  msg: string;
  metadata: DeviceMetadata | null;
  logs: DeviceLogLine[];
  paired: boolean;
  clearLogs: () => void;
  /** Client-side BLE/GATT tracer (same ring buffer as device log notify). */
  logBle: (text: string) => void;
  pair: () => Promise<void>;
  simulate: () => void;
  disconnect: () => void;
  unpair: () => Promise<void>;
  sendTwinState: (st: DeviceState) => Promise<boolean>;
  pollHeartbeat: () => Promise<boolean>;
  /** Called by page to register inbound twin state handler */
  setOnRemoteState: (fn: ((st: DeviceState) => void) | null) => void;
  /** Momentary physical side-button press from the lanyard (left/right). */
  setOnButtonPress: (fn: ((side: 'left' | 'right') => void) | null) => void;
  /** Fired when a device→web heartbeat poll arrives (or web poll is answered). */
  setOnHeartbeat: (fn: (() => void) | null) => void;
  /** True when the link watchdog detected a silent / unresponsive lanyard. */
  linkLost: boolean;
  clearLinkLost: () => void;
}

const BleTwinContext = createContext<BleTwinContextValue | null>(null);

const LOG_CAP = 1000;

function errDetail(err: unknown): string {
  const e = err as DOMException & Error;
  const name = e?.name || 'Error';
  const message = e?.message || String(err);
  return `${name}: ${message}`;
}

function isGattFailure(err: unknown): boolean {
  const msg = ((err as Error)?.message || '').toLowerCase();
  return msg.includes('gatt operation failed') || msg.includes('gatt server is disconnected');
}

async function readString(char: BluetoothRemoteGATTCharacteristic): Promise<string> {
  const v = await char.readValue();
  return new TextDecoder().decode(v.buffer);
}

async function readU8(char: BluetoothRemoteGATTCharacteristic): Promise<number> {
  const v = await char.readValue();
  return v.getUint8(0);
}

function hexPreview(buf: Uint8Array, max = 24): string {
  const n = Math.min(buf.length, max);
  let s = '';
  for (let i = 0; i < n; i++) s += buf[i].toString(16).padStart(2, '0');
  if (buf.length > max) s += `…(+${buf.length - max})`;
  return s;
}

export function BleTwinProvider({ children }: { children: ReactNode }) {
  const [status, setStatus] = useState<BleTwinStatus>('disconnected');
  const [name, setName] = useState<string | null>(null);
  const [msg, setMsg] = useState('');
  const [metadata, setMetadata] = useState<DeviceMetadata | null>(null);
  const [logs, setLogs] = useState<DeviceLogLine[]>([]);
  const [paired, setPaired] = useState(false);
  const [linkLost, setLinkLost] = useState(false);

  const deviceRef = useRef<BluetoothDevice | null>(null);
  const serverRef = useRef<BluetoothRemoteGATTServer | null>(null);
  const cmdCharRef = useRef<BluetoothRemoteGATTCharacteristic | null>(null);
  const pairingCharRef = useRef<BluetoothRemoteGATTCharacteristic | null>(null);
  const masterUuid = useRef(randomUuidBytes());
  const msgId = useRef(1);
  const logId = useRef(1);
  const onRemoteRef = useRef<((st: DeviceState) => void) | null>(null);
  const onButtonPressRef = useRef<((side: 'left' | 'right') => void) | null>(null);
  const onHeartbeatRef = useRef<(() => void) | null>(null);
  const lastHeartbeatAtRef = useRef(0);
  const awaitingWebPollRef = useRef<{ msgId: number; since: number } | null>(null);
  const deviceUuidRef = useRef(nullUuid());
  const writeChainRef = useRef<Promise<unknown>>(Promise.resolve());
  const connectingRef = useRef(false);
  const statusRef = useRef<BleTwinStatus>('disconnected');
  statusRef.current = status;
  const writePacketRef = useRef<(pkt: Packet) => Promise<boolean>>(async () => false);
  const forceLinkLostRef = useRef<(why: string) => void>(() => undefined);
  const suppressDisconnectMsgRef = useRef(false);
  /** Avoid double-reporting pairing analytics when disconnect + catch both fire. */
  const pairingAnalyticsSentRef = useRef(false);

  const supported = typeof navigator !== 'undefined' && !!navigator.bluetooth;

  const appendLog = useCallback((text: string) => {
    const id = logId.current++;
    const parsed = parseDeviceLog(text);
    setLogs((prev) => {
      const next = [...prev, { id, ts: Date.now(), text, ...parsed }];
      return next.length > LOG_CAP ? next.slice(-LOG_CAP) : next;
    });
  }, []);

  const logBle = useCallback(
    (text: string) => {
      appendLog(text);
    },
    [appendLog],
  );

  const logGattErr = useCallback(
    (op: string, err: unknown, alsoSetMsg = true) => {
      const detail = errDetail(err);
      appendLog(`E (ble) ${op} failed [${detail}]`);
      if (alsoSetMsg) {
        if (isGattFailure(err)) {
          setMsg(
            `${op} failed — Chrome GATT error (often disconnect/race). Check Device logs.`,
          );
        } else {
          setMsg((err as Error)?.message || `${op} failed`);
        }
      }
    },
    [appendLog],
  );

  const clearLogs = useCallback(() => setLogs([]), []);

  const setOnRemoteState = useCallback((fn: ((st: DeviceState) => void) | null) => {
    onRemoteRef.current = fn;
  }, []);

  const setOnButtonPress = useCallback((fn: ((side: 'left' | 'right') => void) | null) => {
    onButtonPressRef.current = fn;
  }, []);

  const setOnHeartbeat = useCallback((fn: (() => void) | null) => {
    onHeartbeatRef.current = fn;
  }, []);

  const clearLinkLost = useCallback(() => setLinkLost(false), []);

  const onDisconnected = useCallback(() => {
    const wasConnecting = connectingRef.current;
    connectingRef.current = false;
    awaitingWebPollRef.current = null;
    lastHeartbeatAtRef.current = 0;
    appendLog(
      wasConnecting
        ? 'W (ble) gattserverdisconnected during connect/setup'
        : 'W (ble) gattserverdisconnected',
    );
    if (wasConnecting && !pairingAnalyticsSentRef.current) {
      pairingAnalyticsSentRef.current = true;
      ilssAnalytics.pairingIssue({
        reason: 'timeout',
        message: 'Disconnected during connect/setup',
      });
    }
    setStatus('disconnected');
    setName(null);
    setPaired(false);
    if (!suppressDisconnectMsgRef.current) setMsg('Device disconnected');
    suppressDisconnectMsgRef.current = false;
    cmdCharRef.current = null;
    pairingCharRef.current = null;
    serverRef.current = null;
  }, [appendLog]);

  const noteHeartbeat = useCallback(
    (source: string) => {
      lastHeartbeatAtRef.current = Date.now();
      awaitingWebPollRef.current = null;
      appendLog(`I (ble) heartbeat alive (${source})`);
      onHeartbeatRef.current?.();
    },
    [appendLog],
  );

  const forceLinkLost = useCallback(
    (why: string) => {
      appendLog(`E (ble) link lost — ${why}`);
      setLinkLost(true);
      setMsg('Connection lost. Reset the lanyard, then pair again.');
      suppressDisconnectMsgRef.current = true;
      const d = deviceRef.current;
      try {
        if (d?.gatt?.connected) d.gatt.disconnect();
      } catch {
        /* ignore */
      }
      deviceRef.current = null;
      onDisconnected();
    },
    [appendLog, onDisconnected],
  );
  forceLinkLostRef.current = forceLinkLost;

  const handleEventValue = useCallback(
    (ev: Event) => {
      const target = ev.target as BluetoothRemoteGATTCharacteristic;
      const value = target.value;
      if (!value) return;
      const buf = new Uint8Array(value.buffer.slice(value.byteOffset, value.byteOffset + value.byteLength));
      appendLog(`D (ble) RX Event ${buf.length}b ${hexPreview(buf)}`);
      const pkt = decodePacket(buf);
      if (!pkt) {
        appendLog(`W (ble) bad event frame (${buf.length}b)`);
        return;
      }
      if (pkt.flags & FLAG_NAK) {
        appendLog(`W (ble) NAK code=0x${pkt.code.toString(16)} reason=${pkt.data[0] ?? '?'}`);
        if (pkt.code === APP_CODE_HEARTBEAT) {
          const pending = awaitingWebPollRef.current;
          if (pending && pending.msgId === pkt.messageId) {
            forceLinkLostRef.current('heartbeat NAK from device');
          }
        }
        return;
      }
      // ACK/NAK are replies only — never treat as twin-state payloads (empty data
      // unpacks to IDLE and used to bounce the web UI into a send↔status loop).
      if (pkt.flags & FLAG_ACK) {
        appendLog(`I (ble) ACK code=0x${pkt.code.toString(16)} msgId=${pkt.messageId}`);
        if (pkt.code === APP_CODE_HEARTBEAT) {
          const pending = awaitingWebPollRef.current;
          if (pending && pending.msgId === pkt.messageId) {
            noteHeartbeat('web-poll ACK');
          }
        }
        return;
      }

      // Device→web heartbeat poll (CMD on Event notify) — ACK and pulse UI.
      if ((pkt.flags & FLAG_CMD) && pkt.code === APP_CODE_HEARTBEAT) {
        noteHeartbeat('device poll');
        if (pkt.data.length >= 6) {
          const st = unpackTwinState(pkt.data);
          onRemoteRef.current?.(st);
        }
        const ack: Packet = {
          flags: FLAG_ACK,
          from: masterUuid.current,
          to: deviceUuidRef.current,
          code: APP_CODE_HEARTBEAT,
          retries: 0,
          messageId: pkt.messageId,
          fragmentIndex: 0,
          totalFragments: 1,
          data: new Uint8Array(0),
        };
        void writePacketRef.current(ack);
        return;
      }

      if (pkt.flags & (FLAG_EVT | FLAG_RPL)) {
        if (pkt.code === APP_CODE_BUTTON && (pkt.flags & FLAG_EVT) && pkt.data.length >= 1) {
          const side = pkt.data[0] === BUTTON_SIDE_LEFT ? 'left' : 'right';
          onButtonPressRef.current?.(side);
          appendLog(`I (ble) button press side=${side}`);
          return;
        }
        if (
          pkt.code === APP_CODE_HEARTBEAT &&
          (pkt.flags & FLAG_RPL) &&
          pkt.data.length >= 6
        ) {
          noteHeartbeat('web-poll RPL');
          const st = unpackTwinState(pkt.data);
          onRemoteRef.current?.(st);
          return;
        }
        if (
          pkt.data.length >= 6 &&
          (pkt.code === APP_CODE_TWIN_STATE || (pkt.flags & FLAG_RPL) !== 0)
        ) {
          const st = unpackTwinState(pkt.data);
          onRemoteRef.current?.(st);
          appendLog(`I (ble) twin state alert=${st.alert} led=${st.led} color=${st.color}`);
        }
      }
    },
    [appendLog, noteHeartbeat],
  );

  const handleStatusValue = useCallback(
    (ev: Event) => {
      const target = ev.target as BluetoothRemoteGATTCharacteristic;
      const value = target.value;
      if (!value || value.byteLength < 6) return;
      const buf = new Uint8Array(value.buffer.slice(value.byteOffset, value.byteOffset + value.byteLength));
      const st = unpackTwinState(buf);
      appendLog(`D (ble) RX Status alert=${st.alert} led=${st.led}`);
      // Status mirrors device state after applies; UI sync is driven by Event twin
      // notifies. Still forward so the page can adopt device-originated changes when
      // no Event twin payload arrived — page dedupes identical keys before TX.
      onRemoteRef.current?.(st);
    },
    [appendLog],
  );

  const handleLogValue = useCallback(
    (ev: Event) => {
      const target = ev.target as BluetoothRemoteGATTCharacteristic;
      const value = target.value;
      if (!value) return;
      const text = new TextDecoder().decode(value.buffer.slice(value.byteOffset, value.byteOffset + value.byteLength));
      appendLog(text);
    },
    [appendLog],
  );

  const handlePairingValue = useCallback(
    (ev: Event) => {
      const target = ev.target as BluetoothRemoteGATTCharacteristic;
      const value = target.value;
      if (!value || value.byteLength < 1) return;
      const op = value.getUint8(0);
      appendLog(`D (ble) RX Pairing op=0x${op.toString(16)} len=${value.byteLength}`);
      if (op === 0x02 && value.byteLength >= 17) {
        const uuid = new Uint8Array(16);
        for (let i = 0; i < 16; i++) uuid[i] = value.getUint8(1 + i);
        deviceUuidRef.current = uuid;
        setPaired(true);
        lastHeartbeatAtRef.current = Date.now();
        appendLog(`I (ble) paired deviceUuid=${hexPreview(uuid, 16)}`);
        if (!pairingAnalyticsSentRef.current) {
          pairingAnalyticsSentRef.current = true;
          ilssAnalytics.pairingSucceeded({
            simulated: false,
            name: deviceRef.current?.name ?? null,
          });
        }
      } else if (op === 0x03) {
        setPaired(false);
        appendLog('I (ble) unpaired');
      }
    },
    [appendLog],
  );

  /** Serialize GATT writes — Chrome often rejects concurrent ATT ops with opaque errors. */
  const enqueueGatt = useCallback(<T,>(label: string, fn: () => Promise<T>): Promise<T> => {
    const run = writeChainRef.current.then(fn, fn);
    writeChainRef.current = run.then(
      () => undefined,
      () => undefined,
    );
    return run.catch((err) => {
      logGattErr(label, err);
      throw err;
    });
  }, [logGattErr]);

  const writePacket = useCallback(
    async (pkt: Packet) => {
      const char = cmdCharRef.current;
      if (!char) {
        appendLog('W (ble) writePacket skipped — no Command characteristic');
        return false;
      }
      if (!serverRef.current?.connected) {
        appendLog('W (ble) writePacket skipped — GATT disconnected');
        return false;
      }
      const encoded = encodePacket(pkt);
      if (!encoded) {
        appendLog('E (ble) encodePacket failed');
        return false;
      }
      appendLog(
        `D (ble) TX Command flags=0x${pkt.flags.toString(16)} code=0x${pkt.code.toString(16)} msgId=${pkt.messageId} ${encoded.length}b ${hexPreview(encoded)}`,
      );
      try {
        await enqueueGatt('writePacket', () =>
          char.writeValueWithoutResponse(new Uint8Array(encoded)),
        );
        return true;
      } catch {
        return false;
      }
    },
    [appendLog, enqueueGatt],
  );
  writePacketRef.current = writePacket;

  const sendTwinState = useCallback(
    async (st: DeviceState) => {
      if (status !== 'connected' || !cmdCharRef.current) return false;
      const advanced = isAdvancedPatch(st);
      const data = packTwinState(st, advanced);
      appendLog(
        `I (ble) sendTwinState alert=${st.alert} color=${st.color} led=${st.led} bright=${st.brightness ?? 100} hap=${st.haptic} buzz=${st.buzzer} adv=${advanced ? 1 : 0}`,
      );
      const pkt: Packet = {
        flags: FLAG_CMD,
        from: masterUuid.current,
        to: deviceUuidRef.current,
        code: APP_CODE_TWIN_STATE,
        retries: 3,
        messageId: msgId.current++,
        fragmentIndex: 0,
        totalFragments: 1,
        data,
      };
      return await writePacket(pkt);
    },
    [status, writePacket, appendLog],
  );

  const pollHeartbeat = useCallback(async () => {
    if (status !== 'connected' || !cmdCharRef.current) return false;
    const id = msgId.current++;
    appendLog(`D (ble) heartbeat TX msgId=${id}`);
    const pkt: Packet = {
      flags: FLAG_CMD,
      from: masterUuid.current,
      to: deviceUuidRef.current,
      code: APP_CODE_HEARTBEAT,
      retries: 1,
      messageId: id,
      fragmentIndex: 0,
      totalFragments: 1,
      data: new Uint8Array(0),
    };
    awaitingWebPollRef.current = { msgId: id, since: Date.now() };
    const ok = await writePacket(pkt);
    if (!ok) {
      awaitingWebPollRef.current = null;
      forceLinkLostRef.current('heartbeat write failed');
      return false;
    }
    return true;
  }, [status, writePacket, appendLog]);

  // Watchdog: if device is silent, web polls; if poll unanswered → link-lost modal.
  useEffect(() => {
    if (status !== 'connected' || !paired) return;
    if (lastHeartbeatAtRef.current === 0) {
      lastHeartbeatAtRef.current = Date.now();
    }
    const SILENCE_MS = 12_000;
    const ACK_TIMEOUT_MS = 4_000;
    const t = window.setInterval(() => {
      if (statusRef.current !== 'connected') return;
      const now = Date.now();
      const pending = awaitingWebPollRef.current;
      if (pending && now - pending.since >= ACK_TIMEOUT_MS) {
        forceLinkLostRef.current('no heartbeat reply within 4s');
        return;
      }
      if (!pending && now - lastHeartbeatAtRef.current >= SILENCE_MS) {
        void pollHeartbeat();
      }
    }, 1000);
    return () => window.clearInterval(t);
  }, [status, paired, pollHeartbeat]);

  const doPairingExchange = useCallback(async () => {
    const char = pairingCharRef.current;
    if (!char) {
      appendLog('W (ble) pairing write skipped — no Pairing characteristic');
      return;
    }
    const nonce = new Uint8Array(16);
    crypto.getRandomValues(nonce);
    const payload = new Uint8Array(1 + 16 + 16);
    payload[0] = 0x01;
    payload.set(masterUuid.current, 1);
    payload.set(nonce, 17);
    appendLog(`I (ble) TX pair_req ${payload.length}b master=${hexPreview(masterUuid.current, 8)}…`);
    await enqueueGatt('pair_req', () => char.writeValue(new Uint8Array(payload)));
  }, [appendLog, enqueueGatt]);

  const unpair = useCallback(async () => {
    const char = pairingCharRef.current;
    if (!char) return;
    appendLog('I (ble) TX unpair');
    try {
      await enqueueGatt('unpair', () => char.writeValue(Uint8Array.of(0x03)));
      setPaired(false);
    } catch {
      /* logged in enqueueGatt */
    }
  }, [appendLog, enqueueGatt]);

  const pair = useCallback(async () => {
    setMsg('');
    setLinkLost(false);
    if (!navigator.bluetooth) {
      setMsg('Web Bluetooth isn’t available in this browser.');
      appendLog('E (ble) navigator.bluetooth unavailable');
      ilssAnalytics.pairingIssue({
        reason: 'unsupported',
        message: 'navigator.bluetooth unavailable',
      });
      return;
    }
    connectingRef.current = true;
    pairingAnalyticsSentRef.current = false;
    try {
      setStatus('connecting');
      appendLog('I (ble) requestDevice (Twin service + ILSS-LY name prefix)');
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [UUID_TWIN_SVC] }, { namePrefix: 'ILSS-LY' }],
        optionalServices: OPTIONAL_SERVICES,
      });
      deviceRef.current = device;
      appendLog(`I (ble) selected device name=${device.name || '(none)'} id=${device.id}`);
      device.addEventListener('gattserverdisconnected', onDisconnected);

      appendLog('I (ble) gatt.connect()');
      const server = await device.gatt!.connect();
      serverRef.current = server;
      appendLog('I (ble) GATT connected');

      // Brief settle — Chrome often fails subsequent ATT ops if hammered immediately.
      await new Promise((r) => setTimeout(r, 50));

      appendLog(`I (ble) getPrimaryService meta=${UUID_META_SVC}`);
      const meta = await server.getPrimaryService(UUID_META_SVC);
      appendLog(`I (ble) getPrimaryService twin=${UUID_TWIN_SVC}`);
      const twin = await server.getPrimaryService(UUID_TWIN_SVC);

      appendLog('I (ble) reading metadata characteristics…');
      const [serial, model, software, brand, battery] = await Promise.all([
        readString(await meta.getCharacteristic(UUID_SERIAL)),
        readString(await meta.getCharacteristic(UUID_MODEL)),
        readString(await meta.getCharacteristic(UUID_SWVER)),
        readU8(await meta.getCharacteristic(UUID_BRAND)),
        readU8(await meta.getCharacteristic(UUID_BATT)),
      ]);
      setMetadata({ serial, model, software, brand, battery });
      appendLog(
        `I (ble) metadata serial=${serial} model=${model} sw=${software} brand=${brand} batt=${battery}`,
      );

      appendLog('I (ble) getCharacteristic Command');
      const cmd = await twin.getCharacteristic(UUID_CMD);
      cmdCharRef.current = cmd;

      appendLog('I (ble) startNotifications Event');
      const eventChar = await twin.getCharacteristic(UUID_EVENT);
      await eventChar.startNotifications();
      eventChar.addEventListener('characteristicvaluechanged', handleEventValue);

      appendLog('I (ble) startNotifications Status');
      const statusChar = await twin.getCharacteristic(UUID_STATUS);
      await statusChar.startNotifications();
      statusChar.addEventListener('characteristicvaluechanged', handleStatusValue);
      try {
        const sv = await statusChar.readValue();
        appendLog(`D (ble) Status read ${sv.byteLength}b`);
        if (sv.byteLength >= 6) {
          const buf = new Uint8Array(sv.buffer.slice(sv.byteOffset, sv.byteOffset + sv.byteLength));
          const st = unpackTwinState(buf);
          appendLog(`I (ble) initial status alert=${st.alert} led=${st.led}`);
          onRemoteRef.current?.(st);
        }
      } catch (e) {
        appendLog(`W (ble) Status read skipped [${errDetail(e)}]`);
      }

      appendLog('I (ble) startNotifications Log');
      const logChar = await twin.getCharacteristic(UUID_LOG);
      await logChar.startNotifications();
      logChar.addEventListener('characteristicvaluechanged', handleLogValue);

      appendLog('I (ble) startNotifications Pairing');
      const pairing = await twin.getCharacteristic(UUID_PAIRING);
      pairingCharRef.current = pairing;
      await pairing.startNotifications();
      pairing.addEventListener('characteristicvaluechanged', handlePairingValue);

      await doPairingExchange();

      connectingRef.current = false;
      setName(device.name || serial || 'ILSS Lanyard');
      setStatus('connected');
      setMsg('');
      appendLog(`I (ble) connected to ${device.name || serial}`);
    } catch (err) {
      connectingRef.current = false;
      setStatus('disconnected');
      const error = err as Error & { name?: string };
      if (error?.name === 'NotFoundError') {
        appendLog('W (ble) requestDevice cancelled / no device');
        setMsg('No device selected.');
        if (!pairingAnalyticsSentRef.current) {
          pairingAnalyticsSentRef.current = true;
          ilssAnalytics.pairingIssue({
            reason: 'cancelled',
            message: 'No device selected',
          });
        }
      } else if (error?.name === 'SecurityError') {
        logGattErr('pair', err);
        setMsg('Bluetooth is blocked by the browser’s permissions here.');
        if (!pairingAnalyticsSentRef.current) {
          pairingAnalyticsSentRef.current = true;
          ilssAnalytics.pairingIssue({
            reason: 'error',
            message: errDetail(err),
          });
        }
      } else {
        logGattErr('pair', err);
        if (!pairingAnalyticsSentRef.current) {
          pairingAnalyticsSentRef.current = true;
          ilssAnalytics.pairingIssue({
            reason: 'error',
            message: errDetail(err),
          });
        }
      }
      try {
        if (deviceRef.current?.gatt?.connected) deviceRef.current.gatt.disconnect();
      } catch {
        /* ignore */
      }
      cmdCharRef.current = null;
      pairingCharRef.current = null;
      serverRef.current = null;
    }
  }, [
    appendLog,
    doPairingExchange,
    handleEventValue,
    handleLogValue,
    handlePairingValue,
    handleStatusValue,
    logGattErr,
    onDisconnected,
  ]);

  const simulate = useCallback(() => {
    setMsg('');
    setStatus('connecting');
    appendLog('I (sim) connecting…');
    setTimeout(() => {
      setName('ILSS-LY · SIM');
      setMetadata({
        serial: 'ILSS-LY-SIM',
        model: 'ILSS-LANYARD-HW1_0-DISABILITYIN',
        software: '0.1.0-sim',
        brand: 1,
        battery: 87,
      });
      setStatus('connected');
      setPaired(true);
      lastHeartbeatAtRef.current = Date.now();
      appendLog('I (sim) simulated device connected');
      appendLog('I (DigitalTwinApp) Digital twin running');
      ilssAnalytics.pairingSucceeded({ simulated: true, name: 'ILSS-LY · SIM' });
    }, 750);
  }, [appendLog]);

  const disconnect = useCallback(() => {
    const d = deviceRef.current;
    appendLog('I (ble) disconnect requested');
    setLinkLost(false);
    try {
      if (d?.gatt?.connected) d.gatt.disconnect();
    } catch (e) {
      appendLog(`W (ble) disconnect [${errDetail(e)}]`);
    }
    deviceRef.current = null;
    onDisconnected();
    setMsg('');
  }, [onDisconnected, appendLog]);

  const value = useMemo<BleTwinContextValue>(
    () => ({
      status,
      supported,
      name,
      msg,
      metadata,
      logs,
      paired,
      clearLogs,
      logBle,
      pair,
      simulate,
      disconnect,
      unpair,
      sendTwinState,
      pollHeartbeat,
      setOnRemoteState,
      setOnButtonPress,
      setOnHeartbeat,
      linkLost,
      clearLinkLost,
    }),
    [
      status,
      supported,
      name,
      msg,
      metadata,
      logs,
      paired,
      clearLogs,
      logBle,
      pair,
      simulate,
      disconnect,
      unpair,
      sendTwinState,
      pollHeartbeat,
      setOnRemoteState,
      setOnButtonPress,
      setOnHeartbeat,
      linkLost,
      clearLinkLost,
    ],
  );

  return <BleTwinContext.Provider value={value}>{children}</BleTwinContext.Provider>;
}

export function useBleTwin() {
  const ctx = useContext(BleTwinContext);
  if (!ctx) throw new Error('useBleTwin must be used within BleTwinProvider');
  return ctx;
}
