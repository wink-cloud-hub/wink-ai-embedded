import {
  normalizeManifest,
  resolvePluginIdentity,
  BaseSimulationPlugin,
  type I2CDevice,
  type I2CTransferResult,
  type IPluginContext,
  type ManifestFactory,
  type PeripheralManifest,
} from '@wink-ai/unisim-sdk';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(
  import.meta.url,
  typeof __PLUGIN_TYPE__ !== 'undefined' ? __PLUGIN_TYPE__ : 'i2c_eeprom',
  typeof __PLUGIN_VERSION__ !== 'undefined' ? __PLUGIN_VERSION__ : '1.0.0',
  typeof __PLUGIN_CATEGORY__ !== 'undefined' ? __PLUGIN_CATEGORY__ : 'storage',
);

const DEFAULT_ADDRESS = 0x50;
const DEFAULT_SIZE = 32768; // AT24C256: 256 Kbit = 32 KiB
const DEFAULT_PAGE_SIZE = 64;

export interface I2cEepromState {
  addressPointer: number;
  busy: number;
  writeCount: number;
  readCount: number;
  lastReadByte: number;
  readbackHex: string;
}

export function createI2cEepromManifest(): PeripheralManifest {
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: 'AT24C256 I2C EEPROM',
    description: '32 KiB 24C-series I2C EEPROM with 64-byte page write and tWR NACK window',
    timingModel: 'event-driven',
    pins: [
      { name: 'VCC', pinType: 'vcc', role: 'power', required: false },
      { name: 'GND', pinType: 'gnd', role: 'ground', required: false },
      { name: 'SDA', pinType: 'i2c_sda', role: 'signal', required: true },
      { name: 'SCL', pinType: 'i2c_scl', role: 'signal', required: true },
      { name: 'WP', pinType: 'digital_in', role: 'signal', required: false },
    ],
    properties: {
      address: { type: 'number', default: DEFAULT_ADDRESS }, // 7-bit 0x50
      sizeBytes: { type: 'number', default: DEFAULT_SIZE },
      pageSize: { type: 'number', default: DEFAULT_PAGE_SIZE },
      writeCycleUs: { type: 'number', default: 0 }, // 0 = immediate (behavioral)
    },
    stateChannels: {
      addressPointer: { type: 'number', default: 0 },
      busy: { type: 'number', default: 0 },
      writeCount: { type: 'number', default: 0 },
      readCount: { type: 'number', default: 0 },
      lastReadByte: { type: 'number', default: 0 },
      /** Hex of the bytes served in the current read phase (readback proof). */
      readbackHex: { type: 'string', default: '' },
    },
    events: {},
  });
}

export const i2cEepromManifest: PeripheralManifest = createI2cEepromManifest();
export const i2cEepromManifestFactory: ManifestFactory = () => createI2cEepromManifest();

type EepromPhase = 'idle' | 'address' | 'data';

/**
 * AT24C256-class virtual EEPROM (ADR-0085/0086, plan Phase 2 T2.1).
 *
 * Implements both contracts:
 *   - whole-frame `onTransfer` for the legacy `js_pal_i2c_transfer` path;
 *   - line-level callbacks for the controller session stream: the first two
 *     payload bytes of a write transaction are the 16-bit word address, the
 *     rest is data committed on STOP with page roll-over; reads serve the
 *     sequential pointer. `writeCycleUs > 0` gates the address-phase NACK
 *     window exactly like silicon tWR.
 */
export class I2cEepromPlugin extends BaseSimulationPlugin<I2cEepromState> implements I2CDevice {
  override get type(): string {
    return identity.type;
  }
  readonly manifest = i2cEepromManifest;
  static readonly manifest = i2cEepromManifest;

  private _address = DEFAULT_ADDRESS;
  private _size = DEFAULT_SIZE;
  private _pageSize = DEFAULT_PAGE_SIZE;
  private _writeCycleUs = 0;
  private _memory: Uint8Array = new Uint8Array(DEFAULT_SIZE).fill(0xff);
  private _pointer = 0;
  private _writeAddress = 0;
  private _addressPending = false;
  private _writeData: number[] = [];
  private _addressBytes: number[] = [];
  private _phase: EepromPhase = 'idle';
  private _busyUntilUs = 0n;
  private _writeCount = 0;
  private _readCount = 0;
  private _readbackBytes: number[] = [];
  private _busyGeneration = 0;
  private _unregisterI2c?: () => void;

  get addr(): number {
    return this._address;
  }

  get addressPointer(): number {
    return this._pointer;
  }

  protected override onBound(
    ctx: IPluginContext<I2cEepromState>,
    _pinMapping: Record<string, number>,
    properties: Record<string, unknown>,
  ): void {
    this._address = Number(properties.address ?? DEFAULT_ADDRESS) & 0x7f;
    this._size = Math.max(1, Math.trunc(Number(properties.sizeBytes ?? DEFAULT_SIZE)));
    this._pageSize = Math.max(1, Math.trunc(Number(properties.pageSize ?? DEFAULT_PAGE_SIZE)));
    this._writeCycleUs = Math.max(0, Math.trunc(Number(properties.writeCycleUs ?? 0)));
    this._memory = new Uint8Array(this._size).fill(0xff);
    this._pointer = 0;
    this._writeCount = 0;
    this._readCount = 0;
    this._readbackBytes = [];

    const busI2c = this.ctx?.bus?.i2c;
    if (busI2c && typeof busI2c.registerDevice === 'function') {
      this._unregisterI2c = busI2c.registerDevice({
        address: this._address,
        onTransfer: (writeBytes: Uint8Array, readLen: number) => {
          const res = this.onTransfer(writeBytes, readLen);
          return res.readBytes ?? new Uint8Array(0);
        },
        onAddressPhase: (direction: number) => this.onAddressPhase(direction),
        onWriteByte: (byte: number, byteIndex: number) => this.onWriteByte(byte, byteIndex),
        onReadByte: (byteIndex: number, ack: boolean) => this.onReadByte(byteIndex, ack),
        onTransactionStart: () => this.onTransactionStart(),
        onTransactionEnd: () => this.onTransactionEnd(),
      });
    } else if (this.ctx) {
      this.ctx.registerI2cDevice(this);
    }
    void ctx;

    this.ctx?.publish('addressPointer', 0);
    this.ctx?.publish('busy', 0);
    this.ctx?.publish('writeCount', 0);
    this.ctx?.publish('readCount', 0);
    this.ctx?.publish('lastReadByte', 0);
    this.ctx?.publish('readbackHex', '');
  }

  override onDestroy(): void {
    if (this._unregisterI2c) {
      this._unregisterI2c();
      this._unregisterI2c = undefined;
      return;
    }
    super.onDestroy();
  }

  /** Non-volatile memory survives an MCU reset; the bus state machine does not. */
  override onReset(): void {
    this._pointer = 0;
    this._writeAddress = 0;
    this._addressPending = false;
    this._writeData = [];
    this._addressBytes = [];
    this._phase = 'idle';
    this._busyUntilUs = 0n;
    this._busyGeneration++;
    this._readCount = 0;
    this._readbackBytes = [];
    this.ctx?.publish('addressPointer', 0);
    this.ctx?.publish('busy', 0);
    this.ctx?.publish('readCount', 0);
    this.ctx?.publish('lastReadByte', 0);
    this.ctx?.publish('readbackHex', '');
  }

  // ── ADR-0085/0086 device callbacks ────────────────────────────────────────

  /** Address-phase gate: NACK while inside the tWR write cycle (silicon tWR). */
  onAddressPhase(direction: number): boolean {
    if (direction === 1) {
      // A read phase restarts the readback observation window.
      this._readbackBytes = [];
      this.ctx?.publish('readbackHex', '');
    }
    return this.ctx ? this.ctx.nowUs() >= this._busyUntilUs : true;
  }

  onTransactionStart(): void {
    this._finalizePending(false);
    this._phase = 'address';
    this._addressBytes = [];
    this._writeData = [];
    this._addressPending = false;
  }

  onTransactionEnd(): void {
    this._finalizePending(true);
    this._phase = 'idle';
  }

  onWriteByte(byte: number, _byteIndex: number): boolean {
    if (!this.onAddressPhase(0)) {
      return false;
    }
    if (this._phase === 'address') {
      this._addressBytes.push(byte & 0xff);
      if (this._addressBytes.length === 2) {
        this._writeAddress =
          (((this._addressBytes[0] << 8) | this._addressBytes[1]) >>> 0) % this._size;
        this._addressPending = true;
        this._phase = 'data';
      }
      return true;
    }
    this._addressPending = false;
    this._writeData.push(byte & 0xff);
    return true;
  }

  onReadByte(_byteIndex: number, _ack: boolean): number {
    const value = this._memory[this._pointer % this._size];
    this._pointer = (this._pointer + 1) % this._size;
    this._readCount++;
    this._readbackBytes.push(value);
    this.ctx?.publish('addressPointer', this._pointer);
    this.ctx?.publish('readCount', this._readCount);
    this.ctx?.publish('lastReadByte', value);
    this.ctx?.publish('readbackHex', this._readbackHex());
    return value;
  }

  /** Hex of the bytes served since the read phase started (scenario assertion). */
  private _readbackHex(): string {
    return this._readbackBytes.map(b => b.toString(16).padStart(2, '0')).join('');
  }

  // ── Legacy whole-frame contract ───────────────────────────────────────────

  onTransfer(writeBytes: Uint8Array, readLen: number): I2CTransferResult {
    this.onTransactionStart();
    if (!this.onAddressPhase(0)) {
      // Whole-frame refusal at the address phase: attribute the NACK to the
      // first (address) byte, matching the session addr_nack semantics.
      this.onTransactionEnd();
      return { ack: false, nackBits: 0b1, nackBitsTruncated: false, stretchUs: undefined };
    }
    let acked = true;
    let nackBits = 0;
    let nackBitsTruncated = false;
    for (let i = 0; i < writeBytes.length; i++) {
      if (!this.onWriteByte(writeBytes[i], i)) {
        acked = false;
        if (i < 32) {
          nackBits |= 1 << i;
        } else {
          nackBitsTruncated = true;
        }
      }
    }
    let readBytes: Uint8Array | undefined;
    if (acked && readLen > 0) {
      // Random Read prelude: an address-only write must move the pointer
      // before the read phase starts (repeated START semantics).
      this._finalizePending(false);
      readBytes = new Uint8Array(readLen);
      for (let i = 0; i < readLen; i++) {
        readBytes[i] = this.onReadByte(i, i < readLen - 1);
      }
    }
    this.onTransactionEnd();
    return {
      ack: acked,
      readBytes,
      nackBits: nackBits >>> 0,
      nackBitsTruncated,
      stretchUs: this._writeCycleUs > 0 ? this._writeCycleUs : undefined,
    };
  }

  // ── Snapshot (ADR-0086 §2: plugin state joins the engine state hash) ──────

  override serializeState(): Record<string, unknown> {
    return {
      addressPointer: this._pointer,
      busyUntilUs: this._busyUntilUs.toString(),
      memoryBase64: bytesToBase64(this._memory),
      writeCount: this._writeCount,
      readCount: this._readCount,
      readbackHex: this._readbackHex(),
    };
  }

  override deserializeState(snapshot: Record<string, unknown>): void {
    if (typeof snapshot.memoryBase64 === 'string') {
      const restored = base64ToBytes(snapshot.memoryBase64);
      if (restored.length === this._size) {
        this._memory = restored;
      }
    }
    if (typeof snapshot.addressPointer === 'number') {
      this._pointer = snapshot.addressPointer % this._size;
    }
    if (typeof snapshot.busyUntilUs === 'string') {
      try {
        this._busyUntilUs = BigInt(snapshot.busyUntilUs);
      } catch {
        this._busyUntilUs = 0n;
      }
    }
    if (typeof snapshot.writeCount === 'number') {
      this._writeCount = snapshot.writeCount;
    }
    if (typeof snapshot.readCount === 'number') {
      this._readCount = snapshot.readCount;
    }
    if (typeof snapshot.readbackHex === 'string') {
      const restored: number[] = [];
      for (let i = 0; i + 1 < snapshot.readbackHex.length; i += 2) {
        restored.push(Number.parseInt(snapshot.readbackHex.slice(i, i + 2), 16));
      }
      this._readbackBytes = restored;
    }
    this._phase = 'idle';
    this._busyGeneration++;
    this.ctx?.publish('addressPointer', this._pointer);
    this.ctx?.publish('busy', this._busyUntilUs > 0n ? 1 : 0);
    this.ctx?.publish('readCount', this._readCount);
    this.ctx?.publish('lastReadByte', this._readbackBytes[this._readbackBytes.length - 1] ?? 0);
    this.ctx?.publish('readbackHex', this._readbackHex());
  }

  /**
   * `withStop=false` (repeated START): an address-only prelude moves the
   * pointer (Random Read); buffered data without STOP is dropped (not a
   * silicon pattern). `withStop=true`: commit with page roll-over and arm
   * the tWR window.
   */
  private _finalizePending(withStop: boolean): void {
    if (this._writeData.length > 0 && withStop) {
      const pageStart = this._writeAddress - (this._writeAddress % this._pageSize);
      let cursor = this._writeAddress;
      for (const byte of this._writeData) {
        const offsetInPage = (cursor - pageStart) % this._pageSize;
        this._memory[pageStart + offsetInPage] = byte;
        cursor++;
      }
      this._pointer = cursor % this._size;
      this._writeCount++;
      this._addressPending = false;
      this._addressBytes = [];
      this._writeData = [];
      this.ctx?.publish('addressPointer', this._pointer);
      this.ctx?.publish('writeCount', this._writeCount);

      if (this._writeCycleUs > 0 && this.ctx) {
        this._busyUntilUs = this.ctx.nowUs() + BigInt(this._writeCycleUs);
        this.ctx.publish('busy', 1);
        const generation = ++this._busyGeneration;
        this.ctx.deferUs(BigInt(this._writeCycleUs), () => {
          if (generation === this._busyGeneration) {
            this.ctx?.publish('busy', 0);
          }
        });
      }
      return;
    }

    if (this._addressPending) {
      // Address-only transaction (Random Read prelude or a write with no
      // data): the pointer moves, nothing is committed.
      this._pointer = this._writeAddress % this._size;
      this._addressPending = false;
      this.ctx?.publish('addressPointer', this._pointer);
    }
    this._addressBytes = [];
    if (withStop) {
      this._writeData = [];
    }
  }
}

const BASE64_ALPHABET = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

function bytesToBase64(bytes: Uint8Array): string {
  let out = '';
  for (let i = 0; i < bytes.length; i += 3) {
    const b0 = bytes[i];
    const b1 = i + 1 < bytes.length ? bytes[i + 1] : 0;
    const b2 = i + 2 < bytes.length ? bytes[i + 2] : 0;
    out += BASE64_ALPHABET[b0 >> 2];
    out += BASE64_ALPHABET[((b0 & 0x03) << 4) | (b1 >> 4)];
    out += i + 1 < bytes.length ? BASE64_ALPHABET[((b1 & 0x0f) << 2) | (b2 >> 6)] : '=';
    out += i + 2 < bytes.length ? BASE64_ALPHABET[b2 & 0x3f] : '=';
  }
  return out;
}

function base64ToBytes(encoded: string): Uint8Array {
  const clean = encoded.replace(/[^A-Za-z0-9+/]/g, '');
  const length = Math.floor((clean.length * 3) / 4);
  const bytes = new Uint8Array(length);
  let outIndex = 0;
  for (let i = 0; i < clean.length; i += 4) {
    const c0 = BASE64_ALPHABET.indexOf(clean[i]);
    const c1 = BASE64_ALPHABET.indexOf(clean[i + 1]);
    const c2 = i + 2 < clean.length ? BASE64_ALPHABET.indexOf(clean[i + 2]) : -1;
    const c3 = i + 3 < clean.length ? BASE64_ALPHABET.indexOf(clean[i + 3]) : -1;
    if (outIndex < length) {
      bytes[outIndex++] = ((c0 << 2) | (c1 >> 4)) & 0xff;
    }
    if (c2 >= 0 && outIndex < length) {
      bytes[outIndex++] = ((c1 << 4) | (c2 >> 2)) & 0xff;
    }
    if (c3 >= 0 && outIndex < length) {
      bytes[outIndex++] = ((c2 << 6) | c3) & 0xff;
    }
  }
  return bytes;
}

export default {
  manifest: i2cEepromManifest,
  manifestFactory: i2cEepromManifestFactory,
  PluginClass: I2cEepromPlugin,
};
