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

const identity = resolvePluginIdentity(import.meta.url, 'ntc', '1.0.0', 'sensor');

export type NtcVariant = 'default';

export function createNtcManifest(variantName: NtcVariant = 'default'): PeripheralManifest {
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: 'NTC Temperature Sensor',
    description: 'NTC thermistor analog temperature sensor module with pull-up divider network',
    timingModel: 'event-driven',
    pins: [
      {
        name: 'OUT',
        direction: 'source',
        signal: 'analog',
        role: 'signal',
        aliases: ['out', 'AO', 'ao', 'SIG', 'sig'],
        required: true,
      },
      { name: 'VCC', pinType: 'vcc', role: 'power', aliases: ['5v', 'vcc'], required: false },
      { name: 'GND', pinType: 'gnd', role: 'ground', aliases: ['gnd'], required: false },
    ],
    properties: {
      temperature: { type: 'number', default: 25, min: -40, max: 200 },
      r25: { type: 'number', default: 10000 },
      bValue: { type: 'number', default: 3950 },
      pullUpResistor: { type: 'number', default: 10000 },
      variant: { type: 'string', default: variantName },
    },
    stateChannels: {
      temperature: { type: 'number', default: 25, min: -40, max: 200 },
      voltageRatio: { type: 'number', default: 0.5, min: 0, max: 1 },
      resistanceOhm: { type: 'number', default: 10000 },
    },
    events: {
      SET_TEMPERATURE: {
        params: {
          temperature: { type: 'number', default: 25, min: -40, max: 200, unit: '°C' },
        },
      },
    },
  });
}

export const ntcManifest: PeripheralManifest = createNtcManifest();
export const ntcManifestFactory: ManifestFactory = (variant?: string) =>
  createNtcManifest((variant as NtcVariant) || 'default');

export class NtcPlugin extends BaseSimulationPlugin {
  override get type(): string {
    return identity.type;
  }
  readonly manifest = ntcManifest;
  static readonly manifest = ntcManifest;

  private _outPinName = 'OUT';
  private _tempC = 25;
  private _r25 = 10000;
  private _bValue = 3950;
  private _pullUpResistor = 10000;

  override onBound(ctx: any, pinMapping: Record<string, number>, props: any): any {
    super.onBound?.(ctx, pinMapping, props);
    if ('OUT' in pinMapping) this._outPinName = 'OUT';
    else if ('out' in pinMapping) this._outPinName = 'out';
    else if ('AO' in pinMapping) this._outPinName = 'AO';
    else if ('ao' in pinMapping) this._outPinName = 'ao';
    else if ('SIG' in pinMapping) this._outPinName = 'SIG';
    else if ('sig' in pinMapping) this._outPinName = 'sig';

    this._readProps(props);
    return this.updateTemperature(this._tempC);
  }

  onDestroy(): void {}

  private _readProps(props: any): void {
    if (props?.r25 !== undefined) this._r25 = Number(props.r25) || 10000;
    if (props?.bValue !== undefined) this._bValue = Number(props.bValue) || 3950;
    if (props?.pullUpResistor !== undefined)
      this._pullUpResistor = Number(props.pullUpResistor) || 10000;
    if (props?.temperature !== undefined) this._tempC = Number(props.temperature);
  }

  /**
   * 经典 NTC B 参数分压模型:
   * VCC --- R_pull ---+--- R_ntc --- GND
   * V_out / V_ref = R_ntc / (R_ntc + R_pull)
   */
  updateTemperature(degC: number): { temperature: number; voltageRatio: number; resistanceOhm: number } {
    this._tempC = Number(degC);
    const T_kelvin = this._tempC + 273.15;
    const T0 = 298.15; // 25°C in Kelvin
    const r_ntc = this._r25 * Math.exp(this._bValue * (1.0 / T_kelvin - 1.0 / T0));

    // Voltage divider: Vout = Vcc * (r_ntc / (r_ntc + r_pull))
    const ratio = Math.max(0.0, Math.min(1.0, r_ntc / (r_ntc + this._pullUpResistor)));

    if (this._outPinName && this.ctx) {
      if (this.ctx.adc) {
        this.ctx.adc.writeNorm(this._outPinName, ratio);
      } else if (typeof (this.ctx as any).analogWrite === 'function') {
        (this.ctx as any).analogWrite(this._outPinName, ratio);
      }
      this.ctx.publish('temperature', this._tempC);
      this.ctx.publish('voltageRatio', ratio);
      this.ctx.publish('resistanceOhm', r_ntc);
    }

    return { temperature: this._tempC, voltageRatio: ratio, resistanceOhm: r_ntc };
  }

  onPropertyChange(name: string, _oldVal: any, newVal: any): void {
    if (name === 'temperature') {
      this.updateTemperature(Number(newVal));
    } else if (name === 'r25' || name === 'bValue' || name === 'pullUpResistor') {
      this._readProps({ [name]: newVal });
      this.updateTemperature(this._tempC);
    }
  }

  onEvent(name: string, params: Record<string, unknown>): void {
    if (name === 'SET_TEMPERATURE') {
      const val = params.temperature ?? params.value ?? params.temp;
      if (val !== undefined) {
        this.updateTemperature(Number(val));
      }
    }
  }

  /**
   * Event handler for SET_TEMPERATURE dispatched from PeripheralControlPanel slider.
   * Method name resolved by mapEventToMethod('SET_TEMPERATURE') → '_temperature'.
   */
  _temperature(val: number): void {
    this.updateTemperature(Number(val));
  }

  onPropsUpdated(props: any): void {
    this._readProps(props);
    const val = props?.temperature;
    if (val !== undefined) {
      this.updateTemperature(Number(val));
    } else {
      this.updateTemperature(this._tempC);
    }
  }
}

export default {
  manifest: ntcManifest,
  manifestFactory: ntcManifestFactory,
  PluginClass: NtcPlugin,
};
