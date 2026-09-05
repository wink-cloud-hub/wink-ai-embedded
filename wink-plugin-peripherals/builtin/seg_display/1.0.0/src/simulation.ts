import {
  BaseSimulationPlugin,
  LogicStates,
  normalizeManifest,
  resolvePluginIdentity,
  createThrottlePublish,
  type LogicState,
  type PeripheralManifest,
  type PeripheralManifestPinInput,
  type ManifestFactory,
  type PluginContext,
  type ThrottlePublishHandle,
} from '@wink-ai/unisim';

import {
  SEG_VARIANT_DIGITS,
  resolveSegVariant,
  type SegVariantKey,
} from './variants';
import { decodeSegMask } from './seg-font';

export interface SegDisplayProps {
  variant: SegVariantKey;
  appearanceId: string;
  segActiveLevel: 'high' | 'low';
  digitActiveLevel: 'high' | 'low';
  commonAnode: boolean;
  color: string;
  glow: boolean;
  brightness: number;
  label: string;
  flip: boolean;
}

export interface SegDisplayState {
  bright: Uint8Array | string;
  segMask: string;
  text: string;
  scanHz: number;
  activeDigits: number;
}

const identity = resolvePluginIdentity(import.meta.url, 'seg_display', '1.0.0', 'display');

const SEGMENT_NAMES = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'DP'] as const;

export function createSegDisplayPins(variantName: SegVariantKey): PeripheralManifestPinInput[] {
  const nDigits = SEG_VARIANT_DIGITS[variantName] ?? 8;
  const pins: PeripheralManifestPinInput[] = [];

  for (const seg of SEGMENT_NAMES) {
    pins.push({
      name: seg,
      pinType: 'digital_in',
      role: `seg_${seg.toLowerCase()}`,
      aliases: [seg.toLowerCase(), `seg_${seg.toLowerCase()}`],
      required: false,
    });
  }

  for (let d = 0; d < nDigits; d++) {
    const num = d + 1;
    pins.push({
      name: `DIG${num}`,
      pinType: 'digital_in',
      role: `dig_${num}`,
      aliases: [`dig${num}`, `digit${num}`, `com${d}`],
      required: false,
    });
  }

  return pins;
}

export function createSegDisplayManifest(variantName: SegVariantKey = 'direct_gpio_8d'): PeripheralManifest {
  const validVariant = resolveSegVariant(variantName);
  const pins = createSegDisplayPins(validVariant);

  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: `${SEG_VARIANT_DIGITS[validVariant]}-Digit 7-Segment Display`,
    description: 'Multiplexed 7-segment digital LED display with duty-cycle brightness simulation',
    timingModel: 'event-driven',
    pins,
    properties: {
      variant: {
        type: 'string',
        default: validVariant,
        enum: ['direct_gpio_8d', 'direct_gpio_4d', 'direct_gpio_2d', 'direct_gpio_1d'],
      },
      appearanceId: {
        type: 'string',
        default: `seg_display_${SEG_VARIANT_DIGITS[validVariant]}`,
      },
      segActiveLevel: {
        type: 'string',
        default: 'high',
        enum: ['high', 'low'],
      },
      digitActiveLevel: {
        type: 'string',
        default: 'low',
        enum: ['high', 'low'],
      },
      commonAnode: {
        type: 'boolean',
        default: false,
      },
      color: {
        type: 'string',
        default: 'red',
        enum: ['red', 'green', 'blue', 'yellow', 'white', 'orange', 'purple'],
      },
      glow: {
        type: 'boolean',
        default: true,
      },
      brightness: {
        type: 'number',
        default: 1.0,
        min: 0.0,
        max: 1.0,
      },
      label: {
        type: 'string',
        default: '',
      },
      flip: {
        type: 'boolean',
        default: false,
      },
    },
    stateChannels: {
      bright: { type: 'string', default: '' },
      segMask: { type: 'string', default: '[]' },
      text: { type: 'string', default: '' },
      scanHz: { type: 'number', default: 0 },
      activeDigits: { type: 'number', default: 0 },
    },
    events: {},
  });
}

export const segDisplayManifest: PeripheralManifest = createSegDisplayManifest('direct_gpio_8d');

export const segDisplayManifestFactory: ManifestFactory = (variant: string) =>
  createSegDisplayManifest(resolveSegVariant(variant));

// Simulation physics constants
export const DECAY_TAU_US = 80_000n; // 80ms persistence of vision (POV) equivalent to human eye retinal integration
export const CHARGE_RATE = 255.0 / 2000.0; // 255 / 2000us = 0.1275 / us
export const LOGIC_THRESHOLD = 50; // Calibrated for multiplexed displays, rejects <40 ghosting glitches
export const GHOST_MAX_BRIGHT = 40;
export const PUBLISH_INTERVAL_US = 16_000n; // ~60 fps throttle interval (aligns with 60Hz display refresh)
export const MAX_DT_US = 100_000n; // Max integration step clamp

export class SegDisplayPlugin extends BaseSimulationPlugin<SegDisplayState, SegDisplayProps> {
  readonly manifest = segDisplayManifest;
  static readonly manifest = segDisplayManifest;

  private nDigits = 8;
  private segPinOf = new Map<number, number>(); // MCU pin -> seg index 0..7
  private digPinOf = new Map<number, number>(); // MCU pin -> digit index 0..nDigits-1
  private segLevel = new Uint8Array(8); // LogicStates per segment line (初值 HI_Z)
  private digLevel = new Uint8Array(8); // LogicStates per digit line (初值 HI_Z)
  private bright = new Uint8Array(8 * 8); // Brightness per (d * 8 + s) [0..255]
  private segMask = new Uint8Array(8); // Bitmask per digit [0..255]

  private lastEdgeUs = 0n;
  private tailGen = 0;
  private tailPending = false;
  private staticDrive = false;
  private segActiveHigh = true;
  private digActiveHigh = false;

  private lastDig0ActiveUs = 0n;
  private scanHz = 0;
  private maxActiveDigitsInWindow = 0;
  private lastConflictWarnUs = 0n;
  private rawProperties?: Record<string, unknown>;

  private throttle: ThrottlePublishHandle = createThrottlePublish({
    ctx: () => this.ctx,
    intervalUs: PUBLISH_INTERVAL_US,
    publish: (nowUs: bigint) => this.publishFrame(nowUs),
  });

  override onBind(
    ctx: PluginContext<any>,
    pinMapping: Record<string, number>,
    properties: Record<string, unknown>,
  ): void {
    this.rawProperties = properties;
    super.onBind(ctx, pinMapping, properties);
  }

  protected override onBound(
    _ctx: PluginContext<any>,
    pinMapping: Record<string, number>,
    props: SegDisplayProps,
  ): Partial<SegDisplayState> {
    const variantKey = resolveSegVariant(props.variant);
    this.nDigits = SEG_VARIANT_DIGITS[variantKey] ?? 8;

    // Resolve polarities: explicit overrides take precedence over commonAnode preset
    const raw = this.rawProperties ?? {};
    if (raw.segActiveLevel !== undefined) {
      this.segActiveHigh = raw.segActiveLevel === 'high';
    } else if (raw.commonAnode !== undefined) {
      this.segActiveHigh = !raw.commonAnode;
    } else {
      this.segActiveHigh = props.segActiveLevel === 'high';
    }

    if (raw.digitActiveLevel !== undefined) {
      this.digActiveHigh = raw.digitActiveLevel === 'high';
    } else if (raw.commonAnode !== undefined) {
      this.digActiveHigh = Boolean(raw.commonAnode);
    } else {
      this.digActiveHigh = props.digitActiveLevel === 'high';
    }

    // Allocate & initialize buffers
    this.segLevel.fill(LogicStates.HI_Z);
    this.digLevel = new Uint8Array(this.nDigits);
    this.digLevel.fill(LogicStates.HI_Z);
    this.bright = new Uint8Array(this.nDigits * 8);
    this.segMask = new Uint8Array(this.nDigits);

    // Build reverse pin lookup tables
    this.segPinOf.clear();
    this.digPinOf.clear();

    for (let s = 0; s < SEGMENT_NAMES.length; s++) {
      const segName = SEGMENT_NAMES[s];
      const mcu =
        pinMapping[segName] ??
        pinMapping[segName.toLowerCase()] ??
        pinMapping[`seg_${segName.toLowerCase()}`];
      if (mcu !== undefined) {
        this.segPinOf.set(mcu, s);
      }
    }

    for (let d = 0; d < this.nDigits; d++) {
      const digName = `DIG${d + 1}`;
      const mcu =
        pinMapping[digName] ??
        pinMapping[digName.toLowerCase()] ??
        pinMapping[`digit${d + 1}`] ??
        pinMapping[`dig_${d + 1}`];
      if (mcu !== undefined) {
        this.digPinOf.set(mcu, d);
      }
    }

    // 1-digit variant static drive when DIG1 pin is unmapped
    this.staticDrive = this.nDigits === 1 && this.digPinOf.size === 0;

    const initialNow = this.getNowUs();
    this.lastEdgeUs = initialNow;
    this.tailGen = 0;
    this.tailPending = false;
    this.scanHz = 0;
    this.maxActiveDigitsInWindow = 0;
    this.lastConflictWarnUs = 0n;

    return {
      bright: this.bright,
      segMask: JSON.stringify(Array.from(this.segMask)),
      text: ''.padStart(this.nDigits, ' '),
      scanHz: 0,
      activeDigits: 0,
    };
  }

  private getNowUs(): bigint {
    const ctx = this.ctx as any;
    if (typeof ctx?.nowUs === 'function') {
      return ctx.nowUs();
    }
    if (typeof ctx?.system?.time?.nowUs === 'function') {
      return ctx.system.time.nowUs();
    }
    return 0n;
  }

  private isDigitActive(d: number): boolean {
    if (this.staticDrive && d === 0) return true;
    const lvl = this.digLevel[d];
    if (lvl === LogicStates.HI_Z || lvl === LogicStates.CONFLICT) return false;
    return this.digActiveHigh ? lvl === LogicStates.HIGH : lvl === LogicStates.LOW;
  }

  private isSegActive(s: number): boolean {
    const lvl = this.segLevel[s];
    if (lvl === LogicStates.HI_Z || lvl === LogicStates.CONFLICT) return false;
    return this.segActiveHigh ? lvl === LogicStates.HIGH : lvl === LogicStates.LOW;
  }

  private integrateTo(nowUs: bigint): void {
    if (nowUs <= this.lastEdgeUs) return;

    let dtBig = nowUs - this.lastEdgeUs;
    if (dtBig > MAX_DT_US) {
      dtBig = MAX_DT_US;
    }
    const dtUs = Number(dtBig);
    if (dtUs <= 0) {
      this.lastEdgeUs = nowUs;
      return;
    }

    const decayFactor = Math.exp(-dtUs / Number(DECAY_TAU_US));
    const chargeDelta = dtUs * CHARGE_RATE;

    let currentActiveCount = 0;
    for (let d = 0; d < this.nDigits; d++) {
      if (this.isDigitActive(d)) {
        currentActiveCount++;
      }
    }
    if (currentActiveCount > this.maxActiveDigitsInWindow) {
      this.maxActiveDigitsInWindow = currentActiveCount;
    }

    if (currentActiveCount > 1 && nowUs - this.lastConflictWarnUs >= 100_000n) {
      this.lastConflictWarnUs = nowUs;
      (this.ctx as any)?.system?.log?.warn?.(
        `[seg_display] multiple digits driven simultaneously (${currentActiveCount})`,
      );
    }

    for (let d = 0; d < this.nDigits; d++) {
      const digitActive = this.isDigitActive(d);
      const baseIdx = d * 8;
      for (let s = 0; s < 8; s++) {
        const segActive = this.isSegActive(s);
        const lit = digitActive && segActive;
        const k = baseIdx + s;
        let b = this.bright[k];
        if (lit) {
          b = Math.min(255, b + chargeDelta);
        }
        b = Math.max(0, b * decayFactor);
        this.bright[k] = Math.round(b);
      }
    }

    this.lastEdgeUs = nowUs;
  }

  override onPinChange(eventOrPin: any, level?: LogicState, atUs?: bigint): void {
    const pin = typeof eventOrPin === 'object' && eventOrPin !== null ? eventOrPin.pin : eventOrPin;
    const state = typeof eventOrPin === 'object' && eventOrPin !== null ? eventOrPin.state : level;
    const timeVal =
      typeof eventOrPin === 'object' && eventOrPin !== null
        ? (eventOrPin.atUs ?? eventOrPin.tUs)
        : atUs;

    const pinNum = typeof pin === 'number' ? pin : parseInt(String(pin), 10);
    const nowUs = timeVal !== undefined ? BigInt(timeVal) : this.getNowUs();

    // 1. Integrate elapsed virtual time using existing pin states
    this.integrateTo(nowUs);

    // 2. Check if changed pin is segment or digit line
    let changed = false;
    const segIndex = this.segPinOf.get(pinNum);
    if (segIndex !== undefined) {
      if (this.segLevel[segIndex] !== state) {
        this.segLevel[segIndex] = state ?? LogicStates.HI_Z;
        changed = true;
      }
    }

    const digIndex = this.digPinOf.get(pinNum);
    if (digIndex !== undefined) {
      const wasActive = this.isDigitActive(digIndex);
      if (this.digLevel[digIndex] !== state) {
        this.digLevel[digIndex] = state ?? LogicStates.HI_Z;
        changed = true;
      }
      const isActiveNow = this.isDigitActive(digIndex);

      // Track frame scan frequency on rising active edge of DIG1
      if (digIndex === 0 && isActiveNow && !wasActive) {
        if (this.lastDig0ActiveUs > 0n && nowUs > this.lastDig0ActiveUs) {
          const periodUs = nowUs - this.lastDig0ActiveUs;
          if (periodUs > 0n) {
            this.scanHz = Math.round(1_000_000 / Number(periodUs));
          }
        }
        this.lastDig0ActiveUs = nowUs;
      }
    }

    if (state === LogicStates.CONFLICT && nowUs - this.lastConflictWarnUs >= 100_000n) {
      this.lastConflictWarnUs = nowUs;
      (this.ctx as any)?.system?.log?.warn?.(`[seg_display] bus conflict on MCU pin ${pinNum}`);
    }

    if (changed) {
      this.throttle.request();
    }
  }

  private publishFrame(nowUs: bigint = this.getNowUs()): void {
    this.integrateTo(nowUs);

    let textStr = '';
    for (let d = 0; d < this.nDigits; d++) {
      let mask = 0;
      const baseIdx = d * 8;
      for (let s = 0; s < 8; s++) {
        if (this.bright[baseIdx + s] >= LOGIC_THRESHOLD) {
          mask |= 1 << s;
        }
      }
      this.segMask[d] = mask;
      textStr += decodeSegMask(mask);
    }

    if (this.ctx) {
      this.ctx.publish('bright', this.bright);
      this.ctx.publish('segMask', JSON.stringify(Array.from(this.segMask)));
      this.ctx.publish('text', textStr);
      this.ctx.publish('scanHz', this.scanHz);
      this.ctx.publish('activeDigits', this.maxActiveDigitsInWindow);
    }

    this.maxActiveDigitsInWindow = 0;

    // Check residual brightness for tail decay chaining (until full darkness)
    let hasResidual = false;
    for (let i = 0; i < this.bright.length; i++) {
      if (this.bright[i] > 0) {
        hasResidual = true;
        break;
      }
    }

    if (hasResidual && !this.tailPending && this.ctx) {
      this.scheduleTail(nowUs);
    }
  }

  private scheduleTail(scheduledAtUs: bigint): void {
    this.tailPending = true;
    const gen = ++this.tailGen;
    const ctx = this.ctx as any;

    if (typeof ctx?.deferUs === 'function') {
      ctx.deferUs(PUBLISH_INTERVAL_US, () => {
        if (gen !== this.tailGen) return;
        this.tailPending = false;
        const liveNow = this.getNowUs();
        const effectiveNow = liveNow > scheduledAtUs ? liveNow : scheduledAtUs + PUBLISH_INTERVAL_US;
        this.publishFrame(effectiveNow);
      });
    } else {
      this.tailPending = false;
    }
  }

  override onReset(): void {
    this.segLevel.fill(LogicStates.HI_Z);
    this.digLevel.fill(LogicStates.HI_Z);
    this.bright.fill(0);
    this.segMask.fill(0);
    this.throttle.reset();
    this.tailGen++;
    this.tailPending = false;
    this.scanHz = 0;
    this.maxActiveDigitsInWindow = 0;
    this.lastEdgeUs = this.getNowUs();
    this.publishFrame(this.lastEdgeUs);
  }

  override serializeState(): Record<string, unknown> {
    return {
      bright: Array.from(this.bright),
      segLevel: Array.from(this.segLevel),
      digLevel: Array.from(this.digLevel),
      segMask: Array.from(this.segMask),
      staticDrive: this.staticDrive,
      segActiveHigh: this.segActiveHigh,
      digActiveHigh: this.digActiveHigh,
    };
  }

  override deserializeState(snapshot: Record<string, unknown>): void {
    if (Array.isArray(snapshot.bright)) {
      this.bright.set(snapshot.bright as number[]);
    }
    if (Array.isArray(snapshot.segLevel)) {
      this.segLevel.set(snapshot.segLevel as number[]);
    }
    if (Array.isArray(snapshot.digLevel)) {
      this.digLevel.set(snapshot.digLevel as number[]);
    }
    if (Array.isArray(snapshot.segMask)) {
      this.segMask.set(snapshot.segMask as number[]);
    }
    if (typeof snapshot.staticDrive === 'boolean') {
      this.staticDrive = snapshot.staticDrive;
    }
    if (typeof snapshot.segActiveHigh === 'boolean') {
      this.segActiveHigh = snapshot.segActiveHigh;
    }
    if (typeof snapshot.digActiveHigh === 'boolean') {
      this.digActiveHigh = snapshot.digActiveHigh;
    }

    // Reset lastEdgeUs to current time to avoid large jump decay on restore
    this.lastEdgeUs = this.getNowUs();
  }

  override onPropertyChange(key: string, _oldVal: unknown, newVal: unknown): void {
    if (key === 'variant') {
      (this.ctx as any)?.system?.log?.warn?.(
        `[seg_display] runtime variant change is not supported (pin sets are static)`,
      );
      return;
    }
    if (key === 'segActiveLevel') {
      this.segActiveHigh = newVal === 'high';
    } else if (key === 'digitActiveLevel') {
      this.digActiveHigh = newVal === 'high';
    } else if (key === 'commonAnode') {
      const ca = Boolean(newVal);
      this.segActiveHigh = !ca;
      this.digActiveHigh = ca;
    }
  }

  override async onPowerOn(_ctx: PluginContext<any>): Promise<void> {
    this.onReset();
  }

  override onPowerOff(): void {
    this.bright.fill(0);
    this.segMask.fill(0);
    this.publishFrame(this.getNowUs());
  }

  override onDestroy(): void {
    this.throttle.reset();
    this.tailGen++;
    this.tailPending = false;
    super.onDestroy();
  }
}

export default {
  manifest: segDisplayManifest,
  manifestFactory: segDisplayManifestFactory,
  PluginClass: SegDisplayPlugin,
};
