import {
  normalizeManifest,
  resolvePluginIdentity,
  BaseSimulationPlugin,
  normalizeVariantKey,
  LogicStates,
  type PluginContext,
  type PeripheralManifest,
  type PeripheralManifestPinInput,
  type ManifestFactory,
  type LogicState,
} from '@wink-ai/unisim';
import { distanceCmToEchoUs } from './physics/distance-echo-us';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(import.meta.url, 'ultrasonic', '1.0.0', 'sensor');

export type UltrasonicProps = {
  maxDistanceCm: number;
  minDistanceCm: number;
  speedOfSoundMps: number;
  useRmt: boolean;
  autoPollMs: number;
};

export type UltrasonicState = {
  distanceCm: number;
  echoUs: number;
};

export type UltrasonicVariant = 'default';

export const ULTRASONIC_PIN_VARIANTS: Record<
  UltrasonicVariant,
  { displayName: string; pins: PeripheralManifestPinInput[] }
> = {
  default: {
    displayName: 'HC-SR04 Ultrasonic Sensor',
    pins: [
      { name: 'TRIG', pinType: 'digital_in', required: true },
      { name: 'ECHO', pinType: 'digital_out', required: true },
      { name: 'VCC', pinType: 'vcc', required: false },
      { name: 'GND', pinType: 'gnd', required: false },
    ],
  },
};

function resolveUltrasonicVariant(raw?: string): UltrasonicVariant {
  const key = normalizeVariantKey(raw);
  return key && key in ULTRASONIC_PIN_VARIANTS ? (key as UltrasonicVariant) : 'default';
}

export function createUltrasonicManifest(
  variantName: UltrasonicVariant = 'default',
): PeripheralManifest {
  const row = ULTRASONIC_PIN_VARIANTS[variantName] ?? ULTRASONIC_PIN_VARIANTS.default;
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: row.displayName,
    description: '4-pin ultrasonic distance sensor (2cm-400cm range, ~1cm resolution)',
    timingModel: 'event-driven',
    pins: row.pins,
    properties: {
      variant: { type: 'string', default: 'default' },
      backend: { type: 'string', default: 'auto' },
      maxDistanceCm: { type: 'number', default: 400, min: 2, max: 400, unit: 'cm' },
      minDistanceCm: { type: 'number', default: 2, min: 0, max: 10, unit: 'cm' },
      speedOfSoundMps: { type: 'number', default: 343, min: 300, max: 400, unit: 'm/s' },
      useRmt: { type: 'boolean', default: false },
      autoPollMs: { type: 'number', default: 50, min: 0, max: 10000, unit: 'ms' },
    },
    stateChannels: {
      distanceCm: {
        type: 'number',
        default: 100,
        unit: 'cm',
        min: 2,
        max: 400,
        description: 'Measured distance in centimeters',
      },
      echoUs: {
        type: 'number',
        default: 0,
        unit: 'us',
        description: 'Echo pulse width in microseconds',
      },
    },
    events: {
      SET_DISTANCE_CM: {
        description: 'Set the simulated distance for testing',
        params: {
          cm: { type: 'number', required: true, unit: 'cm', min: 2, max: 400, default: 100 },
        },
      },
    },
  });
}

export const ultrasonicManifest: PeripheralManifest = createUltrasonicManifest('default');

export const ultrasonicManifestFactory: ManifestFactory = (variant: string) =>
  createUltrasonicManifest(resolveUltrasonicVariant(variant));

export class UltrasonicPlugin extends BaseSimulationPlugin<UltrasonicState, UltrasonicProps> {
  readonly manifest = ultrasonicManifest;
  static readonly manifest = ultrasonicManifest;

  get type(): string {
    return this.manifest.type;
  }

  private trigState: LogicState = LogicStates.LOW;
  private distanceCm = 100;
  private alive = true;

  protected override onBound(
    _ctx: PluginContext<UltrasonicState>,
    pinMapping: Record<string, number>,
    props: UltrasonicProps,
  ): Partial<UltrasonicState> {
    this.alive = true;
    this.pinMapping = pinMapping;
    const distanceCm = Math.min(100, props.maxDistanceCm);
    this.distanceCm = distanceCm;
    return { distanceCm, echoUs: 0 };
  }

  onDestroy(): void {
    this.alive = false;
    if (this.ctx?.gpio) {
      this.ctx.gpio.releasePin('ECHO');
    } else {
      this.ctx?.releasePin('ECHO');
    }
  }

  private _echoGeneration = 1;

  onPinChange(eventOrPin: any, level?: LogicState, _atUs?: bigint): void {
    if (!this.alive || !this.ctx || !this.pinMapping) return;

    const pin = typeof eventOrPin === 'object' && eventOrPin !== null ? eventOrPin.pin : eventOrPin;
    const state = typeof eventOrPin === 'object' && eventOrPin !== null ? eventOrPin.state : level;
    const pinNum = typeof pin === 'number' ? pin : parseInt(String(pin), 10) || pin;

    const trigPin = this.pinMapping['TRIG'];
    if (pinNum !== trigPin) return;

    const wasLow = this.trigState === LogicStates.LOW;
    const isHigh = state === LogicStates.HIGH;
    this.trigState = state;

    if (wasLow && isHigh) {
      const echoUs = distanceCmToEchoUs(this.distanceCm, this.properties?.speedOfSoundMps ?? 343);
      this.ctx.publish('echoUs', echoUs);

      const echoPin = this.pinMapping['ECHO'];
      if (echoPin === undefined) return;

      // CRITICAL FIX: Directly push ECHO edges into C-side s_pin_events[] via
      // pal_wasm_push_pin_event, bypassing the isWasmExecuting() enqueue check.
      //
      // When this callback fires from within a WASM call frame (i.e. triggered by
      // pal_wasm_trigger_ultrasonic_measurement -> dal_ultrasonic_request_measurement
      // -> pal_gpio_write(TRIG,HIGH) -> JS onPinChange), isWasmExecuting()==true causes
      // injectWaveform() to only ENQUEUE events. But pal_gpio_pulse_in() checks
      // s_pin_events[] synchronously in the same WASM frame before those events are
      // ever flushed, so it always returns WINK_ERR_TIMEOUT with no data.
      //
      // By calling pal_wasm_push_pin_event directly here, the ECHO HIGH+LOW edges are
      // immediately visible to pal_gpio_pulse_in() and it can return the correct pulse_us.
      const exports = this.ctx?.getWasmExports?.();
      const pushFn = exports?.pal_wasm_push_pin_event ?? exports?._pal_wasm_push_pin_event;

      if (typeof pushFn === 'function') {
        try {
          pushFn(echoPin, 0n, 1); // ECHO HIGH at delay=0us (BigInt required for i64 ABI)
          pushFn(echoPin, BigInt(echoUs), 0); // ECHO LOW  at delay=echoUs
        } catch {
          // Fallback if BigInt is not supported by wrapper
          pushFn(echoPin, 0 as any, 1);
          pushFn(echoPin, echoUs as any, 0);
        }
      } else {
        // Fallback for headless or when no WASM is loaded
        const now = (this.ctx as any).nowUs?.() ?? this.ctx.system?.time?.nowUs?.() ?? 0n;
        const waveform = {
          edges: [
            { tUs: now, level: LogicStates.HIGH },
            { tUs: now + BigInt(echoUs), level: LogicStates.LOW },
          ],
          generation: this._echoGeneration++,
        };
        if (typeof (this.ctx as any).injectWaveform === 'function') {
          (this.ctx as any).injectWaveform('ECHO', waveform);
        } else if (this.ctx.gpio) {
          this.ctx.gpio.injectWaveform('ECHO', waveform);
        }
      }
    }
  }

  /**
   * Called when the UI slider changes distance.
   *
   * Only updates the modelled distance + publishes the state channel. The
   * firmware drives its own periodic ultrasonic poll (auto_poll_ms, default
   * 50ms); on that poll it raises TRIG, which reaches this plugin through the
   * pin arbiter (`onPinChange`) and we emit the ECHO pulse from the current
   * `distanceCm`. That is the same path real hardware takes.
   *
   * Do NOT call `pal_wasm_trigger_ultrasonic_measurement` synchronously here.
   * That export performs a blocking measurement (`pal_os_busy_wait_us` → Asyncify
   * unwind) on the JS-called export stack, outside any firmware fiber. The
   * browser worker's simLoop is a *synchronous* sub-step loop with no `await`
   * between steps; before the 10µs sleep can rewind, the next sub-step calls
   * `pal_wasm_advance_virtual_clock` / `pal_wasm_app_tick`, and once firmware's
   * own periodic `us_dist` task is due (~50ms) it runs a second measurement
   * inside the scheduler. The two Asyncify operations collide and overwrite the
   * global `Asyncify.currData`, so the outer trigger's `_asyncify_start_rewind`
   * traps (unreachable) and the engine is left permanently yielded (state=2) —
   * freezing the servo at its last angle. (The headless runner hides this only
   * because it `await Promise.resolve()`s every quantum, letting the rewind
   * complete before re-entering WASM.)
   */
  _distanceCm(cm: number): void {
    this.distanceCm = cm;
    this.ctx?.publish('distanceCm', cm);
  }
}

export default {
  manifest: ultrasonicManifest,
  manifestFactory: ultrasonicManifestFactory,
  PluginClass: UltrasonicPlugin,
};
