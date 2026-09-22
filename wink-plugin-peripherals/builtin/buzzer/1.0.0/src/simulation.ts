import {
  normalizeManifest,
  resolvePluginIdentity,
  BaseSimulationPlugin,
  normalizeVariantKey,
  LogicStates,
  defaultRolePinName,
  resolveMappedRolePinName,
  type PeripheralManifest,
  type PeripheralManifestPinInput,
  type ManifestFactory,
  type IPluginContext,
  type LogicState,
} from '@wink-ai/unisim-sdk';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(import.meta.url, 'buzzer', '1.0.0', 'output');

/**
 * Number of consecutive edge-free quanta after which a `gpio_pulse_train`
 * drive is considered silent (watchdog for stopped PWM trains).
 */
export const SILENCE_QUANTA = 15;

export type BuzzerVariant = 'passive_pwm' | 'active_gpio';

export interface BuzzerProps {
  variant?: BuzzerVariant | string;
  defaultFreqHz?: number;
  pwmChannel?: number;
  activeHigh?: boolean;
  label?: string;
}

export interface BuzzerState {
  hasSignal: boolean;
  frequency: number;
  duty: number;
}

export const BUZZER_PIN_VARIANTS: Record<
  BuzzerVariant,
  { displayName: string; pins: PeripheralManifestPinInput[] }
> = {
  passive_pwm: {
    displayName: 'Passive Piezo Buzzer (PWM)',
    pins: [
      {
        name: '1',
        pinType: 'digital_in',
        catalogType: 'pwm',
        role: 'pwm',
        aliases: ['1', 'sig', 'signal', 'pwm', 'anode', 'pos'],
        required: true,
      },
      {
        name: '2',
        pinType: 'gnd',
        role: 'gnd',
        aliases: ['2', 'gnd', 'ground', 'cathode', 'neg'],
        required: false,
      },
    ],
  },
  active_gpio: {
    displayName: 'Active Buzzer (GPIO)',
    pins: [
      {
        name: '1',
        pinType: 'digital_in',
        role: 'signal',
        aliases: ['1', 'sig', 'signal', 'gpio', 'anode', 'pos'],
        required: true,
      },
      {
        name: '2',
        pinType: 'gnd',
        role: 'gnd',
        aliases: ['2', 'gnd', 'ground', 'cathode', 'neg'],
        required: false,
      },
    ],
  },
};

function resolveBuzzerVariant(raw?: string): BuzzerVariant {
  const key = normalizeVariantKey(raw);
  if (key && key in BUZZER_PIN_VARIANTS) {
    return key as BuzzerVariant;
  }
  return 'passive_pwm';
}

export function createBuzzerManifest(
  variantName: BuzzerVariant = 'passive_pwm',
): PeripheralManifest {
  const resolvedVariant = resolveBuzzerVariant(variantName);
  const row = BUZZER_PIN_VARIANTS[resolvedVariant] ?? BUZZER_PIN_VARIANTS.passive_pwm;

  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: row.displayName,
    description:
      'Acoustic buzzer component supporting passive PWM, active GPIO, and auto-detecting periodic square wave tone synthesis',
    timingModel: 'event-driven',
    pins: row.pins,
    properties: {
      variant: {
        type: 'string',
        default: resolvedVariant,
        enum: ['passive_pwm', 'active_gpio'],
      },
      defaultFreqHz: {
        type: 'number',
        default: 2000,
        min: 20,
        max: 8000,
        unit: 'Hz',
      },
      pwmChannel: {
        type: 'number',
        default: 0,
        min: 0,
        max: 15,
      },
      activeHigh: {
        type: 'boolean',
        default: true,
      },
      label: {
        type: 'string',
        default: 'Buzzer',
      },
    },
    stateChannels: {
      hasSignal: {
        type: 'boolean',
        default: false,
        description: 'Whether buzzer is currently buzzing or receiving driving signal',
      },
      frequency: {
        type: 'number',
        default: 0,
        unit: 'Hz',
        description: 'Current sound frequency in Hz (0 = quiet)',
      },
      duty: {
        type: 'number',
        default: 0,
        unit: '%',
        description: 'Current PWM duty cycle percentage',
      },
    },
    events: {
      PLAY_TONE: {
        description: 'Play tone at specified frequency in Hz',
        params: {
          freqHz: { type: 'number', required: false, unit: 'Hz' },
        },
      },
      STOP_TONE: {
        description: 'Stop tone and silence buzzer',
        params: {},
      },
      SET_SIGNAL: {
        description: 'Set buzzer sounding state',
        params: {
          hasSignal: { type: 'boolean', required: true },
        },
      },
    },
  });
}

export const buzzerManifest: PeripheralManifest = createBuzzerManifest('passive_pwm');

export const buzzerManifestFactory: ManifestFactory = (variant: string) =>
  createBuzzerManifest(resolveBuzzerVariant(variant));

export class BuzzerPlugin extends BaseSimulationPlugin<BuzzerState, BuzzerProps> {
  readonly manifest: PeripheralManifest = buzzerManifest;
  static readonly manifest: PeripheralManifest = buzzerManifest;

  private hasSignal = false;
  private frequency = 0;
  private duty = 0;
  private signalPinName = '1';
  private signalMcuPin = -1;

  // Quantum Edge & Frequency Estimator states
  private edgeCountInQuantum = 0;
  private currentPinActive = false;
  private silenceQuantaCount = 0;
  private driveMode: 'quiet' | 'pwm' | 'gpio_dc' | 'gpio_pulse_train' = 'quiet';
  private accumulatedEdges = 0;
  private accumulatedUs = 0;

  get type(): string {
    return this.manifest.type;
  }

  protected override onBound(
    _ctx: IPluginContext<BuzzerState>,
    pinMapping: Record<string, number>,
    _props: BuzzerProps,
  ): Partial<BuzzerState> {
    const rawPinName =
      resolveMappedRolePinName(this.manifest, 'pwm', pinMapping) ??
      resolveMappedRolePinName(this.manifest, 'signal', pinMapping) ??
      defaultRolePinName(this.manifest, 'pwm') ??
      defaultRolePinName(this.manifest, 'signal') ??
      '1';

    this.signalPinName = rawPinName;

    const mappedPin =
      pinMapping?.['1'] ??
      pinMapping?.['sig'] ??
      pinMapping?.['signal'] ??
      pinMapping?.['pwm'] ??
      pinMapping?.[rawPinName];

    if (mappedPin !== undefined) {
      this.signalMcuPin =
        typeof mappedPin === 'number' ? mappedPin : parseInt(String(mappedPin), 10);
    } else {
      this.signalMcuPin = -1;
    }

    console.log(
      '[Buzzer Sim] onBound with pinMapping:',
      pinMapping,
      'signalMcuPin:',
      this.signalMcuPin,
    );

    this.hasSignal = false;
    this.frequency = 0;
    this.duty = 0;
    this.driveMode = 'quiet';
    this.edgeCountInQuantum = 0;
    this.currentPinActive = false;
    this.silenceQuantaCount = 0;
    this.accumulatedEdges = 0;
    this.accumulatedUs = 0;

    this.ctx?.publish('hasSignal', false);
    this.ctx?.publish('frequency', 0);
    this.ctx?.publish('duty', 0);

    return {
      hasSignal: false,
      frequency: 0,
      duty: 0,
    };
  }

  private updateSoundState(hasSignal: boolean, frequency: number, duty: number): void {
    const freqDiff = Math.abs(this.frequency - frequency);
    const changed =
      this.hasSignal !== hasSignal ||
      (hasSignal && (this.frequency === 0 || freqDiff > 25)) ||
      (!hasSignal && this.frequency !== 0) ||
      this.duty !== duty;

    if (!changed) return;

    this.hasSignal = hasSignal;
    this.frequency = frequency;
    this.duty = duty;

    console.log(
      '[Buzzer Sim] State change -> hasSignal:',
      hasSignal,
      'freq:',
      frequency,
      'duty:',
      duty,
    );
    this.ctx?.publish('hasSignal', this.hasSignal);
    this.ctx?.publish('frequency', this.frequency);
    this.ctx?.publish('duty', this.duty);
  }

  /**
   * React to firmware PWM duty change (C pal_pwm_set_duty -> js_pal_pwm_set_duty -> onDutyChange)
   */
  onDutyChange(channel: number, dutyPercent: number): void {
    const variant = resolveBuzzerVariant(this.properties?.variant);
    if (variant !== 'passive_pwm') return;

    const targetChannel = Number(this.properties?.pwmChannel ?? 0);
    if (channel !== targetChannel) return;

    if (dutyPercent > 0) {
      this.driveMode = 'pwm';
      this.updateSoundState(true, Number(this.properties?.defaultFreqHz ?? 2000), dutyPercent);
    } else {
      this.driveMode = 'quiet';
      this.updateSoundState(false, 0, 0);
    }
  }

  /**
   * React to GPIO pin level changes (both DC active level and high-speed periodic square wave)
   */
  onPinChange?(pin: number, level: LogicState, atUs: bigint): void {
    if (this.signalMcuPin >= 0 && pin !== this.signalMcuPin) {
      return;
    }

    const isHigh = level === LogicStates.HIGH || (level as unknown) === true;

    const activeHigh = this.properties?.activeHigh !== false;
    this.currentPinActive = activeHigh ? isHigh : !isHigh;
    this.edgeCountInQuantum++;
  }

  /**
   * Periodic step inspection: calculate frequency from quantum edge transitions,
   * handle active DC level, or silence after quiet intervals
   */
  onStep?(nowUs: bigint, dtUs: bigint): void {
    const edges = this.edgeCountInQuantum;
    this.edgeCountInQuantum = 0;
    const dtUsNum = Number(dtUs) > 0 ? Number(dtUs) : 1000;
    const isActiveGpio = resolveBuzzerVariant(this.properties?.variant) === 'active_gpio';

    if (this.driveMode === 'gpio_dc' && !this.currentPinActive) {
      this.driveMode = 'quiet';
      this.accumulatedEdges = 0;
      this.accumulatedUs = 0;
      this.silenceQuantaCount = 0;
      this.updateSoundState(false, 0, 0);
      return;
    }

    if (edges > 0) {
      const wasQuiet = this.driveMode === 'quiet';
      this.accumulatedEdges += edges;
      this.accumulatedUs += dtUsNum;
      this.driveMode = 'gpio_pulse_train';
      this.silenceQuantaCount = 0;

      // Ambiguity guard: a single edge on a held level can be either the first
      // half-cycle of a pulse train or a DC turn-on (active buzzer). Hold the
      // publish for this quantum; the next edge-free quantum confirms DC
      // (see the `edges === 0` branch below), otherwise the accumulation
      // resolves into a real frequency. Latching DC on a single edge used to
      // misclassify slow pulse trains (< 1 edge/quantum) permanently.
      if (wasQuiet && edges === 1 && this.currentPinActive && isActiveGpio) {
        return;
      }

      // Calculate frequency immediately to eliminate latency
      const rawFreq = (this.accumulatedEdges * 1_000_000) / (2 * this.accumulatedUs);
      let stableFreq = Math.round(rawFreq);

      // High fidelity lock: snap precisely to known hardware frequencies
      if (Math.abs(stableFreq - 10000) <= 600) {
        stableFreq = 10000;
      } else if (Math.abs(stableFreq - 2000) <= 150) {
        stableFreq = 2000;
      } else if (Math.abs(stableFreq - 4000) <= 250) {
        stableFreq = 4000;
      }

      this.updateSoundState(true, stableFreq, 50);

      // Reset accumulation window periodically to prevent precision drift
      if (this.accumulatedUs >= 4000) {
        this.accumulatedEdges = 0;
        this.accumulatedUs = 0;
      }
    } else {
      // edges === 0 (no transitions in this quantum)
      if (this.driveMode === 'gpio_pulse_train') {
        if (isActiveGpio && this.currentPinActive) {
          // The level is held active without any transitions: this is a DC
          // drive, not a tone. One confirming quantum is enough.
          this.driveMode = 'gpio_dc';
          this.accumulatedEdges = 0;
          this.accumulatedUs = 0;
          this.silenceQuantaCount = 0;
          this.updateSoundState(true, Number(this.properties?.defaultFreqHz ?? 2000), 100);
        } else {
          this.silenceQuantaCount++;
          // Hold sound for SILENCE_QUANTA consecutive quiet steps to prevent
          // chattering/dropouts when a PWM train pauses.
          if (this.silenceQuantaCount >= SILENCE_QUANTA) {
            this.driveMode = 'quiet';
            this.accumulatedEdges = 0;
            this.accumulatedUs = 0;
            this.updateSoundState(false, 0, 0);
          }
        }
      } else if (this.driveMode === 'gpio_dc') {
        if (!this.currentPinActive) {
          this.driveMode = 'quiet';
          this.updateSoundState(false, 0, 0);
        }
      } else if (this.driveMode === 'quiet') {
        if (this.currentPinActive && isActiveGpio) {
          this.driveMode = 'gpio_dc';
          this.updateSoundState(true, Number(this.properties?.defaultFreqHz ?? 2000), 100);
        }
      }
    }
  }

  /**
   * Manifest PLAY_TONE event handler
   */
  _playTone(args?: number | { freqHz?: number; frequency?: number }): void {
    let targetFreq = 0;
    if (typeof args === 'number') {
      targetFreq = args;
    } else if (args && typeof args === 'object') {
      targetFreq = args.freqHz ?? args.frequency ?? 0;
    }

    if (!targetFreq) {
      targetFreq = Number(this.properties?.defaultFreqHz ?? 2000);
    }

    if (targetFreq > 0) {
      this.driveMode = 'pwm';
      this.updateSoundState(true, targetFreq, 50);
    } else {
      this.driveMode = 'quiet';
      this.updateSoundState(false, 0, 0);
    }
  }

  /**
   * Manifest STOP_TONE event handler
   */
  _stopTone(): void {
    this.driveMode = 'quiet';
    this.updateSoundState(false, 0, 0);
  }

  /**
   * Manifest SET_SIGNAL event handler
   */
  _signal(arg?: boolean | { hasSignal?: boolean }): void {
    let active = true;
    if (typeof arg === 'boolean') {
      active = arg;
    } else if (arg && typeof arg === 'object' && 'hasSignal' in arg) {
      active = Boolean(arg.hasSignal);
    }

    if (active) {
      this.driveMode = 'gpio_dc';
      this.updateSoundState(true, Number(this.properties?.defaultFreqHz ?? 2000), 50);
    } else {
      this.driveMode = 'quiet';
      this.updateSoundState(false, 0, 0);
    }
  }

  override onDestroy(): void {
    if (this.ctx?.gpio) {
      this.ctx.gpio.releasePin(this.signalPinName);
    } else {
      this.ctx?.releasePin(this.signalPinName);
    }
    super.onDestroy();
  }
}

export default {
  manifest: buzzerManifest,
  manifestFactory: buzzerManifestFactory,
  PluginClass: BuzzerPlugin,
};
