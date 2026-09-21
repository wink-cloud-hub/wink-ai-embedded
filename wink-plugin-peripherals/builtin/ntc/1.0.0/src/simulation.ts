import {
  normalizeManifest,
  resolvePluginIdentity,
  BaseSimulationPlugin,
  type PeripheralManifest,
  type ManifestFactory,
} from '@wink-ai/unisim-sdk';

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
      label: { type: 'string', default: '' },
      vref: { type: 'number', default: 3.0, description: 'Nominal reference voltage in volts' },
      vrefNoisePct: {
        type: 'number',
        default: 0,
        min: 0,
        max: 5,
        description: 'Vref random noise percentage (e.g. 0.1 for +/-0.1%)',
      },
      lineRegulationPct: {
        type: 'number',
        default: 0,
        description: 'LDO line regulation drift percentage (delta_line)',
      },
      tempDriftPpm: {
        type: 'number',
        default: 0,
        description: 'LDO temperature coefficient in ppm/degC (delta_temp)',
      },
      gndBouncePct: {
        type: 'number',
        default: 0,
        description: 'Ground bounce perturbation percentage when heater active (delta_gnd_bounce)',
      },
      heaterActive: {
        type: 'boolean',
        default: false,
        description: 'Heater active state causing ground bounce',
      },
      prngSeed: {
        type: 'number',
        default: 42,
        description: 'Deterministic PRNG seed for noise generation',
      },
    },
    stateChannels: {
      temperature: { type: 'number', default: 25, min: -40, max: 200 },
      voltageRatio: { type: 'number', default: 0.5, min: 0, max: 1 },
      resistanceOhm: { type: 'number', default: 10000 },
      vrefEffective: { type: 'number', default: 3.0 },
    },
    events: {
      SET_TEMPERATURE: {
        params: {
          temperature: { type: 'number', default: 25, min: -40, max: 200, unit: '°C' },
        },
      },
      SET_VREF_PERTURBATION: {
        params: {
          vref_noise_pct: { type: 'number' },
          line_regulation_pct: { type: 'number' },
          temp_drift_ppm: { type: 'number' },
          gnd_bounce_pct: { type: 'number' },
          heater_active: { type: 'boolean' },
        },
      },
      SET_HEATER_ACTIVE: {
        params: {
          active: { type: 'boolean', required: true },
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
  private _vrefNominal = 3.0;
  private _vrefNoisePct = 0;
  private _lineRegulationPct = 0;
  private _tempDriftPpm = 0;
  private _gndBouncePct = 0;
  private _heaterOn = false;
  private _prngSeed = 42;
  private _prngState = 42;

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
    if (props?.vref !== undefined) this._vrefNominal = Number(props.vref) || 3.0;

    const noisePct = props?.vref_noise_pct ?? props?.vrefNoisePct;
    if (noisePct !== undefined) this._vrefNoisePct = Number(noisePct) || 0;

    const linePct = props?.line_regulation_pct ?? props?.lineRegulationPct;
    if (linePct !== undefined) this._lineRegulationPct = Number(linePct) || 0;

    const tempDrift = props?.temp_drift_ppm ?? props?.tempDriftPpm;
    if (tempDrift !== undefined) this._tempDriftPpm = Number(tempDrift) || 0;

    const gndBounce = props?.gnd_bounce_pct ?? props?.gndBouncePct;
    if (gndBounce !== undefined) this._gndBouncePct = Number(gndBounce) || 0;

    const heaterActive = props?.heater_active ?? props?.heaterActive;
    if (heaterActive !== undefined) this._heaterOn = Boolean(heaterActive);

    const seed = props?.prng_seed ?? props?.prngSeed ?? props?.seed;
    if (seed !== undefined) {
      this._prngSeed = Number(seed) || 42;
      this._prngState = this._prngSeed;
    }
  }

  private _prngNext(): number {
    this._prngState = (this._prngState * 16807) % 2147483647;
    // Map to [-1.0, 1.0]
    return ((this._prngState - 1) / 2147483646) * 2.0 - 1.0;
  }

  /**
   * 经典 NTC B 参数分压模型与片内 LDO 基准微扰 (PLAN-20260921 Item 01):
   * V_ref(t) = 3.0V * [1 + delta_line(VDD) + delta_temp(T_chip) + delta_gnd_bounce(Pin) + delta_noise]
   * VCC --- R_pull ---+--- R_ntc --- GND
   * V_out / V_ref = (R_ntc / (R_ntc + R_pull)) / (1 + delta_total)
   */
  updateTemperature(degC: number): {
    temperature: number;
    voltageRatio: number;
    resistanceOhm: number;
    vrefEffective: number;
  } {
    this._tempC = Number(degC);
    const T_kelvin = this._tempC + 273.15;
    const T0 = 298.15; // 25°C in Kelvin
    const r_ntc = this._r25 * Math.exp(this._bValue * (1.0 / T_kelvin - 1.0 / T0));

    // Voltage divider nominal ratio: Vout / Vcc = r_ntc / (r_ntc + r_pull)
    const ratioNominal = Math.max(0.0, Math.min(1.0, r_ntc / (r_ntc + this._pullUpResistor)));

    // Electrical perturbation model:
    const delta_line = this._lineRegulationPct / 100.0;
    const delta_temp = (this._tempC - 25.0) * (this._tempDriftPpm * 1e-6);
    const delta_gnd_bounce = this._heaterOn ? this._gndBouncePct / 100.0 : 0.0;
    const delta_noise =
      this._vrefNoisePct > 0 ? this._prngNext() * (this._vrefNoisePct / 100.0) : 0.0;
    const delta_total = delta_line + delta_temp + delta_gnd_bounce + delta_noise;

    const vrefEffective = this._vrefNominal * (1.0 + delta_total);

    // Effective ratio measured against Vref(t)
    const ratio = Math.max(0.0, Math.min(1.0, ratioNominal / (1.0 + delta_total)));

    if (this._outPinName && this.ctx) {
      if (this.ctx.adc) {
        this.ctx.adc.writeNorm(this._outPinName, ratio);
      } else if (typeof (this.ctx as any).analogWrite === 'function') {
        (this.ctx as any).analogWrite(this._outPinName, ratio);
      }
      this.ctx.publish('temperature', this._tempC);
      this.ctx.publish('voltageRatio', ratio);
      this.ctx.publish('resistanceOhm', r_ntc);
      this.ctx.publish('vrefEffective', vrefEffective);
    }

    return {
      temperature: this._tempC,
      voltageRatio: ratio,
      resistanceOhm: r_ntc,
      vrefEffective,
    };
  }

  onPropertyChange(name: string, _oldVal: any, newVal: any): void {
    if (name === 'temperature') {
      this.updateTemperature(Number(newVal));
    } else if (
      name === 'r25' ||
      name === 'bValue' ||
      name === 'pullUpResistor' ||
      name === 'vref' ||
      name === 'vref_noise_pct' ||
      name === 'vrefNoisePct' ||
      name === 'line_regulation_pct' ||
      name === 'lineRegulationPct' ||
      name === 'temp_drift_ppm' ||
      name === 'tempDriftPpm' ||
      name === 'gnd_bounce_pct' ||
      name === 'gndBouncePct' ||
      name === 'heater_active' ||
      name === 'heaterActive'
    ) {
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
    } else if (name === 'SET_VREF_PERTURBATION') {
      this._readProps(params);
      this.updateTemperature(this._tempC);
    } else if (name === 'SET_HEATER_ACTIVE') {
      const active = params.active ?? params.heaterActive ?? params.heater_active;
      if (active !== undefined) {
        this._heaterOn = Boolean(active);
        this.updateTemperature(this._tempC);
      }
    }
  }

  /**
   * Event handler for SET_TEMPERATURE dispatched from PeripheralControlPanel slider.
   * Method name resolved by mapEventToMethod('SET_TEMPERATURE') → '_temperature'.
   */
  _temperature(val: unknown): void {
    const num =
      typeof val === 'object' && val !== null
        ? ((val as Record<string, unknown>).temperature ??
          (val as Record<string, unknown>).value ??
          (val as Record<string, unknown>).temp)
        : val;
    this.updateTemperature(Number(num));
  }

  _vrefPerturbation(params: unknown): void {
    if (typeof params === 'object' && params !== null) {
      this._readProps(params);
      this.updateTemperature(this._tempC);
    }
  }

  _heaterActive(val: unknown): void {
    const active =
      typeof val === 'object' && val !== null
        ? ((val as Record<string, unknown>).active ??
          (val as Record<string, unknown>).heater_active ??
          (val as Record<string, unknown>).heaterActive)
        : val;
    if (active !== undefined) {
      this._heaterOn = Boolean(active);
      this.updateTemperature(this._tempC);
    }
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
