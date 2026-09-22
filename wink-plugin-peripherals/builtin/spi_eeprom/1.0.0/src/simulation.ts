import {
  normalizeManifest,
  resolvePluginIdentity,
  BaseSimulationPlugin,
  type IPluginContext,
  type ManifestFactory,
  type PeripheralManifest,
} from '@wink-ai/unisim-sdk';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(
  import.meta.url,
  typeof __PLUGIN_TYPE__ !== 'undefined' ? __PLUGIN_TYPE__ : 'spi_eeprom',
  typeof __PLUGIN_VERSION__ !== 'undefined' ? __PLUGIN_VERSION__ : '1.0.0',
  typeof __PLUGIN_CATEGORY__ !== 'undefined' ? __PLUGIN_CATEGORY__ : 'storage',
);

const DEFAULT_SIZE = 32768; // M95256: 256 Kbit = 32 KiB
const DEFAULT_PAGE_SIZE = 64;

// M95256 command set (datasheet).
const CMD_WRSR = 0x01;
const CMD_WRITE = 0x02;
const CMD_READ = 0x03;
const CMD_WRDI = 0x04;
const CMD_RDSR = 0x05;
const CMD_WREN = 0x06;

export interface SpiEepromState {
  wel: number;
  wip: number;
  writeCount: number;
  readCount: number;
  lastReadByte: number;
  readbackHex: string;
}

export function createSpiEepromManifest(): PeripheralManifest {
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: 'M95256 SPI EEPROM',
    description: '32 KiB 25/95-series SPI EEPROM with 64-byte page write, WEL latch and WIP window',
    timingModel: 'event-driven',
    pins: [
      { name: 'VCC', pinType: 'vcc', role: 'power', required: false },
      { name: 'GND', pinType: 'gnd', role: 'ground', required: false },
      { name: 'SCK', pinType: 'spi_sck', role: 'signal', required: true },
      { name: 'MOSI', pinType: 'spi_mosi', role: 'signal', required: true },
      { name: 'MISO', pinType: 'spi_miso', role: 'signal', required: true },
      { name: 'CS', pinType: 'spi_cs', role: 'signal', required: true },
      { name: 'W', pinType: 'digital_in', role: 'signal', required: false },
    ],
    properties: {
      deviceId: { type: 'string', default: 'spi_eeprom' },
      sizeBytes: { type: 'number', default: DEFAULT_SIZE },
      pageSize: { type: 'number', default: DEFAULT_PAGE_SIZE },
      writeCycleUs: { type: 'number', default: 0 }, // 0 = immediate (behavioral)
    },
    stateChannels: {
      wel: { type: 'number', default: 0 },
      wip: { type: 'number', default: 0 },
      writeCount: { type: 'number', default: 0 },
      readCount: { type: 'number', default: 0 },
      lastReadByte: { type: 'number', default: 0 },
      /** Hex of the READ data bytes served in the current frame (readback proof). */
      readbackHex: { type: 'string', default: '' },
    },
    events: {},
  });
}

export const spiEepromManifest: PeripheralManifest = createSpiEepromManifest();
export const spiEepromManifestFactory: ManifestFactory = () => createSpiEepromManifest();

type FramePhase = 'idle' | 'cmd' | 'addr' | 'data' | 'status';

/**
 * M95256-class virtual SPI EEPROM (ADR-0087, plan Phase 2 T2.2).
 *
 * One CS-low window is one instruction frame. `onExchangeByte` drives the
 * frame FSM for session traffic; `onFrame` wraps the same FSM for the legacy
 * whole-frame path. All latch semantics (WEL set on the WREN frame's CS
 * rising edge, WRITE commit + WEL clear on the write frame's rising edge,
 * WIP until tW) follow the datasheet.
 */
export class SpiEepromPlugin extends BaseSimulationPlugin<SpiEepromState> {
  override get type(): string {
    return identity.type;
  }
  readonly manifest = spiEepromManifest;
  static readonly manifest = spiEepromManifest;

  private _deviceId = 'spi_eeprom';
  private _size = DEFAULT_SIZE;
  private _pageSize = DEFAULT_PAGE_SIZE;
  private _writeCycleUs = 0;
  private _memory: Uint8Array = new Uint8Array(DEFAULT_SIZE).fill(0xff);
  private _wel = false;
  private _wipUntilUs = 0n;
  private _writeCount = 0;
  private _readCount = 0;
  private _readbackBytes: number[] = [];
  private _wipGeneration = 0;
  private _phase: FramePhase = 'idle';
  private _cmd = 0;
  private _ignored = false;
  private _addrBytes: number[] = [];
  private _address = 0;
  private _writeData: number[] = [];
  private _unregisterSpi?: () => void;

  protected override onBound(
    ctx: IPluginContext<SpiEepromState>,
    _pinMapping: Record<string, number>,
    properties: Record<string, unknown>,
  ): void {
    this._deviceId = String(properties.deviceId ?? 'spi_eeprom');
    this._size = Math.max(1, Math.trunc(Number(properties.sizeBytes ?? DEFAULT_SIZE)));
    this._pageSize = Math.max(1, Math.trunc(Number(properties.pageSize ?? DEFAULT_PAGE_SIZE)));
    this._writeCycleUs = Math.max(0, Math.trunc(Number(properties.writeCycleUs ?? 0)));
    this._memory = new Uint8Array(this._size).fill(0xff);
    this._wel = false;
    this._wipUntilUs = 0n;
    this._writeCount = 0;
    this._readCount = 0;
    this._readbackBytes = [];

    const busSpi = this.ctx?.bus?.spi;
    if (busSpi && typeof busSpi.registerDevice === 'function') {
      this._unregisterSpi = busSpi.registerDevice({
        deviceId: this._deviceId,
        mode: 0,
        csPin: 'CS',
        onTransactionStart: () => this.onTransactionStart(),
        onExchangeByte: (mosi: number, byteIndex: number) => this.onExchangeByte(mosi, byteIndex),
        onFrame: (tx: Uint8Array) => this.onFrame(tx),
        onTransactionEnd: () => this.onTransactionEnd(),
      });
    }
    void ctx;

    this.ctx?.publish('wel', 0);
    this.ctx?.publish('wip', 0);
    this.ctx?.publish('writeCount', 0);
    this.ctx?.publish('readCount', 0);
    this.ctx?.publish('lastReadByte', 0);
    this.ctx?.publish('readbackHex', '');
  }

  override onDestroy(): void {
    if (this._unregisterSpi) {
      this._unregisterSpi();
      this._unregisterSpi = undefined;
      return;
    }
    super.onDestroy();
  }

  /** Non-volatile memory survives an MCU reset; WEL and the tW window do not. */
  override onReset(): void {
    this._wel = false;
    this._wipUntilUs = 0n;
    this._wipGeneration++;
    this._readCount = 0;
    this._readbackBytes = [];
    this._resetFrame();
    this.ctx?.publish('wel', 0);
    this.ctx?.publish('wip', 0);
    this.ctx?.publish('readCount', 0);
    this.ctx?.publish('lastReadByte', 0);
    this.ctx?.publish('readbackHex', '');
  }

  isWelSet(): boolean {
    return this._wel;
  }

  isWip(): boolean {
    return this._isWipActive();
  }

  // ── ADR-0087 frame lifecycle ──────────────────────────────────────────────

  onTransactionStart(): void {
    this._resetFrame();
    // A new frame restarts the readback observation window.
    this._readbackBytes = [];
    this.ctx?.publish('readbackHex', '');
  }

  onExchangeByte(mosi: number, _byteIndex: number): number {
    if (this._phase === 'idle') {
      return 0xff;
    }
    if (this._phase === 'cmd') {
      this._cmd = mosi & 0xff;
      if (this._isWipActive() && this._cmd !== CMD_RDSR) {
        // While WIP only RDSR is accepted (datasheet): the frame is ignored.
        this._ignored = true;
        this._phase = 'status';
        return 0xff;
      }
      switch (this._cmd) {
        case CMD_READ:
        case CMD_WRITE:
          this._phase = 'addr';
          break;
        case CMD_WREN:
        case CMD_WRDI:
        case CMD_RDSR:
        case CMD_WRSR:
        default:
          this._phase = 'status';
          break;
      }
      return 0x00;
    }

    if (this._phase === 'addr') {
      this._addrBytes.push(mosi & 0xff);
      if (this._addrBytes.length === 2) {
        this._address = (((this._addrBytes[0] << 8) | this._addrBytes[1]) >>> 0) % this._size;
        this._phase = 'data';
      }
      return 0x00;
    }

    if (this._phase === 'data') {
      if (this._cmd === CMD_READ) {
        const value = this._memory[this._address % this._size];
        this._address = (this._address + 1) % this._size;
        this._readCount++;
        this._readbackBytes.push(value);
        this.ctx?.publish('readCount', this._readCount);
        this.ctx?.publish('lastReadByte', value);
        this.ctx?.publish('readbackHex', this._readbackHex());
        return value;
      }
      if (this._cmd === CMD_WRITE) {
        this._writeData.push(mosi & 0xff);
      }
      return 0x00;
    }

    // 'status' phase: RDSR streams the status byte; others are no-ops.
    if (this._cmd === CMD_RDSR && !this._ignored) {
      return this._status();
    }
    return 0x00;
  }

  /** CS rising edge = frame end; WREN/WRDI/WRITE latch semantics apply here. */
  onTransactionEnd(): void {
    const wasIgnored = this._ignored;
    const cmd = this._cmd;
    if (!wasIgnored) {
      if (cmd === CMD_WREN) {
        this._wel = true;
      } else if (cmd === CMD_WRDI) {
        this._wel = false;
      } else if (cmd === CMD_WRITE) {
        if (this._wel && !this._isWipActive() && this._writeData.length > 0) {
          this._commitWrite();
        }
        this._wel = false; // WEL is cleared when the write cycle starts/completes
      }
    }
    this._resetFrame();
    this.ctx?.publish('wel', this._wel ? 1 : 0);
    this.ctx?.publish('wip', this._isWipActive() ? 1 : 0);
  }

  /** Legacy whole-frame path: one call = one CS-low frame. */
  onFrame(tx: Uint8Array): Uint8Array {
    this.onTransactionStart();
    const rx = new Uint8Array(tx.length);
    for (let i = 0; i < tx.length; i++) {
      rx[i] = this.onExchangeByte(tx[i], i);
    }
    this.onTransactionEnd();
    return rx;
  }

  // ── Snapshot (ADR-0087 §2: plugin state joins the engine state hash) ──────

  override serializeState(): Record<string, unknown> {
    return {
      wel: this._wel ? 1 : 0,
      wipUntilUs: this._wipUntilUs.toString(),
      writeCount: this._writeCount,
      readCount: this._readCount,
      readbackHex: this._readbackHex(),
      memoryBase64: bytesToBase64(this._memory),
    };
  }

  override deserializeState(snapshot: Record<string, unknown>): void {
    if (typeof snapshot.memoryBase64 === 'string') {
      const restored = base64ToBytes(snapshot.memoryBase64);
      if (restored.length === this._size) {
        this._memory = restored;
      }
    }
    this._wel = snapshot.wel === 1;
    if (typeof snapshot.wipUntilUs === 'string') {
      try {
        this._wipUntilUs = BigInt(snapshot.wipUntilUs);
      } catch {
        this._wipUntilUs = 0n;
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
    this._wipGeneration++;
    this._resetFrame();
    this.ctx?.publish('wel', this._wel ? 1 : 0);
    this.ctx?.publish('wip', this._isWipActive() ? 1 : 0);
    this.ctx?.publish('readCount', this._readCount);
    this.ctx?.publish('lastReadByte', this._readbackBytes[this._readbackBytes.length - 1] ?? 0);
    this.ctx?.publish('readbackHex', this._readbackHex());
  }

  private _readbackHex(): string {
    return this._readbackBytes.map(b => b.toString(16).padStart(2, '0')).join('');
  }

  private _status(): number {
    return (this._wel ? 0b10 : 0) | (this._isWipActive() ? 0b01 : 0);
  }

  private _isWipActive(): boolean {
    return this.ctx ? this.ctx.nowUs() < this._wipUntilUs : false;
  }

  private _resetFrame(): void {
    this._phase = 'cmd';
    this._cmd = 0;
    this._ignored = false;
    this._addrBytes = [];
    this._writeData = [];
  }

  private _commitWrite(): void {
    const pageStart = this._address - (this._address % this._pageSize);
    let cursor = this._address;
    for (const byte of this._writeData) {
      const offsetInPage = (cursor - pageStart) % this._pageSize;
      this._memory[pageStart + offsetInPage] = byte;
      cursor++;
    }
    this._writeCount++;
    this.ctx?.publish('writeCount', this._writeCount);

    if (this._writeCycleUs > 0 && this.ctx) {
      this._wipUntilUs = this.ctx.nowUs() + BigInt(this._writeCycleUs);
      this.ctx.publish('wip', 1);
      const generation = ++this._wipGeneration;
      this.ctx.deferUs(BigInt(this._writeCycleUs), () => {
        if (generation === this._wipGeneration) {
          this.ctx?.publish('wip', 0);
        }
      });
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
  manifest: spiEepromManifest,
  manifestFactory: spiEepromManifestFactory,
  PluginClass: SpiEepromPlugin,
};
