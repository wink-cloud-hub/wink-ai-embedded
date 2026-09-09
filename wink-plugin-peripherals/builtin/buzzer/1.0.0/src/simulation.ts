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
  type PluginContext,
  type LogicState,
} from '@wink-ai/unisim';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(import.meta.url, 'buzzer', '1.0.0', 'output');

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
        pinType: 'pwm',
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
    description: 'Acoustic buzzer component supporting passive PWM, active GPIO, and auto-detecting periodic square wave tone synthesis',
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

  // Pulse Train & Frequency Estimator states
  private lastEdgeUs = 0n;
  private recentIntervalsUs: number[] = [];
  private watchdogGen = 0;
  private lastTransitionAtUs = 0n;
  private driveMode: 'quiet' | 'pwm' | 'gpio_dc' | 'gpio_pulse_train' = 'quiet';

  get type(): string {
    return this.manifest.type;
  }

  protected override onBound(
    _ctx: PluginContext<BuzzerState>,
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

    if (pinMapping && pinMapping[rawPinName] !== undefined) {
      const p = pinMapping[rawPinName];
      this.signalMcuPin = typeof p === 'number' ? p : parseInt(String(p), 10);
    } else {
      this.signalMcuPin = -1;
    }

    this.hasSignal = false;
    this.frequency = 0;
    this.duty = 0;
    this.driveMode = 'quiet';
    this.lastEdgeUs = 0n;
    this.recentIntervalsUs = [];
    this.watchdogGen = 0;
    this.lastTransitionAtUs = 0n;

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
    const changed =
      this.hasSignal !== hasSignal ||
      this.frequency !== frequency ||
      this.duty !== duty;

    this.hasSignal = hasSignal;
    this.frequency = frequency;
    this.duty = duty;

    if (changed) {
      this.ctx?.publish('hasSignal', this.hasSignal);
      this.ctx?.publish('frequency', this.frequency);
      this.ctx?.publish('duty', this.duty);
    }
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
      this.updateSoundState(
        true,
        Number(this.properties?.defaultFreqHz ?? 2000),
        dutyPercent,
      );
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

    const isHigh =
      level === LogicStates.HIGH ||
      (level as unknown) === true;

    const activeHigh = this.properties?.activeHigh !== false;
    const isLevelActive = activeHigh ? isHigh : !isHigh;

    // 1. If this is a subsequent edge, inspect edge-to-edge interval
    if (this.lastEdgeUs > 0n && atUs >= this.lastEdgeUs) {
      const deltaUs = Number(atUs - this.lastEdgeUs);

      // Half-period interval in audible square wave range:
      // 25µs (20kHz half period) to 25,000µs (20Hz half period)
      if (deltaUs >= 25 && deltaUs <= 25000) {
        this.recentIntervalsUs.push(deltaUs);
        if (this.recentIntervalsUs.length > 5) {
          this.recentIntervalsUs.shift();
        }

        // Multi-edge confirmation: at least 3 consecutive edges
        if (this.recentIntervalsUs.length >= 3) {
          const sum = this.recentIntervalsUs.reduce((acc, v) => acc + v, 0);
          const avgHalfPeriodUs = sum / this.recentIntervalsUs.length;

          // Jitter check: all intervals must be within ±30% or ±10µs of average
          const isPeriodic = this.recentIntervalsUs.every(
            v => Math.abs(v - avgHalfPeriodUs) <= Math.max(10, avgHalfPeriodUs * 0.3),
          );

          if (isPeriodic && avgHalfPeriodUs > 0) {
            const periodUs = avgHalfPeriodUs * 2;
            const detectedFreq = Math.round(1_000_000 / periodUs);

            this.driveMode = 'gpio_pulse_train';
            this.updateSoundState(true, detectedFreq, 50);
          }
        }
      } else if (deltaUs > 25000) {
        // Gap is longer than 25ms -> treat as DC transition, reset periodic train
        this.recentIntervalsUs = [];
        if (isLevelActive) {
          this.driveMode = 'gpio_dc';
          this.updateSoundState(
            true,
            Number(this.properties?.defaultFreqHz ?? 2000),
            100,
          );
        } else {
          this.driveMode = 'quiet';
          this.updateSoundState(false, 0, 0);
        }
      }
    } else {
      // First edge
      this.recentIntervalsUs = [];
      if (isLevelActive) {
        this.driveMode = 'gpio_dc';
        this.updateSoundState(
          true,
          Number(this.properties?.defaultFreqHz ?? 2000),
          100,
        );
      } else {
        this.driveMode = 'quiet';
        this.updateSoundState(false, 0, 0);
      }
    }

    this.lastEdgeUs = atUs;
    this.lastTransitionAtUs = atUs;

    // 2. Silence Watchdog for pulse train:
    // If pulses stop arriving (e.g. BUZ_DisableBuzzer called), silence the buzzer!
    if (this.driveMode === 'gpio_pulse_train') {
      const currentGen = ++this.watchdogGen;
      const lastInterval =
        this.recentIntervalsUs.length > 0
          ? this.recentIntervalsUs[this.recentIntervalsUs.length - 1]
          : 50;
      const timeoutUs = Math.max(25000, lastInterval * 4);

      const ctxAny = this.ctx as any;
      if (typeof ctxAny?.deferUs === 'function') {
        ctxAny.deferUs(BigInt(timeoutUs), () => {
          if (
            this.watchdogGen === currentGen &&
            this.driveMode === 'gpio_pulse_train'
          ) {
            this.driveMode = 'quiet';
            this.recentIntervalsUs = [];
            this.updateSoundState(false, 0, 0);
          }
        });
      }
    }
  }

  /**
   * Periodic step inspection: fallback watchdog silence guarantee
   */
  onStep?(nowUs: bigint, _dtUs: bigint): void {
    if (
      this.driveMode === 'gpio_pulse_train' &&
      this.lastTransitionAtUs > 0n &&
      nowUs - this.lastTransitionAtUs > 35000n
    ) {
      this.driveMode = 'quiet';
      this.recentIntervalsUs = [];
      this.updateSoundState(false, 0, 0);
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
      this.updateSoundState(
        true,
        Number(this.properties?.defaultFreqHz ?? 2000),
        50,
      );
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
