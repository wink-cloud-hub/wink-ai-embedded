import {
  normalizeManifest,
  resolvePluginIdentity,
  BaseSimulationPlugin,
  type PeripheralManifest,
  type ManifestFactory,
} from '@wink-ai/unisim';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(
  import.meta.url,
  typeof __PLUGIN_TYPE__ !== 'undefined' ? __PLUGIN_TYPE__ : 'ws2812_strip',
  typeof __PLUGIN_VERSION__ !== 'undefined' ? __PLUGIN_VERSION__ : '1.0.0',
  typeof __PLUGIN_CATEGORY__ !== 'undefined' ? __PLUGIN_CATEGORY__ : 'display',
);

export function createWs2812Manifest(): PeripheralManifest {
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: 'WS2812 RGB LED Strip',
    description: 'Addressable RGB LED strip powered by WS2812 protocol',
    timingModel: 'event-driven',
    pins: [
      {
        name: 'DIN',
        pinType: 'digital_in',
        role: 'data',
        aliases: ['data', 'din', 'gpio'],
        required: true,
      },
      { name: 'VCC', pinType: 'vcc', role: 'power', aliases: ['5v', 'vcc'], required: false },
      { name: 'GND', pinType: 'gnd', role: 'ground', aliases: ['gnd'], required: false },
    ],
    properties: {
      numLeds: { type: 'number', default: 8, min: 1, max: 256 },
      maxFps: { type: 'number', default: 30, min: 1, max: 120 },
    },
    stateChannels: {
      pixels: { type: 'string', default: '[]', description: 'JSON array of RGB hex colors' },
    },
    events: {},
  });
}

export const ws2812Manifest: PeripheralManifest = createWs2812Manifest();

export const ws2812ManifestFactory: ManifestFactory = () => createWs2812Manifest();

export class Ws2812StripPlugin extends BaseSimulationPlugin {
  override get type(): string {
    return identity.type;
  }
  readonly manifest = ws2812Manifest;
  static readonly manifest = ws2812Manifest;

  private _latestFrame: Uint8Array | null = null;
  private _frameScheduled = false;

  protected override onBound(ctx: any, pinMapping: Record<string, number>, _props: any): void {
    const dinPin = pinMapping['DIN'] ?? pinMapping['din'] ?? pinMapping['data'];
    if (dinPin === undefined) return;

    if (ctx && typeof ctx.registerWs2812Sink === 'function') {
      ctx.registerWs2812Sink(dinPin, (frame: Uint8Array) => {
        this.onWs2812Frame(frame);
      });
    }
  }

  onWs2812Frame(frame: Uint8Array): void {
    this._latestFrame = frame;
    if (!this._frameScheduled) {
      this._frameScheduled = true;
      queueMicrotask(() => {
        this._frameScheduled = false;
        if (this._latestFrame) {
          this.flushFrame(this._latestFrame);
        }
      });
    }
  }

  private flushFrame(frame: Uint8Array): void {
    const numLeds = Number(this.properties?.numLeds ?? 8);
    const hexColors: string[] = [];
    for (let i = 0; i < numLeds; i++) {
      const r = frame[i * 3 + 0] ?? 0;
      const g = frame[i * 3 + 1] ?? 0;
      const b = frame[i * 3 + 2] ?? 0;
      hexColors.push(
        `#${r.toString(16).padStart(2, '0')}${g.toString(16).padStart(2, '0')}${b.toString(16).padStart(2, '0')}`,
      );
    }
    if (this.ctx && typeof this.ctx.publish === 'function') {
      this.ctx.publish('pixels', JSON.stringify(hexColors));
    }
  }
}

export default {
  manifest: ws2812Manifest,
  manifestFactory: ws2812ManifestFactory,
  PluginClass: Ws2812StripPlugin,
};
