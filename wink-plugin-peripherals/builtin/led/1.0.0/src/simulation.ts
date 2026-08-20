import {
  normalizeManifest,
  resolvePluginIdentity,
  SimpleGpioPlugin,
  normalizeVariantKey,
  type PeripheralManifest,
  type PeripheralManifestPinInput,
  type ManifestFactory,
} from '@wink-ai/unisim';

export type LedVariant = 'default';

export const LED_PIN_VARIANTS: Record<
  LedVariant,
  { displayName: string; pins: PeripheralManifestPinInput[] }
> = {
  default: {
    displayName: 'GPIO LED',
    pins: [
      {
        name: 'A',
        pinType: 'digital_in',
        role: 'anode',
        aliases: ['anode', 'gpio'],
        required: false,
      },
      { name: 'C', pinType: 'gnd', role: 'cathode', aliases: ['cathode'], required: false },
    ],
  },
};

function resolveLedVariant(raw?: string): LedVariant {
  const key = normalizeVariantKey(raw);
  return key && key in LED_PIN_VARIANTS ? (key as LedVariant) : 'default';
}

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(import.meta.url, 'led', '1.0.0', 'output');

export function createLedManifest(variantName: LedVariant = 'default'): PeripheralManifest {
  const row = LED_PIN_VARIANTS[variantName] ?? LED_PIN_VARIANTS.default;
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: row.displayName,
    description: 'Discrete LED with anode/cathode package pins',
    timingModel: 'event-driven',
    pins: row.pins,
    properties: {
      variant: { type: 'string', default: 'default' },
      color: {
        type: 'string',
        default: 'red',
        enum: ['red', 'green', 'blue', 'yellow', 'white', 'orange', 'purple'],
      },
      brightness: { type: 'number', default: 1.0, min: 0.0, max: 1.0 },
      activeHigh: { type: 'boolean', default: true },
    },
    stateChannels: {
      on: { type: 'boolean', default: false, description: 'Lit state' },
    },
    events: {},
  });
}

export const ledManifest: PeripheralManifest = createLedManifest('default');

export const ledManifestFactory: ManifestFactory = (variant: string) =>
  createLedManifest(resolveLedVariant(variant));

export class LedPlugin extends SimpleGpioPlugin {
  readonly manifest = ledManifest;
  static readonly manifest = ledManifest;
}

export default {
  manifest: ledManifest,
  manifestFactory: ledManifestFactory,
  PluginClass: LedPlugin,
};
