import {
  normalizeManifest,
  resolvePluginIdentity,
  SimpleGpioPlugin,
  normalizeVariantKey,
  type PeripheralManifest,
  type PeripheralManifestPinInput,
  type ManifestFactory,
  type IPluginContext,
} from '@wink-ai/unisim-sdk';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(import.meta.url, 'button', '1.0.0', 'input');

export type ButtonVariant = 'default';

export type BounceModel = 'deterministic_train' | 'stochastic_fretting';

export interface ButtonPressOptions {
  pressed?: boolean;
  pressDurationUs?: number | string;
  bounceUs?: number | string;
  bounceCount?: number;
  bounceModel?: BounceModel;
  chatterDurationRangeUs?: [number, number] | number[];
  contactResistanceRangeOhm?: [number, number] | number[];
  pullupResistanceOhm?: number;
  seed?: number;
}

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
      label: { type: 'string', default: '' },
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
        description: 'Set button pressed state (atomic press/release waveform pair in timing mode)',
        params: {
          pressed: { type: 'boolean', required: true },
          pressDurationUs: {
            type: 'number',
            default: 0,
            min: 0,
            max: 60000000,
            unit: 'us',
            description:
              'Explicit nominal pulse width; when > 0 the press/release atomic pair is injected at press time (ADR-0068, min 30ms). 0 keeps the event-driven release',
          },
          bounceUs: {
            type: 'number',
            default: 0,
            min: 0,
            max: 10000,
            unit: 'us',
            description: 'Contact bounce window at the leading edge (0 = clean press)',
          },
          bounceCount: { type: 'number', default: 6, min: 0, max: 64 },
          bounceModel: {
            type: 'string',
            default: 'deterministic_train',
            enum: ['deterministic_train', 'stochastic_fretting'],
            description: 'Contact bounce dynamics model',
          },
          chatterDurationRangeUs: {
            type: 'array',
            description: '[minUs, maxUs] range for non-symmetric chatter duration',
          },
          contactResistanceRangeOhm: {
            type: 'array',
            description: '[minOhm, maxOhm] range for dynamic contact resistance',
          },
          pullupResistanceOhm: {
            type: 'number',
            default: 32000,
            description: 'MCU internal weak pull-up resistance in Ohms',
          },
          seed: {
            type: 'number',
            description: 'Deterministic PRNG seed for chatter generation',
          },
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
  declare protected ctx?: IPluginContext<any>;

  readonly manifest = buttonManifest;
  static readonly manifest = buttonManifest;

  /** ADR-0068: minimum effective pulse width that survives the 20 ms firmware debounce. */
  private static readonly MIN_PRESS_US = 30_000n;
  private static readonly DEFAULT_BOUNCE_COUNT = 6;

  private _pressedState = false;
  private _activeLow = true;
  private _signalPinName = '1.l';
  private _waveGeneration = 0;
  private _pressStartUs = 0n;
  private _scheduledReleaseUs = 0n;

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

  override onReset(): void {
    // Cancel any pending atomic pair so a reset cannot leave a dangling release.
    this._waveGeneration++;
    const ctx: any = this.ctx;
    if (typeof ctx?.cancelWaveform === 'function') {
      ctx.cancelWaveform(this._signalPinName, this._waveGeneration - 1);
    }
    this._pressedState = false;
    this._pressStartUs = 0n;
    this._scheduledReleaseUs = 0n;
  }

  _pressed(arg?: boolean | ButtonPressOptions): void {
    const obj = typeof arg === 'object' && arg !== null ? arg : null;
    const pressed = obj ? Boolean(obj.pressed) : Boolean(arg ?? true);
    this._setState(pressed, {
      pressDurationUs: toBigUs(obj?.pressDurationUs),
      bounceUs: toBigUs(obj?.bounceUs),
      bounceCount:
        typeof obj?.bounceCount === 'number'
          ? obj.bounceCount
          : ButtonPlugin.DEFAULT_BOUNCE_COUNT,
      bounceModel: obj?.bounceModel,
      chatterDurationRangeUs:
        Array.isArray(obj?.chatterDurationRangeUs) && obj.chatterDurationRangeUs.length >= 2
          ? [Number(obj.chatterDurationRangeUs[0]), Number(obj.chatterDurationRangeUs[1])]
          : undefined,
      contactResistanceRangeOhm:
        Array.isArray(obj?.contactResistanceRangeOhm) && obj.contactResistanceRangeOhm.length >= 2
          ? [Number(obj.contactResistanceRangeOhm[0]), Number(obj.contactResistanceRangeOhm[1])]
          : undefined,
      pullupResistanceOhm:
        typeof obj?.pullupResistanceOhm === 'number' ? obj.pullupResistanceOhm : 32000,
      seed: typeof obj?.seed === 'number' ? obj.seed : undefined,
    });
  }

  _press(): void {
    this._pressed(true);
  }

  _release(): void {
    this._pressed(false);
  }

  private _setState(
    pressed: boolean,
    options: {
      pressDurationUs?: bigint;
      bounceUs?: bigint;
      bounceCount: number;
      bounceModel?: BounceModel;
      chatterDurationRangeUs?: [number, number];
      contactResistanceRangeOhm?: [number, number];
      pullupResistanceOhm: number;
      seed?: number;
    },
  ): void {
    this._pressedState = pressed;
    this.ctx?.publish('pressed', pressed);

    const idleLogical = this._activeLow;
    const pressedLogical = !idleLogical;
    const pinLevel = pressed ? pressedLogical : idleLogical;
    const idleLevel = idleLogical ? 1 : 0;
    const pressLevel = pressedLogical ? 1 : 0;

    const ctx: any = this.ctx;
    const canInject =
      ctx &&
      typeof ctx.injectWaveform === 'function' &&
      typeof ctx.nowUs === 'function' &&
      ctx.accuracyMode !== 'behavioral';

    if (!canInject) {
      ctx?.writePin?.(this._signalPinName, pinLevel);
      return;
    }

    if (pressed) {
      const nowUs: bigint = ctx.nowUs();
      const bounceUs = options.bounceUs && options.bounceUs > 0n ? options.bounceUs : 0n;
      const pressDurationUs = options.pressDurationUs;
      this._pressStartUs = nowUs;
      this._waveGeneration++;

      if (options.bounceModel === 'stochastic_fretting') {
        const seed = options.seed ?? Number((nowUs ^ 0x5a5a5a5an) & 0xffffffffn);
        const prng = createMulberry32(seed);
        const makeRange = options.chatterDurationRangeUs ?? [2000, 8000];
        const chatterUs = BigInt(
          Math.max(500, Math.floor(makeRange[0] + prng() * (makeRange[1] - makeRange[0]))),
        );
        const edges = buildStochasticFrettingEdges(
          nowUs,
          chatterUs,
          prng,
          idleLevel,
          pressLevel,
          true,
          options.contactResistanceRangeOhm,
          options.pullupResistanceOhm,
        );
        if (pressDurationUs !== undefined) {
          this._scheduledReleaseUs = nowUs + chatterUs + pressDurationUs;
          const breakRange = options.chatterDurationRangeUs ?? [5000, 18000];
          const releaseChatterUs = BigInt(
            Math.max(500, Math.floor(breakRange[0] + prng() * (breakRange[1] - breakRange[0]))),
          );
          const releaseEdges = buildStochasticFrettingEdges(
            this._scheduledReleaseUs,
            releaseChatterUs,
            prng,
            idleLevel,
            pressLevel,
            false,
            options.contactResistanceRangeOhm,
            options.pullupResistanceOhm,
          );
          edges.push(...releaseEdges);
        } else {
          this._scheduledReleaseUs = 0n;
        }
        ctx.injectWaveform(this._signalPinName, {
          edges,
          generation: this._waveGeneration,
        });
      } else if (pressDurationUs !== undefined || bounceUs > 0n) {
        // Explicit pulse: one atomic press/release pair (optionally with a
        // deterministic chatter train inside the leading edge).
        const edges = buildPressEdges(
          nowUs,
          bounceUs,
          options.bounceCount,
          idleLevel,
          pressLevel,
        );
        this._scheduledReleaseUs = nowUs + bounceUs + (pressDurationUs ?? 0n);
        edges.push({ tUs: this._scheduledReleaseUs, level: idleLevel });
        ctx.injectWaveform(this._signalPinName, {
          edges,
          generation: this._waveGeneration,
        });
      } else {
        // Event-driven hold: inject the press edge only; the release event
        // arrives later and is clamped to the 30 ms debounce floor.
        this._scheduledReleaseUs = 0n;
        ctx.injectWaveform(this._signalPinName, {
          edges: [{ tUs: nowUs, level: pressLevel }],
          generation: this._waveGeneration,
        });
      }
    } else {
      // Release: re-open the atomic pair early when needed, or deliver the
      // event-driven release. Never shorter than the 30 ms debounce floor.
      const minReleaseUs = this._pressStartUs + ButtonPlugin.MIN_PRESS_US;
      const releaseUs = ctx.nowUs() > minReleaseUs ? ctx.nowUs() : minReleaseUs;
      if (this._scheduledReleaseUs === 0n || releaseUs < this._scheduledReleaseUs) {
        this._waveGeneration++;
        if (options.bounceModel === 'stochastic_fretting') {
          const seed = options.seed ?? Number((releaseUs ^ 0xa5a5a5a5n) & 0xffffffffn);
          const prng = createMulberry32(seed);
          const breakRange = options.chatterDurationRangeUs ?? [5000, 18000];
          const releaseChatterUs = BigInt(
            Math.max(500, Math.floor(breakRange[0] + prng() * (breakRange[1] - breakRange[0]))),
          );
          const edges = buildStochasticFrettingEdges(
            releaseUs,
            releaseChatterUs,
            prng,
            idleLevel,
            pressLevel,
            false,
            options.contactResistanceRangeOhm,
            options.pullupResistanceOhm,
          );
          ctx.injectWaveform(this._signalPinName, {
            edges,
            generation: this._waveGeneration,
          });
        } else {
          ctx.injectWaveform(this._signalPinName, {
            edges: [{ tUs: releaseUs, level: idleLevel }],
            generation: this._waveGeneration,
          });
        }
      }
    }
  }
}

/**
 * Mulberry32 32-bit deterministic pseudo-random number generator.
 * Yields uniform pseudo-random floats in [0, 1).
 */
export function createMulberry32(seed: number): () => number {
  let s = seed >>> 0;
  return () => {
    s = (s + 0x6d2b79f5) | 0;
    let t = Math.imul(s ^ (s >>> 15), 1 | s);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/**
 * Physical contact bounce & fretting dynamics waveform builder.
 * Generates asymmetric micro-chatter pulses with dynamic contact resistance
 * and Schmitt trigger hysteresis (VIL <= 0.3 * VDD, VIH >= 0.7 * VDD).
 */
export function buildStochasticFrettingEdges(
  startUs: bigint,
  chatterDurationUs: bigint,
  prng: () => number,
  idleLevel: number,
  pressLevel: number,
  isPress: boolean,
  contactResistanceRange: [number, number] = [500, 5000],
  pullupResistanceOhm: number = 32000,
): Array<{ tUs: bigint; level: number }> {
  const edges: Array<{ tUs: bigint; level: number }> = [];
  const targetLevel = isPress ? pressLevel : idleLevel;
  const initialLevel = isPress ? idleLevel : pressLevel;

  if (chatterDurationUs <= 0n) {
    edges.push({ tUs: startUs, level: targetLevel });
    return edges;
  }

  // Generate 6 to 14 micro-bounces
  const numBounces = 6 + Math.floor(prng() * 9);
  const rawWeights: number[] = [];
  for (let i = 0; i < numBounces; i++) {
    rawWeights.push(0.2 + prng() * 1.8);
  }
  const sumWeight = rawWeights.reduce((a, b) => a + b, 0);

  let currentLevel = initialLevel;
  let accumulatedUs = 0n;
  const totalDuration = Number(chatterDurationUs);

  for (let i = 0; i < numBounces - 1; i++) {
    const fraction = rawWeights[i] / sumWeight;
    const deltaUs = BigInt(Math.max(20, Math.floor(fraction * totalDuration)));
    accumulatedUs += deltaUs;
    if (accumulatedUs >= chatterDurationUs) break;

    const tUs = startUs + accumulatedUs;

    // Alternating make and break phases with stochastic fretting resistance
    const isContactMake = i % 2 === (isPress ? 0 : 1);
    let nextLevel = currentLevel;

    if (isContactMake) {
      const [rMin, rMax] = contactResistanceRange;
      const rContact = rMin + prng() * (rMax - rMin);
      const vPinRatio = rContact / (rContact + pullupResistanceOhm);
      // Schmitt trigger: VIL threshold = 0.3 * VDD, VIH threshold = 0.7 * VDD
      if (vPinRatio <= 0.3) {
        nextLevel = pressLevel;
      } else if (vPinRatio >= 0.7) {
        nextLevel = idleLevel;
      }
    } else {
      nextLevel = idleLevel;
    }

    if (nextLevel !== currentLevel) {
      edges.push({ tUs, level: nextLevel });
      currentLevel = nextLevel;
    }
  }

  // Final edge: settle firmly at target level at the end of chatter window
  const settleUs = startUs + chatterDurationUs;
  if (currentLevel !== targetLevel || edges.length === 0) {
    edges.push({ tUs: settleUs, level: targetLevel });
  }

  return edges;
}

/**
 * Deterministic contact-bounce glitch sequence inside the atomic press pair
 * (ADR-0068): idle -> press -> idle -> ... -> press, settling at `bounceUs`.
 * No RNG, no wall clock: reproducible across hosts.
 */
export function buildPressEdges(
  startUs: bigint,
  bounceUs: bigint,
  count: number,
  idleLevel: number,
  pressLevel: number,
): Array<{ tUs: bigint; level: number }> {
  const edges: Array<{ tUs: bigint; level: number }> = [];
  if (bounceUs <= 0n) {
    edges.push({ tUs: startUs, level: pressLevel });
    return edges;
  }
  const n = Math.max(1, Math.min(64, Math.floor(count)));
  for (let i = 1; i <= n; i++) {
    const tUs = startUs + (bounceUs * BigInt(i)) / BigInt(n + 1);
    const level = i % 2 === 1 ? idleLevel : pressLevel;
    edges.push({ tUs, level });
  }
  edges.push({ tUs: startUs + bounceUs, level: pressLevel });
  return edges;
}

function toBigUs(value: number | string | undefined): bigint | undefined {
  if (value === undefined) return undefined;
  if (typeof value === 'number') {
    return Number.isFinite(value) && value > 0 ? BigInt(Math.floor(value)) : undefined;
  }
  try {
    const parsed = BigInt(value);
    return parsed > 0n ? parsed : undefined;
  } catch {
    return undefined;
  }
}

export default {
  manifest: buttonManifest,
  manifestFactory: buttonManifestFactory,
  PluginClass: ButtonPlugin,
};
