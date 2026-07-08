interface BluetoothRemoteGATTServer {
  connected: boolean;
  connect(): Promise<BluetoothRemoteGATTServer>;
  disconnect(): void;
}

interface BluetoothDevice extends EventTarget {
  readonly id: string;
  readonly name?: string;
  readonly gatt?: BluetoothRemoteGATTServer;
  addEventListener(
    type: 'gattserverdisconnected',
    listener: () => void,
    options?: boolean | AddEventListenerOptions,
  ): void;
}

interface Bluetooth extends EventTarget {
  requestDevice(options?: {
    acceptAllDevices?: boolean;
    optionalServices?: string[];
  }): Promise<BluetoothDevice>;
}

interface Navigator {
  bluetooth?: Bluetooth;
}
