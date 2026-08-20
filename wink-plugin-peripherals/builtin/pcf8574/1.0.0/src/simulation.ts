import {
  normalizeManifest,
  resolvePluginIdentity,
  BaseSimulationPlugin,
  LogicStates,
  type ManifestFactory,
  type VirtualPinProvider,
  type LogicState,
  type PeripheralManifest,
  type I2CDevice,
  type I2CTransferResult,
} from '@wink-ai/unisim';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(
  import.meta.url,
  typeof __PLUGIN_TYPE__ !== 'undefined' ? __PLUGIN_TYPE__ : 'pcf8574',
  typeof __PLUGIN_VERSION__ !== 'undefined' ? __PLUGIN_VERSION__ : '1.0.0',
  typeof __PLUGIN_CATEGORY__ !== 'undefined' ? __PLUGIN_CATEGORY__ : 'infrastructure',
);

export function createPcf8574Manifest(): PeripheralManifest {
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: 'PCF8574 I2C IO Expander',
    description: '8-bit I2C I/O expander',
    timingModel: 'event-driven',
    pins: [
      { name: 'VCC', pinType: 'vcc', role: 'power', required: false },
      { name: 'GND', pinType: 'gnd', role: 'ground', required: false },
      { name: 'SDA', pinType: 'i2c_sda', role: 'signal', required: true },
      { name: 'SCL', pinType: 'i2c_scl', role: 'signal', required: true },
      { name: 'INT', pinType: 'digital_out', role: 'signal', required: false },
      { name: 'P0', pinType: 'gpio', role: 'gpio', required: false },
      { name: 'P1', pinType: 'gpio', role: 'gpio', required: false },
      { name: 'P2', pinType: 'gpio', role: 'gpio', required: false },
      { name: 'P3', pinType: 'gpio', role: 'gpio', required: false },
      { name: 'P4', pinType: 'gpio', role: 'gpio', required: false },
      { name: 'P5', pinType: 'gpio', role: 'gpio', required: false },
      { name: 'P6', pinType: 'gpio', role: 'gpio', required: false },
      { name: 'P7', pinType: 'gpio', role: 'gpio', required: false },
    ],
    properties: {
      address: { type: 'number', default: 32 }, // 0x20
      variant: { type: 'string', default: 'pcf8574_i2c' },
    },
    stateChannels: {
      latch: { type: 'number', default: 255 }, // P0-P7 output latch
    },
    events: {},
  });
}

export const pcf8574Manifest: PeripheralManifest = createPcf8574Manifest();
export const pcf8574ManifestFactory: ManifestFactory = () => createPcf8574Manifest();

export class Pcf8574Plugin extends BaseSimulationPlugin implements I2CDevice, VirtualPinProvider {
  override get type(): string {
    return identity.type;
  }
  readonly manifest = pcf8574Manifest;
  static readonly manifest = pcf8574Manifest;

  private _i2cAddress = 0x20;
  private _outputLatch = 0xff; // default all HIGH (quasi-bidirectional inputs)
  private _pPins: Array<string | undefined> = [];

  get providerId(): string {
    return `io_expander:${this.ctx?.instanceId ?? 'pcf8574'}`;
  }

  readVirtualPin(pinIndex: number): LogicState {
    const inputs = this.readInputs();
    return (inputs & (1 << pinIndex)) !== 0 ? LogicStates.HIGH : LogicStates.LOW;
  }

  writeVirtualPin(pinIndex: number, level: LogicState): void {
    if (level === LogicStates.HIGH) {
      this._outputLatch |= 1 << pinIndex;
    } else {
      this._outputLatch &= ~(1 << pinIndex);
    }
    this.updateOutputs();
    this.ctx?.publish('latch', this._outputLatch);
  }

  private _unregisterI2c?: () => void;

  protected override onBound(ctx: any, pinMapping: Record<string, number>, props: any): void {
    this._i2cAddress = Number(props.address ?? 0x20);

    this._pPins = [
      pinMapping['P0'] !== undefined ? 'P0' : undefined,
      pinMapping['P1'] !== undefined ? 'P1' : undefined,
      pinMapping['P2'] !== undefined ? 'P2' : undefined,
      pinMapping['P3'] !== undefined ? 'P3' : undefined,
      pinMapping['P4'] !== undefined ? 'P4' : undefined,
      pinMapping['P5'] !== undefined ? 'P5' : undefined,
      pinMapping['P6'] !== undefined ? 'P6' : undefined,
      pinMapping['P7'] !== undefined ? 'P7' : undefined,
    ];

    const busI2c = this.ctx?.bus?.i2c;
    if (busI2c && typeof busI2c.registerDevice === 'function') {
      this._unregisterI2c = busI2c.registerDevice({
        address: this._i2cAddress,
        onTransfer: (writeBytes: Uint8Array, readLen: number) => {
          const res = this.onTransfer(writeBytes, readLen);
          return res.readBytes ?? new Uint8Array(0);
        },
      });
    } else if (this.ctx && typeof this.ctx.registerI2cDevice === 'function') {
      this.ctx.registerI2cDevice(this);
    }

    this.updateOutputs();
  }

  override onDestroy(): void {
    if (this._unregisterI2c) {
      this._unregisterI2c();
      this._unregisterI2c = undefined;
    } else if (this.ctx?.bus?.i2c && typeof this.ctx.bus.i2c.unregisterDevice === 'function') {
      this.ctx.bus.i2c.unregisterDevice(this._i2cAddress);
    } else if (this.ctx && typeof this.ctx.unregisterI2cDevice === 'function') {
      this.ctx.unregisterI2cDevice(this._i2cAddress);
    }
  }

  // I2CDevice implementation
  get addr(): number {
    return this._i2cAddress;
  }

  onTransfer(writeBytes: Uint8Array, readLen: number): I2CTransferResult {
    if (writeBytes.length > 0) {
      this._outputLatch = writeBytes[0];
      this.updateOutputs();
      this.ctx?.publish('latch', this._outputLatch);
    }
    let readBytes: Uint8Array | undefined;
    if (readLen > 0) {
      readBytes = new Uint8Array([this.readInputs()]);
    }
    return { ack: true, readBytes };
  }

  private readInputs(): number {
    let result = 0;
    for (let i = 0; i < 8; i++) {
      const pinName = this._pPins[i];
      if (pinName) {
        const state = this.ctx?.gpio
          ? this.ctx.gpio.read(pinName)
          : typeof this.ctx?.readPin === 'function'
            ? this.ctx.readPin(pinName)
            : LogicStates.HIGH;
        if (state === LogicStates.HIGH || state === true) {
          result |= 1 << i;
        }
      } else {
        // If pin not mapped, default to HIGH due to quasi-bidirectional pull-up
        result |= 1 << i;
      }
    }
    return result;
  }

  private updateOutputs(): void {
    for (let i = 0; i < 8; i++) {
      const pinName = this._pPins[i];
      if (pinName) {
        const isHigh = (this._outputLatch & (1 << i)) !== 0;
        const level = isHigh ? LogicStates.HIGH : LogicStates.LOW;
        if (this.ctx?.gpio) {
          this.ctx.gpio.write(pinName, level);
        } else if (typeof this.ctx?.writePin === 'function') {
          this.ctx.writePin(pinName, isHigh);
        }
      }
    }
  }
}

export default {
  manifest: pcf8574Manifest,
  manifestFactory: pcf8574ManifestFactory,
  PluginClass: Pcf8574Plugin,
};
