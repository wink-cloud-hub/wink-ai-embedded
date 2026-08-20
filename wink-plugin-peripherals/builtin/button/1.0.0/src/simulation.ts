import {
  normalizeManifest,
  resolvePluginIdentity,
  SimpleGpioPlugin,
  normalizeVariantKey,
  type PeripheralManifest,
  type PeripheralManifestPinInput,
  type ManifestFactory,
} from '@wink-ai/unisim';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(import.meta.url, 'button', '1.0.0', 'input');

export type ButtonVariant = 'default';

export const BUTTON_PIN_VARIANTS: Record<
  ButtonVariant,
  { displayName: string; pins: PeripheralManifestPinInput[] }
> = {
  default: {
    displayName: 'GPIO Push Button',
    pins: [
      {
        name: '1.l',
        pinType: 'digital_out',
        role: 'signal',
        aliases: ['signal', 'gpio'],
        required: false,
      },
      { name: '2.l', pinType: 'gnd', required: false },
      { name: '1.r', pinType: 'digital_out', role: 'signal', required: false },
      { name: '2.r', pinType: 'gnd', required: false },
    ],
  },
};

function resolveButtonVariant(raw?: string): ButtonVariant {
  const key = normalizeVariantKey(raw);
  return key && key in BUTTON_PIN_VARIANTS ? (key as ButtonVariant) : 'default';
}

export function createButtonManifest(variantName: ButtonVariant = 'default'): PeripheralManifest {
  const row = BUTTON_PIN_VARIANTS[variantName] ?? BUTTON_PIN_VARIANTS.default;
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: row.displayName,
    description: 'Momentary push button with dual-side package pins',
    timingModel: 'event-driven',
    pins: row.pins,
    properties: {
      variant: { type: 'string', default: 'default' },
      color: {
        type: 'string',
        default: 'red',
        enum: ['red', 'green', 'blue', 'yellow', 'white', 'black'],
      },
      activeLow: { type: 'boolean', default: true },
      autoPollMs: { type: 'number', default: 10, min: 0, max: 10000, unit: 'ms' },
      debounceMs: { type: 'number', default: 20, min: 0, max: 1000, unit: 'ms' },
      longPressMs: { type: 'number', default: 3000, min: 0, max: 60000, unit: 'ms' },
      isrCounter: { type: 'boolean', default: false },
    },
    stateChannels: {
      pressed: { type: 'boolean', default: false, description: 'Pressed state' },
    },
    events: {
      SET_PRESSED: {
        description: 'Set button pressed state',
        params: {
          pressed: { type: 'boolean', required: true },
        },
      },
      PRESS: {
        description: 'Press button',
        params: {},
      },
      RELEASE: {
        description: 'Release button',
        params: {},
      },
    },
  });
}

export const buttonManifest: PeripheralManifest = createButtonManifest('default');

export const buttonManifestFactory: ManifestFactory = (variant: string) =>
  createButtonManifest(resolveButtonVariant(variant));

export class ButtonPlugin extends SimpleGpioPlugin {
  readonly manifest = buttonManifest;
  static readonly manifest = buttonManifest;

  private _pressedState = false;
  private _activeLow = true;
  private _signalPinName = '1.l';

  protected override onBound(
    ctx: any,
    pinMapping: Record<string, number>,
    props: Record<string, unknown>,
  ): Record<string, unknown> | void {
    const initialOutputs = super.onBound(ctx, pinMapping, props);
    this._activeLow = Boolean(props.activeLow ?? true);

    const pinList = Array.isArray(this.manifest.pins) ? this.manifest.pins : [];
    const pinName =
      pinList.find(p => (p.simRole ?? p.name).toLowerCase() === 'signal')?.name ?? '1.l';
    this._signalPinName = pinName;

    const idleLevel = this._activeLow;
    this.ctx?.writePin(this._signalPinName, idleLevel);
    this.ctx?.publish('pressed', false);
    return { ...(initialOutputs ?? {}), pressed: false };
  }

  _pressed(arg?: boolean | { pressed?: boolean }): void {
    const pressed =
      typeof arg === 'object' && arg !== null ? Boolean(arg.pressed) : Boolean(arg ?? true);
    this._pressedState = pressed;
    this.ctx?.publish('pressed', pressed);
    const pinLevel = this._activeLow ? !pressed : pressed;
    this.ctx?.writePin(this._signalPinName, pinLevel);
  }

  _press(): void {
    this._pressed(true);
  }

  _release(): void {
    this._pressed(false);
  }
}

export default {
  manifest: buttonManifest,
  manifestFactory: buttonManifestFactory,
  PluginClass: ButtonPlugin,
};
