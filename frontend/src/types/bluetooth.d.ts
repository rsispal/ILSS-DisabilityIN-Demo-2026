/** Minimal Web Bluetooth typings for the ILSS twin client. */

interface BluetoothRequestDeviceFilter {
  services?: BluetoothServiceUUID[];
  name?: string;
  namePrefix?: string;
}

interface RequestDeviceOptions {
  filters?: BluetoothRequestDeviceFilter[];
  optionalServices?: BluetoothServiceUUID[];
  acceptAllDevices?: boolean;
}

type BluetoothServiceUUID = number | string;

interface Bluetooth {
  requestDevice(options?: RequestDeviceOptions): Promise<BluetoothDevice>;
  getAvailability?(): Promise<boolean>;
}

interface BluetoothDevice extends EventTarget {
  readonly id: string;
  readonly name?: string | null;
  readonly gatt?: BluetoothRemoteGATTServer;
  addEventListener(
    type: 'gattserverdisconnected',
    listener: (ev: Event) => void,
    options?: boolean | AddEventListenerOptions,
  ): void;
}

interface BluetoothRemoteGATTServer {
  readonly device: BluetoothDevice;
  readonly connected: boolean;
  connect(): Promise<BluetoothRemoteGATTServer>;
  disconnect(): void;
  getPrimaryService(service: BluetoothServiceUUID): Promise<BluetoothRemoteGATTService>;
  getPrimaryServices(service?: BluetoothServiceUUID): Promise<BluetoothRemoteGATTService[]>;
}

interface BluetoothRemoteGATTService {
  readonly device: BluetoothDevice;
  readonly uuid: string;
  getCharacteristic(characteristic: BluetoothServiceUUID): Promise<BluetoothRemoteGATTCharacteristic>;
  getCharacteristics(characteristic?: BluetoothServiceUUID): Promise<BluetoothRemoteGATTCharacteristic[]>;
}

interface BluetoothRemoteGATTCharacteristic extends EventTarget {
  readonly service: BluetoothRemoteGATTService;
  readonly uuid: string;
  readonly value?: DataView | null;
  readValue(): Promise<DataView>;
  writeValue(value: BufferSource | ArrayBuffer | Uint8Array): Promise<void>;
  writeValueWithoutResponse(value: BufferSource | ArrayBuffer | Uint8Array): Promise<void>;
  startNotifications(): Promise<BluetoothRemoteGATTCharacteristic>;
  stopNotifications(): Promise<BluetoothRemoteGATTCharacteristic>;
  addEventListener(
    type: 'characteristicvaluechanged',
    listener: (ev: Event) => void,
    options?: boolean | AddEventListenerOptions,
  ): void;
}

interface Navigator {
  bluetooth?: Bluetooth;
}
