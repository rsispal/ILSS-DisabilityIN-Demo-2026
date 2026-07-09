import {
  createContext,
  useCallback,
  useContext,
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
  APP_CODE_HEARTBEAT,
  APP_CODE_TWIN_STATE,
  packTwinState,
  unpackTwinState,
  isAdvancedPatch,
} from '@/lib/ble/twinState';

export type BleTwinStatus = 'disconnected' | 'connecting' | 'connected';

export interface DeviceMetadata {
  serial: string;
  model: string;
  software: string;
  brand: number;
  battery: number;
}

export interface DeviceLogLine {
  id: number;
  ts: number;
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
  pair: () => Promise<void>;
  simulate: () => void;
  disconnect: () => void;
  unpair: () => Promise<void>;
  sendTwinState: (st: DeviceState) => Promise<boolean>;
  pollHeartbeat: () => Promise<DeviceState | null>;
  /** Called by page to register inbound twin state handler */
  setOnRemoteState: (fn: ((st: DeviceState) => void) | null) => void;
}

const BleTwinContext = createContext<BleTwinContextValue | null>(null);

async function readString(char: BluetoothRemoteGATTCharacteristic): Promise<string> {
  const v = await char.readValue();
  return new TextDecoder().decode(v.buffer);
}

async function readU8(char: BluetoothRemoteGATTCharacteristic): Promise<number> {
  const v = await char.readValue();
  return v.getUint8(0);
}

export function BleTwinProvider({ children }: { children: ReactNode }) {
  const [status, setStatus] = useState<BleTwinStatus>('disconnected');
  const [name, setName] = useState<string | null>(null);
  const [msg, setMsg] = useState('');
  const [metadata, setMetadata] = useState<DeviceMetadata | null>(null);
  const [logs, setLogs] = useState<DeviceLogLine[]>([]);
  const [paired, setPaired] = useState(false);

  const deviceRef = useRef<BluetoothDevice | null>(null);
  const serverRef = useRef<BluetoothRemoteGATTServer | null>(null);
  const cmdCharRef = useRef<BluetoothRemoteGATTCharacteristic | null>(null);
  const pairingCharRef = useRef<BluetoothRemoteGATTCharacteristic | null>(null);
  const masterUuid = useRef(randomUuidBytes());
  const msgId = useRef(1);
  const logId = useRef(1);
  const onRemoteRef = useRef<((st: DeviceState) => void) | null>(null);
  const deviceUuidRef = useRef(nullUuid());

  const supported = typeof navigator !== 'undefined' && !!navigator.bluetooth;

  const appendLog = useCallback((text: string) => {
    const id = logId.current++;
    setLogs((prev) => {
      const next = [...prev, { id, ts: Date.now(), text }];
      return next.length > 500 ? next.slice(-500) : next;
    });
  }, []);

  const clearLogs = useCallback(() => setLogs([]), []);

  const setOnRemoteState = useCallback((fn: ((st: DeviceState) => void) | null) => {
    onRemoteRef.current = fn;
  }, []);

  const onDisconnected = useCallback(() => {
    setStatus('disconnected');
    setName(null);
    setPaired(false);
    setMsg('Device disconnected');
    cmdCharRef.current = null;
    pairingCharRef.current = null;
    serverRef.current = null;
  }, []);

  const handleEventValue = useCallback(
    (ev: Event) => {
      const target = ev.target as BluetoothRemoteGATTCharacteristic;
      const value = target.value;
      if (!value) return;
      const buf = new Uint8Array(value.buffer.slice(value.byteOffset, value.byteOffset + value.byteLength));
      const pkt = decodePacket(buf);
      if (!pkt) {
        appendLog(`W (ble) bad event frame (${buf.length}b)`);
        return;
      }
      if (pkt.flags & FLAG_NAK) {
        appendLog(`W (ble) NAK code=0x${pkt.code.toString(16)} reason=${pkt.data[0] ?? '?'}`);
        return;
      }
      if (pkt.flags & (FLAG_EVT | FLAG_RPL | FLAG_ACK)) {
        if (pkt.code === APP_CODE_TWIN_STATE || (pkt.flags & FLAG_RPL && pkt.data.length >= 6)) {
          const st = unpackTwinState(pkt.data);
          onRemoteRef.current?.(st);
          appendLog(`I (ble) twin state alert=${st.alert} led=${st.led}`);
        }
      }
    },
    [appendLog],
  );

  const handleStatusValue = useCallback(
    (ev: Event) => {
      const target = ev.target as BluetoothRemoteGATTCharacteristic;
      const value = target.value;
      if (!value || value.byteLength < 6) return;
      const buf = new Uint8Array(value.buffer.slice(value.byteOffset, value.byteOffset + value.byteLength));
      const st = unpackTwinState(buf);
      onRemoteRef.current?.(st);
    },
    [],
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
      if (op === 0x02 && value.byteLength >= 17) {
        const uuid = new Uint8Array(16);
        for (let i = 0; i < 16; i++) uuid[i] = value.getUint8(1 + i);
        deviceUuidRef.current = uuid;
        setPaired(true);
        appendLog('I (ble) paired');
      } else if (op === 0x03) {
        setPaired(false);
        appendLog('I (ble) unpaired');
      }
    },
    [appendLog],
  );

  const writePacket = useCallback(async (pkt: Packet) => {
    const char = cmdCharRef.current;
    if (!char) return false;
    const encoded = encodePacket(pkt);
    if (!encoded) return false;
    await char.writeValueWithoutResponse(new Uint8Array(encoded));
    return true;
  }, []);

  const sendTwinState = useCallback(
    async (st: DeviceState) => {
      if (status !== 'connected' || !cmdCharRef.current) return false;
      const advanced = isAdvancedPatch(st);
      const data = packTwinState(st, advanced);
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
      try {
        return await writePacket(pkt);
      } catch (e) {
        setMsg((e as Error).message || 'Write failed');
        return false;
      }
    },
    [status, writePacket],
  );

  const pollHeartbeat = useCallback(async () => {
    if (status !== 'connected' || !cmdCharRef.current) return null;
    const pkt: Packet = {
      flags: FLAG_CMD,
      from: masterUuid.current,
      to: deviceUuidRef.current,
      code: APP_CODE_HEARTBEAT,
      retries: 1,
      messageId: msgId.current++,
      fragmentIndex: 0,
      totalFragments: 1,
      data: new Uint8Array(0),
    };
    await writePacket(pkt);
    return null;
  }, [status, writePacket]);

  const doPairingExchange = useCallback(async () => {
    const char = pairingCharRef.current;
    if (!char) return;
    const nonce = new Uint8Array(16);
    crypto.getRandomValues(nonce);
    const payload = new Uint8Array(1 + 16 + 16);
    payload[0] = 0x01;
    payload.set(masterUuid.current, 1);
    payload.set(nonce, 17);
    await char.writeValue(new Uint8Array(payload));
  }, []);

  const unpair = useCallback(async () => {
    const char = pairingCharRef.current;
    if (!char) return;
    await char.writeValue(Uint8Array.of(0x03).buffer);
    setPaired(false);
  }, []);

  const pair = useCallback(async () => {
    setMsg('');
    if (!navigator.bluetooth) {
      setMsg('Web Bluetooth isn’t available in this browser.');
      return;
    }
    try {
      setStatus('connecting');
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ services: [UUID_TWIN_SVC] }, { namePrefix: 'ILSS-LY' }],
        optionalServices: OPTIONAL_SERVICES,
      });
      deviceRef.current = device;
      device.addEventListener('gattserverdisconnected', onDisconnected);
      const server = await device.gatt!.connect();
      serverRef.current = server;

      const meta = await server.getPrimaryService(UUID_META_SVC);
      const twin = await server.getPrimaryService(UUID_TWIN_SVC);

      const [serial, model, software, brand, battery] = await Promise.all([
        readString(await meta.getCharacteristic(UUID_SERIAL)),
        readString(await meta.getCharacteristic(UUID_MODEL)),
        readString(await meta.getCharacteristic(UUID_SWVER)),
        readU8(await meta.getCharacteristic(UUID_BRAND)),
        readU8(await meta.getCharacteristic(UUID_BATT)),
      ]);
      setMetadata({ serial, model, software, brand, battery });

      const cmd = await twin.getCharacteristic(UUID_CMD);
      cmdCharRef.current = cmd;

      const eventChar = await twin.getCharacteristic(UUID_EVENT);
      await eventChar.startNotifications();
      eventChar.addEventListener('characteristicvaluechanged', handleEventValue);

      const statusChar = await twin.getCharacteristic(UUID_STATUS);
      await statusChar.startNotifications();
      statusChar.addEventListener('characteristicvaluechanged', handleStatusValue);
      try {
        const sv = await statusChar.readValue();
        if (sv.byteLength >= 6) {
          const buf = new Uint8Array(sv.buffer.slice(sv.byteOffset, sv.byteOffset + sv.byteLength));
          onRemoteRef.current?.(unpackTwinState(buf));
        }
      } catch {
        /* ignore */
      }

      const logChar = await twin.getCharacteristic(UUID_LOG);
      await logChar.startNotifications();
      logChar.addEventListener('characteristicvaluechanged', handleLogValue);

      const pairing = await twin.getCharacteristic(UUID_PAIRING);
      pairingCharRef.current = pairing;
      await pairing.startNotifications();
      pairing.addEventListener('characteristicvaluechanged', handlePairingValue);
      await doPairingExchange();

      setName(device.name || serial || 'ILSS Lanyard');
      setStatus('connected');
      setMsg('');
      appendLog(`I (ble) connected to ${device.name || serial}`);
    } catch (err) {
      setStatus('disconnected');
      const error = err as Error & { name?: string };
      if (error?.name === 'NotFoundError') setMsg('No device selected.');
      else if (error?.name === 'SecurityError')
        setMsg('Bluetooth is blocked by the browser’s permissions here.');
      else setMsg(error?.message || 'Pairing failed.');
    }
  }, [
    appendLog,
    doPairingExchange,
    handleEventValue,
    handleLogValue,
    handlePairingValue,
    handleStatusValue,
    onDisconnected,
  ]);

  const simulate = useCallback(() => {
    setMsg('');
    setStatus('connecting');
    setTimeout(() => {
      setName('ILSS-LY · SIM');
      setMetadata({
        serial: 'ILSS-LY-SIM',
        model: 'ILSS-Lanyard-Breakout',
        software: '0.1.0-sim',
        brand: 1,
        battery: 87,
      });
      setStatus('connected');
      setPaired(true);
      appendLog('I (sim) simulated device connected');
      appendLog('I (DigitalTwinApp) Digital twin running');
    }, 750);
  }, [appendLog]);

  const disconnect = useCallback(() => {
    const d = deviceRef.current;
    try {
      if (d?.gatt?.connected) d.gatt.disconnect();
    } catch {
      /* ignore */
    }
    deviceRef.current = null;
    onDisconnected();
    setMsg('');
  }, [onDisconnected]);

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
      pair,
      simulate,
      disconnect,
      unpair,
      sendTwinState,
      pollHeartbeat,
      setOnRemoteState,
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
      pair,
      simulate,
      disconnect,
      unpair,
      sendTwinState,
      pollHeartbeat,
      setOnRemoteState,
    ],
  );

  return <BleTwinContext.Provider value={value}>{children}</BleTwinContext.Provider>;
}

export function useBleTwin() {
  const ctx = useContext(BleTwinContext);
  if (!ctx) throw new Error('useBleTwin must be used within BleTwinProvider');
  return ctx;
}
