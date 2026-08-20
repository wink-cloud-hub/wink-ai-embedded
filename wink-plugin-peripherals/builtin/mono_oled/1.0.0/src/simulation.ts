import {
  normalizeManifest,
  resolvePluginIdentity,
  I2cPeripheralPlugin,
  createThrottlePublish,
  normalizeVariantKey,
  type ManifestFactory,
  type PluginContext,
  type PeripheralManifest,
  type PeripheralManifestPinInput,
  type I2CTransferResult,
} from '@wink-ai/unisim';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(import.meta.url, 'mono_oled', '1.0.0', 'display');

const WIDTH = 128;
const HEIGHT = 64;
const FRAMEBUFFER_SIZE = (WIDTH * HEIGHT) / 8;
const PUBLISH_INTERVAL_US = 16_000n;

export type MonoOledVariant = 'ssd1306_i2c' | 'ssd1306_spi';
export type MonoOledIc = 'ssd1306' | 'sh1106';

export type MonoOledProps = {
  variant?: string;
  panel_ic?: string;
  i2cAddr: number;
  i2cBus: number;
};

const I2C_OLED_PINS: PeripheralManifestPinInput[] = [
  {
    name: 'DATA',
    pinType: 'i2c_sda',
    required: true,
    busGroup: 'i2c0',
    description: 'I2C SDA',
    aliases: ['sda'],
  },
  {
    name: 'CLK',
    pinType: 'i2c_scl',
    required: true,
    busGroup: 'i2c0',
    description: 'I2C SCL',
    aliases: ['scl'],
  },
  {
    name: '3V3',
    pinType: 'vcc',
    direction: 'power',
    voltage: '3.3V',
    description: 'Power 3.3V',
    required: false,
  },
  { name: 'GND', pinType: 'gnd', direction: 'ground', description: 'Ground', required: false },
];

const SPI_OLED_PINS: PeripheralManifestPinInput[] = [
  {
    name: 'CLK',
    pinType: 'spi_sck',
    required: true,
    busGroup: 'spi0',
    description: 'SPI Clock',
    aliases: ['sck', 'scl'],
  },
  {
    name: 'DIN',
    pinType: 'spi_mosi',
    required: true,
    busGroup: 'spi0',
    description: 'SPI MOSI Data',
    aliases: ['mosi', 'sda'],
  },
  {
    name: 'CS',
    pinType: 'spi_cs',
    required: false,
    busGroup: 'spi0',
    description: 'SPI Chip Select (-1 if dedicated)',
  },
  { name: 'DC', pinType: 'gpio', required: true, description: 'Data/Command Control' },
  { name: 'RES', pinType: 'gpio', required: false, description: 'Reset Pin (-1 if hardwired)' },
  {
    name: '3V3',
    pinType: 'vcc',
    direction: 'power',
    voltage: '3.3V',
    description: 'Power 3.3V',
    required: false,
  },
  { name: 'GND', pinType: 'gnd', direction: 'ground', description: 'Ground', required: false },
];

export const MONO_OLED_PIN_VARIANTS: Record<
  MonoOledVariant,
  { displayName: string; pins: PeripheralManifestPinInput[] }
> = {
  ssd1306_i2c: {
    displayName: 'SSD1306 0.96" I2C OLED Display',
    pins: I2C_OLED_PINS,
  },
  ssd1306_spi: {
    displayName: 'SSD1306 0.96" SPI OLED Display',
    pins: SPI_OLED_PINS,
  },
};

function resolveMonoOledVariant(raw?: string): MonoOledVariant {
  const key = normalizeVariantKey(raw);
  if (key?.includes('spi')) return 'ssd1306_spi';
  return 'ssd1306_i2c';
}

export function createMonoOledManifest(
  variantName: MonoOledVariant = 'ssd1306_i2c',
): PeripheralManifest {
  const row = MONO_OLED_PIN_VARIANTS[variantName] ?? MONO_OLED_PIN_VARIANTS.ssd1306_i2c;
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: row.displayName,
    description: 'Mono OLED display (SSD1306/SH1106) with a throttled framebuffer channel',
    timingModel: 'event-driven',
    pins: row.pins,
    properties: {
      variant: { type: 'string', default: variantName },
      panel_ic: { type: 'string', default: 'ssd1306' },
      i2cAddr: { type: 'number', default: 60, min: 3, max: 119 },
      i2cBus: { type: 'number', default: 0, min: 0, max: 1 },
    },
    requirements: {
      i2c: [{ bus: 0, address: '0x3C' }],
    },
    stateChannels: {
      fb: {
        type: 'string',
        show: false,
        description: 'Uint8Array binary framebuffer snapshot (128x64, page-major)',
      },
      width: { type: 'number', default: 128, description: 'Framebuffer width in pixels' },
      height: { type: 'number', default: 64, description: 'Framebuffer height in pixels' },
      colorFormat: {
        type: 'string',
        default: 'mono-ssd1306',
        show: false,
        description: 'Pixel layout format (mono-ssd1306)',
      },
      displayKind: {
        type: 'string',
        default: 'ssd1306_fb',
        show: false,
        description: 'Worker display collection kind (ssd1306_fb)',
      },
    },
    events: {},
  });
}

export const monoOledManifest: PeripheralManifest = createMonoOledManifest('ssd1306_i2c');

export const monoOledManifestFactory: ManifestFactory = (variant: string) =>
  createMonoOledManifest(resolveMonoOledVariant(variant));

export class MonoOledPlugin extends I2cPeripheralPlugin {
  private _manifest = monoOledManifest;
  static readonly manifest = monoOledManifest;

  get manifest(): PeripheralManifest {
    return this._manifest;
  }

  applyManifest(m: PeripheralManifest): void {
    this._manifest = m;
  }

  private readonly framebuffer = new Uint8Array(FRAMEBUFFER_SIZE);
  private height = HEIGHT;
  private colStart = 0;
  private colEnd = WIDTH - 1;
  private colCursor = 0;
  private pageStart = 0;
  private pageEnd = HEIGHT / 8 - 1;
  private pageCursor = 0;
  private addressMode = 0;

  private readonly throttle = createThrottlePublish({
    ctx: () => this.ctx,
    intervalUs: PUBLISH_INTERVAL_US,
    publish: () => this.publishFramebuffer(),
  });

  onDestroy(): void {
    this.throttle.reset();
    super.onDestroy();
  }

  protected onI2cBound(_ctx: PluginContext, _props: Record<string, unknown>): void {
    this.publishInitialState();
  }

  private publishInitialState(): void {
    this.ctx?.publish('displayKind', 'ssd1306_fb');
    this.ctx?.publish('width', WIDTH);
    this.ctx?.publish('height', this.height);
    this.ctx?.publish('colorFormat', 'mono-ssd1306');
    this.ctx?.publish('fb', new Uint8Array(this.framebuffer));
  }

  override onI2cTransfer(writeBuffer: Uint8Array, readLength: number): I2CTransferResult {
    return this.handleI2cTransfer(writeBuffer, readLength);
  }

  protected handleI2cTransfer(writeBytes: Uint8Array, _readLen: number): I2CTransferResult {
    if (writeBytes.length > 1) {
      if (writeBytes[0] === 0x00 || writeBytes[0] === 0x80) {
        this.parseCommands(writeBytes.subarray(1));
      } else if (writeBytes[0] === 0x40) {
        this.writeData(writeBytes.subarray(1));
      }
    }
    return { ack: true };
  }

  private parseCommands(commands: Uint8Array): void {
    for (let i = 0; i < commands.length;) {
      const command = commands[i];
      if (command === 0x21 && i + 2 < commands.length) {
        this.colStart = Math.min(commands[i + 1], WIDTH - 1);
        this.colEnd = Math.min(commands[i + 2], WIDTH - 1);
        this.colCursor = this.colStart;
        i += 3;
      } else if (command === 0x22 && i + 2 < commands.length) {
        this.pageStart = Math.min(commands[i + 1], this.maxPage);
        this.pageEnd = Math.min(commands[i + 2], this.maxPage);
        this.pageCursor = this.pageStart;
        i += 3;
      } else if (command === 0x20 && i + 1 < commands.length) {
        this.addressMode = commands[i + 1] & 0x03;
        i += 2;
      } else if (command === 0xa8 && i + 1 < commands.length) {
        this.height = commands[i + 1] === 31 ? 32 : HEIGHT;
        this.pageEnd = this.maxPage;
        this.pageCursor = Math.min(this.pageCursor, this.maxPage);
        i += 2;
      } else if (command >= 0xb0 && command <= 0xb7) {
        this.pageCursor = Math.min(command - 0xb0, this.maxPage);
        i++;
      } else if (command <= 0x0f) {
        /* SH1106 Column low address setting */
        this.colCursor = (this.colCursor & 0xf0) | (command & 0x0f);
        i++;
      } else if (command >= 0x10 && command <= 0x1f) {
        /* SH1106 Column high address setting */
        this.colCursor = (this.colCursor & 0x0f) | ((command & 0x0f) << 4);
        i++;
      } else {
        i++;
      }
    }
  }

  private writeData(data: Uint8Array): void {
    for (const byte of data) {
      const offset = this.pageCursor * WIDTH + this.colCursor;
      if (offset < FRAMEBUFFER_SIZE) {
        this.framebuffer[offset] = byte;
      }
      this.advanceCursor();
    }
    this.throttle.request();
  }

  private advanceCursor(): void {
    if (this.addressMode === 0) {
      if (++this.colCursor > this.colEnd) {
        this.colCursor = this.colStart;
        if (++this.pageCursor > this.pageEnd) this.pageCursor = this.pageStart;
      }
      return;
    }
    if (++this.colCursor >= WIDTH) this.colCursor = 0;
  }

  private publishFramebuffer(): void {
    this.ctx?.publish('displayKind', 'ssd1306_fb');
    this.ctx?.publish('fb', new Uint8Array(this.framebuffer));
    this.ctx?.publish('width', WIDTH);
    this.ctx?.publish('height', this.height);
    this.ctx?.publish('colorFormat', 'mono-ssd1306');
  }

  private get maxPage(): number {
    return this.height / 8 - 1;
  }
}

export default {
  manifest: monoOledManifest,
  manifestFactory: monoOledManifestFactory,
  PluginClass: MonoOledPlugin,
};
