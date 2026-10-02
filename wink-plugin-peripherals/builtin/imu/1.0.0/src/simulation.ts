import {
  normalizeManifest,
  resolvePluginIdentity,
  BaseSimulationPlugin,
  type I2CDevice,
  type I2CTransferResult,
  type IPluginContext,
  type ManifestFactory,
  type PeripheralManifest,
  type LogicState,
} from '@wink-ai/unisim-sdk';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(
  import.meta.url,
  typeof __PLUGIN_TYPE__ !== 'undefined' ? __PLUGIN_TYPE__ : 'imu',
  typeof __PLUGIN_VERSION__ !== 'undefined' ? __PLUGIN_VERSION__ : '1.0.0',
  typeof __PLUGIN_CATEGORY__ !== 'undefined' ? __PLUGIN_CATEGORY__ : 'sensor',
);

// ── Default Constants ────────────────────────────────────────────────────────
const DEFAULT_I2C_ADDR = 0x68;
const REG_MAP_SIZE = 128;

// Register map addresses
const REG_CONFIG = 0x1a;
const REG_GYRO_CONFIG = 0x1b;
const REG_ACCEL_CONFIG = 0x1c;
const REG_INT_PIN_CFG = 0x37;
const REG_INT_ENABLE = 0x38;
const REG_INT_STATUS = 0x3a;
const REG_ACCEL_XOUT_H = 0x3b;
const REG_TEMP_OUT_H = 0x41;
const REG_GYRO_XOUT_H = 0x43;
const REG_PWR_MGMT_1 = 0x6b;
const REG_PWR_MGMT_2 = 0x6c;
const REG_WHO_AM_I = 0x75;

// AK8963 Magnetometer constants (for MPU9250 bypass mode)
const AK8963_ADDR = 0x0c;
const AK8963_WIA = 0x00;
const AK8963_WIA_VAL = 0x48;

export interface ImuState {
  whoAmI: number;
  pwrMgmt1: number;
  regPointer: number;
  accelX: number;
  accelY: number;
  accelZ: number;
  gyroX: number;
  gyroY: number;
  gyroZ: number;
  tempDegc: number;
  writeCount: number;
  readCount: number;
}

export interface ImuProps {
  variant?: string;
  address?: number;
  roll?: number;
  pitch?: number;
  yaw?: number;
  temperature?: number;
}

export function createImuManifest(): PeripheralManifest {
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: 'Inertial Measurement Unit (6-Axis/9-Axis)',
    description: 'High-fidelity 6-Axis/9-Axis IMU sensor supporting MPU-6050/6500/9250 with 3D kinematics',
    timingModel: 'event-driven',
    pins: [
      { name: 'VCC', pinType: 'vcc', role: 'power', required: false },
      { name: 'GND', pinType: 'gnd', role: 'ground', required: false },
      { name: 'SCL', pinType: 'i2c_scl', role: 'signal', required: true },
      { name: 'SDA', pinType: 'i2c_sda', role: 'signal', required: true },
      { name: 'AD0', pinType: 'digital_in', role: 'signal', required: false },
      { name: 'INT', pinType: 'digital_out', role: 'signal', required: false },
      { name: 'XCL', pinType: 'i2c_scl', role: 'signal', required: false },
      { name: 'XDA', pinType: 'i2c_sda', role: 'signal', required: false },
    ],
    properties: {
      variant: { type: 'string', default: 'mpu6050_i2c' },
      address: { type: 'number', default: DEFAULT_I2C_ADDR },
      roll: { type: 'number', default: 0 },
      pitch: { type: 'number', default: 0 },
      yaw: { type: 'number', default: 0 },
      temperature: { type: 'number', default: 25.0 },
    },
    stateChannels: {
      whoAmI: { type: 'number', default: 0x68 },
      pwrMgmt1: { type: 'number', default: 0x01 },
      regPointer: { type: 'number', default: 0 },
      accelX: { type: 'number', default: 0 },
      accelY: { type: 'number', default: 0 },
      accelZ: { type: 'number', default: 16384 },
      gyroX: { type: 'number', default: 0 },
      gyroY: { type: 'number', default: 0 },
      gyroZ: { type: 'number', default: 0 },
      tempDegc: { type: 'number', default: 25.0 },
      writeCount: { type: 'number', default: 0 },
      readCount: { type: 'number', default: 0 },
    },
    events: {},
  });
}

export const imuManifest: PeripheralManifest = createImuManifest();
export const imuManifestFactory: ManifestFactory = () => createImuManifest();

export class ImuPlugin extends BaseSimulationPlugin<ImuState> implements I2CDevice {
  override get type(): string {
    return identity.type;
  }
  readonly manifest = imuManifest;
  static readonly manifest = imuManifest;

  private _variant = 'mpu6050_i2c';
  private _address = DEFAULT_I2C_ADDR;
  private _registers = new Uint8Array(REG_MAP_SIZE);
  private _ak8963Regs = new Uint8Array(16);
  private _regPointer = 0;
  private _isAddrPhase = true;
  private _writeCount = 0;
  private _readCount = 0;
  private _ad0Pin = -1;
  private _intPin = -1;

  // Kinematics orientation
  private _roll = 0;
  private _pitch = 0;
  private _yaw = 0;
  private _temperature = 25.0;

  private _unregisterI2c?: () => void;
  private _unregisterAk8963?: () => void;

  get addr(): number {
    return this._address;
  }

  // ── Sensor Kinematics & Big-Endian Packing ─────────────────────────────────

  private _packBigEndian(offset: number, val: number): void {
    const u16 = val < 0 ? (val + 0x10000) & 0xffff : val & 0xffff;
    this._registers[offset] = (u16 >> 8) & 0xff;
    this._registers[offset + 1] = u16 & 0xff;
  }

  private _computeKinematics(): void {
    const radRoll = (this._roll * Math.PI) / 180.0;
    const radPitch = (this._pitch * Math.PI) / 180.0;

    // 1g = 16384 LSB (in ±2g scale)
    const ax = Math.round(-16384 * Math.sin(radPitch));
    const ay = Math.round(16384 * Math.sin(radRoll) * Math.cos(radPitch));
    const az = Math.round(16384 * Math.cos(radRoll) * Math.cos(radPitch));

    this._packBigEndian(REG_ACCEL_XOUT_H, ax);
    this._packBigEndian(REG_ACCEL_XOUT_H + 2, ay);
    this._packBigEndian(REG_ACCEL_XOUT_H + 4, az);

    // Temperature: (TEMP_OUT - offset) / 333.87 + 21.0
    const rawTemp = Math.round((this._temperature - 21.0) * 333.87);
    this._packBigEndian(REG_TEMP_OUT_H, rawTemp);

    // Gyroscope static zero with slight jitter
    this._packBigEndian(REG_GYRO_XOUT_H, 0);
    this._packBigEndian(REG_GYRO_XOUT_H + 2, 0);
    this._packBigEndian(REG_GYRO_XOUT_H + 4, 0);

    // Data ready interrupt
    if (this._registers[REG_INT_ENABLE] & 0x01) {
      this._registers[REG_INT_STATUS] |= 0x01;
      if (this.ctx && this._intPin >= 0) {
        this.ctx.setPinLevel(this._intPin, 1);
      }
    }

    this._publishThrottledState(ax, ay, az);
  }

  private _publishThrottledState(ax: number, ay: number, az: number): void {
    if (!this.ctx) return;
    this.ctx.publish('whoAmI', this._registers[REG_WHO_AM_I]);
    this.ctx.publish('pwrMgmt1', this._registers[REG_PWR_MGMT_1]);
    this.ctx.publish('regPointer', this._regPointer);
    this.ctx.publish('accelX', ax);
    this.ctx.publish('accelY', ay);
    this.ctx.publish('accelZ', az);
    this.ctx.publish('tempDegc', this._temperature);
    this.ctx.publish('writeCount', this._writeCount);
    this.ctx.publish('readCount', this._readCount);
  }

  private _resolveDefaultWhoAmI(): number {
    if (this._variant.includes('9250')) return 0x71;
    if (this._variant.includes('6500')) return 0x70;
    return 0x68; // mpu6050
  }

  private _resetRegisters(): void {
    this._registers.fill(0);
    this._registers[REG_WHO_AM_I] = this._resolveDefaultWhoAmI();
    this._registers[REG_PWR_MGMT_1] = 0x01; // Auto-select clock source
    this._regPointer = 0;
    this._isAddrPhase = true;

    // AK8963 defaults
    this._ak8963Regs.fill(0);
    this._ak8963Regs[AK8963_WIA] = AK8963_WIA_VAL;

    this._computeKinematics();
  }

  // ── Lifecycle Hooks ────────────────────────────────────────────────────────

  protected override onBound(
    ctx: IPluginContext<ImuState>,
    pinMapping: Record<string, number>,
    properties: Record<string, unknown>,
  ): void {
    this._variant = String(properties.variant ?? 'mpu6050_i2c');
    this._ad0Pin = pinMapping['AD0'] ?? -1;
    this._intPin = pinMapping['INT'] ?? -1;

    this._roll = Number(properties.roll ?? 0);
    this._pitch = Number(properties.pitch ?? 0);
    this._yaw = Number(properties.yaw ?? 0);
    this._temperature = Number(properties.temperature ?? 25.0);

    // Initial Address
    if (properties.address !== undefined) {
      this._address = Number(properties.address) & 0x7f;
    } else {
      this._address = DEFAULT_I2C_ADDR;
    }

    this._resetRegisters();
    this._registerI2cBus(ctx);
  }

  private _registerI2cBus(ctx: IPluginContext<ImuState>): void {
    if (this._unregisterI2c) {
      this._unregisterI2c();
      this._unregisterI2c = undefined;
    }

    const busI2c = (ctx as any)?.bus?.i2c;
    if (busI2c && typeof busI2c.registerDevice === 'function') {
      this._unregisterI2c = busI2c.registerDevice({
        address: this._address,
        onTransfer: (writeBytes: Uint8Array, readLen: number) => {
          const res = this.onTransfer(writeBytes, readLen);
          return res.readBytes ?? new Uint8Array(0);
        },
        onAddressPhase: (_direction: number) => true,
        onWriteByte: (byte: number, byteIndex: number) => this.onWriteByte(byte, byteIndex),
        onReadByte: (byteIndex: number, ack: boolean) => this.onReadByte(byteIndex, ack),
        onTransactionStart: () => this.onTransactionStart(),
        onTransactionEnd: () => this.onTransactionEnd(),
      });
    } else if (typeof (ctx as any).registerI2cDevice === 'function') {
      try {
        (ctx as any).unregisterI2cDevice?.(this._address);
      } catch {}
      (ctx as any).registerI2cDevice(this);
      this._unregisterI2c = () => {
        try {
          (ctx as any).unregisterI2cDevice?.(this._address);
        } catch {}
      };
    }

    // Check AK8963 bypass
    this._checkBypassRegistration(ctx);
  }

  private _checkBypassRegistration(ctx: IPluginContext<ImuState>): void {
    const isBypass = (this._registers[REG_INT_PIN_CFG] & 0x02) !== 0;
    if (this._variant.includes('9250') && isBypass) {
      if (!this._unregisterAk8963) {
        const ak8963Dev: I2CDevice = {
          addr: AK8963_ADDR,
          onTransactionStart: () => {},
          onWriteByte: (_b: number) => true,
          onReadByte: (idx: number) => this._ak8963Regs[idx % 16],
          onTransactionEnd: () => {},
          onTransfer: (_w: Uint8Array, rLen: number) => {
            const r = new Uint8Array(rLen);
            for (let i = 0; i < rLen; i++) r[i] = this._ak8963Regs[i % 16];
            return { ack: true, readBytes: r };
          },
        };

        const busI2c = (ctx as any)?.bus?.i2c;
        if (busI2c && typeof busI2c.registerDevice === 'function') {
          this._unregisterAk8963 = busI2c.registerDevice({
            address: AK8963_ADDR,
            onTransfer: ak8963Dev.onTransfer!,
            onAddressPhase: (_d: number) => true,
            onWriteByte: ak8963Dev.onWriteByte,
            onReadByte: ak8963Dev.onReadByte,
            onTransactionStart: ak8963Dev.onTransactionStart,
            onTransactionEnd: ak8963Dev.onTransactionEnd,
          });
        } else if (typeof (ctx as any).registerI2cDevice === 'function') {
          (ctx as any).registerI2cDevice(ak8963Dev);
          this._unregisterAk8963 = () => {
            try {
              (ctx as any).unregisterI2cDevice?.(AK8963_ADDR);
            } catch {}
          };
        }
      }
    } else if (this._unregisterAk8963) {
      this._unregisterAk8963();
      this._unregisterAk8963 = undefined;
    }
  }

  protected override onPinChange(pin: number, level: LogicState, _atUs?: bigint): void {
    if (pin === this._ad0Pin && this._ad0Pin >= 0) {
      const isHigh = level === 1;
      const newAddr = 0x68 | (isHigh ? 1 : 0);
      if (newAddr !== this._address && this.ctx) {
        this._address = newAddr;
        this._registerI2cBus(this.ctx);
      }
    }
  }

  protected override onPropertyChange(key: string, _oldValue: unknown, newValue: unknown): void {
    if (key === 'roll') this._roll = Number(newValue ?? 0);
    else if (key === 'pitch') this._pitch = Number(newValue ?? 0);
    else if (key === 'yaw') this._yaw = Number(newValue ?? 0);
    else if (key === 'temperature') this._temperature = Number(newValue ?? 25.0);
    else if (key === 'address' && this.ctx) {
      this._address = Number(newValue ?? DEFAULT_I2C_ADDR) & 0x7f;
      this._registerI2cBus(this.ctx);
    }
    this._computeKinematics();
  }

  protected override onReset(): void {
    this._resetRegisters();
    if (this.ctx && this._intPin >= 0) {
      this.ctx.setPinLevel(this._intPin, 0);
    }
  }

  protected override onDestroy(): void {
    if (this._unregisterI2c) {
      this._unregisterI2c();
      this._unregisterI2c = undefined;
    }
    if (this._unregisterAk8963) {
      this._unregisterAk8963();
      this._unregisterAk8963 = undefined;
    }
    super.onDestroy();
  }

  // ── I2C Protocol Handlers ──────────────────────────────────────────────────

  onTransactionStart(): void {
    this._isAddrPhase = true;
  }

  onTransactionEnd(): void {
    this._isAddrPhase = true;
  }

  onWriteByte(byte: number, _index: number): boolean {
    if (this._isAddrPhase) {
      this._regPointer = byte % REG_MAP_SIZE;
      this._isAddrPhase = false;
      this.ctx?.publish('regPointer', this._regPointer);
      return true;
    }

    this._registers[this._regPointer] = byte;
    this._writeCount++;

    // Soft reset if bit 7 of PWR_MGMT_1 is written
    if (this._regPointer === REG_PWR_MGMT_1 && (byte & 0x80)) {
      this._resetRegisters();
      return true;
    }

    // Bypass mode check if INT_PIN_CFG written
    if (this._regPointer === REG_INT_PIN_CFG && this.ctx) {
      this._checkBypassRegistration(this.ctx);
    }

    this.ctx?.publish('pwrMgmt1', this._registers[REG_PWR_MGMT_1]);
    this._regPointer = (this._regPointer + 1) % REG_MAP_SIZE;
    this.ctx?.publish('regPointer', this._regPointer);
    return true;
  }

  onReadByte(_byteIndex: number, _ack: boolean): number {
    const val = this._registers[this._regPointer];

    // Read-to-Clear for INT_STATUS (0x3A)
    if (this._regPointer === REG_INT_STATUS) {
      this._registers[REG_INT_STATUS] = 0;
      if (this.ctx && this._intPin >= 0) {
        this.ctx.setPinLevel(this._intPin, 0);
      }
    }

    this._regPointer = (this._regPointer + 1) % REG_MAP_SIZE;
    this._readCount++;
    this.ctx?.publish('regPointer', this._regPointer);
    this.ctx?.publish('readCount', this._readCount);
    return val;
  }

  // Whole-frame contract (for js_pal_i2c_transfer & BusAnalyzer)
  onTransfer(writeBytes: Uint8Array, readLen: number): I2CTransferResult {
    this.onTransactionStart();
    for (let i = 0; i < writeBytes.length; i++) {
      this.onWriteByte(writeBytes[i], i);
    }

    let readBytes: Uint8Array | undefined;
    if (readLen > 0) {
      readBytes = new Uint8Array(readLen);
      for (let i = 0; i < readLen; i++) {
        readBytes[i] = this.onReadByte(i, i < readLen - 1);
      }
    }
    this.onTransactionEnd();
    return { ack: true, readBytes };
  }
}

export default {
  manifest: imuManifest,
  manifestFactory: imuManifestFactory,
  PluginClass: ImuPlugin,
};
