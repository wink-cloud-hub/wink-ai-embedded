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

const identity = resolvePluginIdentity(import.meta.url, 'analog_knob', '1.0.0', 'input');

export type AnalogKnobVariant = 'standard' | 'slide' | 'logarithmic';

export function createAnalogKnobManifest(
  variantName: AnalogKnobVariant = 'standard',
): PeripheralManifest {
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: variantName === 'slide' ? 'Slide Potentiometer' : 'Analog Potentiometer Knob',
    description: 'Analog potentiometer input knob or slider',
    timingModel: 'event-driven',
    pins: [
      {
        name: 'SIG',
        direction: 'source',
        signal: 'analog',
        role: 'signal',
        aliases: ['sig', 'out'],
        required: true,
      },
      { name: 'VCC', pinType: 'vcc', role: 'power', aliases: ['5v', 'vcc'], required: false },
      { name: 'GND', pinType: 'gnd', role: 'ground', aliases: ['gnd'], required: false },
    ],
    properties: {
      value: { type: 'number', default: 0, min: 0, max: 100 },
      variant: { type: 'string', default: variantName },
    },
    stateChannels: {
      value: { type: 'number', default: 0, min: 0, max: 100 },
    },
    events: {},
  });
}

export const analogKnobManifest: PeripheralManifest = createAnalogKnobManifest();
export const analogKnobManifestFactory: ManifestFactory = (variant?: string) =>
  createAnalogKnobManifest((variant as AnalogKnobVariant) || 'standard');

export class AnalogKnobPlugin extends BaseSimulationPlugin {
  override get type(): string {
    return identity.type;
  }
  readonly manifest = analogKnobManifest;
  static readonly manifest = analogKnobManifest;

  private _sigPinName = 'SIG';

  override onBound(ctx: any, pinMapping: Record<string, number>, props: any): any {
    super.onBound?.(ctx, pinMapping, props);
    if ('SIG' in pinMapping) this._sigPinName = 'SIG';
    else if ('sig' in pinMapping) this._sigPinName = 'sig';
    else if ('out' in pinMapping) this._sigPinName = 'out';
    const pos = Number(props?.position ?? props?.value ?? 0);
    this.onSetPosition(pos);
    return { voltageRatio: Math.max(0, Math.min(100, pos)) / 100 };
  }

  onDestroy(): void {}

  onSetPosition(pos: number): void {
    const clamped = Math.max(0, Math.min(100, Number(pos)));
    const ratio = clamped / 100;
    if (this._sigPinName && this.ctx) {
      if (this.ctx.adc) {
        this.ctx.adc.writeNorm(this._sigPinName, ratio);
      } else if (typeof (this.ctx as any).analogWrite === 'function') {
        (this.ctx as any).analogWrite(this._sigPinName, ratio);
      }
      this.ctx.publish('voltageRatio', ratio);
      this.ctx.publish('value', clamped);
      this.ctx.publish('position', clamped);
    }
  }

  onPropertyChange(name: string, _oldVal: any, newVal: any): void {
    if (name === 'position' || name === 'value') {
      this.onSetPosition(Number(newVal));
    }
  }

  onPropsUpdated(props: any): void {
    const val = props?.position ?? props?.value;
    if (val !== undefined) {
      this.onSetPosition(Number(val));
    }
  }
}

export default {
  manifest: analogKnobManifest,
  manifestFactory: analogKnobManifestFactory,
  PluginClass: AnalogKnobPlugin,
};
