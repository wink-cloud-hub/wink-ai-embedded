import {
  normalizeManifest,
  resolvePluginIdentity,
  SimpleGpioPlugin,
  normalizeVariantKey,
  type PeripheralManifest,
  type PeripheralManifestPinInput,
  type ManifestFactory,
} from '@wink-ai/unisim-sdk';

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
      label: { type: 'string', default: '' },
      activeHigh: { type: 'boolean', default: true },
      contactWelded: { type: 'boolean', default: false, description: 'Relay/switch contact welded' },
      faultType: {
        type: 'string',
        default: 'NONE',
        enum: ['NONE', 'CONTACT_WELDED'],
        description: 'Fault injection type',
      },
      pullDownResistor: {
        type: 'number',
        default: 0,
        description: 'External pull-down resistor in ohms (e.g. 4700 for MCS-51 reset clamping)',
      },
    },
    stateChannels: {
      on: { type: 'boolean', default: false, description: 'Lit / commanded state' },
      welded: { type: 'boolean', default: false, description: 'Contact welded fault state' },
      contactClosed: { type: 'boolean', default: false, description: 'Physical contact closed state' },
    },
    events: {
      INJECT_FAULT: {
        description: 'Inject electrical or mechanical fault',
        params: {
          faultType: { type: 'string', default: 'CONTACT_WELDED' },
        },
      },
      CLEAR_FAULT: {
        description: 'Clear active fault',
        params: {},
      },
    },
  });
}

export const ledManifest: PeripheralManifest = createLedManifest('default');

export const ledManifestFactory: ManifestFactory = (variant: string) =>
  createLedManifest(resolveLedVariant(variant));

export class LedPlugin extends SimpleGpioPlugin {
  readonly manifest = ledManifest;
  static readonly manifest = ledManifest;

  private _isWelded = false;
  private _pinOn = false;

  protected override onBound(
    ctx: any,
    pinMapping: Record<string, number>,
    props: Record<string, unknown>,
  ): Record<string, unknown> | void {
    const initialOutputs = super.onBound(ctx, pinMapping, props) || {};
    this._isWelded = Boolean(props?.welded ?? props?.contactWelded);
    if (this.ctx) {
      this.ctx.publish('welded', this._isWelded);
      this.ctx.publish('contactClosed', this._isWelded || this._pinOn);
    }
    return {
      ...initialOutputs,
      welded: this._isWelded,
      contactClosed: this._isWelded || this._pinOn,
    };
  }

  override onPinChange(eventOrPin: any, level?: any, atUs?: bigint): void {
    const rawPin =
      typeof eventOrPin === 'object' && eventOrPin !== null ? eventOrPin.pin : eventOrPin;
    const rawState =
      typeof eventOrPin === 'object' && eventOrPin !== null ? eventOrPin.state : level;
    const pinNum = typeof rawPin === 'number' ? rawPin : parseInt(String(rawPin), 10);

    if (this.signalMcuPin >= 0 && !isNaN(pinNum) && pinNum !== this.signalMcuPin) return;

    const isHigh = rawState === 1 || rawState === true;
    const activeHigh = (this.properties as any)?.activeHigh ?? true;
    const activeLow = (this.properties as any)?.activeLow ?? false;
    const effectiveActiveHigh = activeLow ? false : activeHigh;
    this._pinOn = effectiveActiveHigh ? isHigh : !isHigh;

    if (this.ctx) {
      this.ctx.publish('on', this._pinOn);
      if (this._isWelded) {
        this.ctx.publish('welded', true);
        this.ctx.publish('contactClosed', true);
      }
    }
  }

  _injectFault(params: unknown): void {
    const faultType =
      typeof params === 'object' && params !== null
        ? ((params as Record<string, unknown>).faultType ?? 'CONTACT_WELDED')
        : params;
    if (faultType === 'CONTACT_WELDED' || faultType === true) {
      this._isWelded = true;
      if (this.ctx) {
        this.ctx.publish('welded', true);
        this.ctx.publish('contactClosed', true);
      }
    }
  }

  _clearFault(): void {
    this._isWelded = false;
    if (this.ctx) {
      this.ctx.publish('welded', false);
      this.ctx.publish('contactClosed', this._pinOn);
    }
  }
}

export default {
  manifest: ledManifest,
  manifestFactory: ledManifestFactory,
  PluginClass: LedPlugin,
};
