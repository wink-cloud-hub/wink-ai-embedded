/**
 * SG90 — **hobby angle-servo** authoring template (Phase 7a / 7c).
 *
 * ┌─ Is this your device? ───────────────────────────────────────────────────┐
 * │ ✅ 标准参考：MG90 / 同类「角度 → 脉宽 → duty」舵机（复制本文件改 ①②③）   │
 * │ ❌ 非本模板：PWM 调光 LED / 蜂鸣占空比 / ESC 油门 / 连续旋转舵机         │
 * │    → 仍可抄「typed props + pwm 脚 resolve + analogWrite」                 │
 * │    → 不要用 angleToPwmDuty（换算公式不同，自写 map → duty）              │
 * │ 数字 GPIO → SimpleGpioPlugin；I2C → I2cPeripheralPlugin                  │
 * └──────────────────────────────────────────────────────────────────────────┘
 *
 * 同构舵机（复制后改这些）:
 *   ① Manifest — type / pins / properties 默认脉宽 / stateChannels / events
 *   ② Props/State 类型 — 与 properties / stateChannels 对齐；
 *      `extends BaseSimulationPlugin<State, Props>`（R8）
 *   ③ 驱动 — `_angle`（或事件映射名）+ `angleToPwmDuty(angle, this.props)`；
 *      pwm 脚 resolve 三行内联；`onDestroy` → `releasePin`（fail-loud）
 *
 * 通常不用改:
 *   - `angleToPwmDuty` / `pwmDutyToAngle`（SSOT: `base/pwm-angle-duty.ts`）
 *   - `observe` / electricalMirror — 平台 preset `observe-presets.ts`
 *     （registry key `servo-sg90`）；本文件勿写 observe / simulation 块
 *   - Base 绑定管线 / Host SET_ANGLE → `_angle` 约定
 *   - Firmware PWM → canvas：实现 `onDutyChange`（只 publish，不写脚）
 *
 * 延期（第二台同构舵机或薄基类再评）: attachPwmPin / SimplePwmServoPlugin
 */
import {
  normalizeManifest,
  resolvePluginIdentity,
  BaseSimulationPlugin,
  defaultRolePinName,
  resolveMappedRolePinName,
  normalizeVariantKey,
  type PluginContext,
  type ManifestFactory,
  type PeripheralManifest,
  type PeripheralManifestPinInput,
} from '@wink-ai/unisim';
import { angleToPwmDuty, pwmDutyToAngle } from './physics/pwm-angle-duty';

declare const __PLUGIN_TYPE__: string | undefined;
declare const __PLUGIN_VERSION__: string | undefined;
declare const __PLUGIN_CATEGORY__: string | undefined;

const identity = resolvePluginIdentity(import.meta.url, 'rc_servo', '1.0.0', 'output');

export type Sg90Props = {
  variant?: string;
  minAngle: number;
  maxAngle: number;
  minPulseMs: number;
  maxPulseMs: number;
  framePeriodMs: number;
  pwmChannel: number;
};

export type Sg90State = {
  angle: number;
  targetAngle: number;
};

/** Single pin topology today; key matches properties.variant default. */
export type RcServoVariant = 'sg90';

export const RC_SERVO_PIN_VARIANTS: Record<
  RcServoVariant,
  { displayName: string; pins: PeripheralManifestPinInput[] }
> = {
  sg90: {
    displayName: 'RC Servo Motor',
    pins: [
      { name: 'PWM', pinType: 'pwm', required: true },
      { name: 'VCC', pinType: 'vcc', voltage: '5V', required: false },
      { name: 'GND', pinType: 'gnd', required: false },
    ],
  },
};

function resolveRcServoVariant(raw?: string): RcServoVariant {
  const key = normalizeVariantKey(raw);
  return key && key in RC_SERVO_PIN_VARIANTS ? (key as RcServoVariant) : 'sg90';
}

export function createRcServoManifest(variantName: RcServoVariant = 'sg90'): PeripheralManifest {
  const row = RC_SERVO_PIN_VARIANTS[variantName] ?? RC_SERVO_PIN_VARIANTS.sg90;
  return normalizeManifest({
    type: identity.type,
    version: identity.version,
    category: identity.category,
    displayName: row.displayName,
    description: 'RC Servo Motor (0-180 degrees)',
    timingModel: 'event-driven',
    pins: row.pins,
    properties: {
      variant: { type: 'string', default: variantName },
      minAngle: { type: 'number', default: 0, min: 0, max: 180 },
      maxAngle: { type: 'number', default: 180, min: 0, max: 180 },
      minPulseMs: { type: 'number', default: 0.5, min: 0.3, max: 1.0 },
      maxPulseMs: { type: 'number', default: 2.5, min: 2.0, max: 3.0 },
      framePeriodMs: { type: 'number', default: 20, min: 1, max: 100, unit: 'ms' },
      /** C/firmware PWM channel id from wink-app.json */
      pwmChannel: { type: 'number', default: 0, min: 0, max: 15 },
    },
    stateChannels: {
      angle: { type: 'number', default: 90, unit: 'degrees', description: 'Current servo angle' },
      targetAngle: {
        type: 'number',
        default: 90,
        unit: 'degrees',
        show: false,
        description: 'Target angle (smoothing)',
      },
    },
    events: {
      SET_ANGLE: {
        description: 'Set servo target angle',
        params: {
          angle: { type: 'number', required: true, unit: 'degrees' },
        },
      },
    },
  });
}

/** pluginEntry = filename `rc_servo` (auto-discovered by manifest-index). */
export const rcServoManifest: PeripheralManifest = createRcServoManifest('sg90');

export const rcServoManifestFactory: ManifestFactory = (variant: string) =>
  createRcServoManifest(resolveRcServoVariant(variant));

export class RcServoPlugin extends BaseSimulationPlugin<Sg90State, Sg90Props> {
  readonly manifest = rcServoManifest;
  static readonly manifest = rcServoManifest;

  get type(): string {
    return this.manifest.type;
  }

  private angle = 90;
  private pwmPinName = 'PWM';

  protected onBound(
    _ctx: PluginContext<Sg90State>,
    pinMapping: Record<string, number>,
    _props: Sg90Props,
  ): void {
    this.pwmPinName =
      resolveMappedRolePinName(this.manifest, 'pwm', pinMapping) ??
      defaultRolePinName(this.manifest, 'pwm') ??
      'PWM';
    this.writeDuty();
  }

  private getResolvedProps(): {
    minAngle: number;
    maxAngle: number;
    minPulseMs: number;
    maxPulseMs: number;
    framePeriodMs: number;
    pwmChannel: number;
  } {
    const raw = (this.properties || {}) as Record<string, unknown>;
    return {
      minAngle: (this.properties?.minAngle ?? raw.min_angle ?? 0) as number,
      maxAngle: (this.properties?.maxAngle ?? raw.max_angle ?? 180) as number,
      minPulseMs: (this.properties?.minPulseMs ?? raw.min_pulse_ms ?? 0.5) as number,
      maxPulseMs: (this.properties?.maxPulseMs ?? raw.max_pulse_ms ?? 2.5) as number,
      framePeriodMs: (this.properties?.framePeriodMs ?? raw.frame_period_ms ?? 20) as number,
      pwmChannel: (this.properties?.pwmChannel ?? raw.pwm_channel ?? 0) as number,
    };
  }

  private writeDuty(): void {
    const normVal = angleToPwmDuty(this.angle, this.getResolvedProps());
    if (this.ctx?.adc) {
      this.ctx.adc.writeNorm(this.pwmPinName, normVal);
    } else if (typeof (this.ctx as any)?.analogWrite === 'function') {
      (this.ctx as any).analogWrite(this.pwmPinName, normVal);
    }
  }

  private clampAngle(angle: number): number {
    const { minAngle, maxAngle } = this.getResolvedProps();
    return Math.max(minAngle, Math.min(maxAngle, angle));
  }

  private publishAngle(angle: number): void {
    this.angle = angle;
    // smoothing deferred — angle === targetAngle today
    this.ctx?.publish('targetAngle', this.angle);
    this.ctx?.publish('angle', this.angle);
  }

  /** SET_ANGLE → _angle */
  _angle(angle: number): void {
    this.publishAngle(this.clampAngle(angle));
    this.writeDuty();
  }

  /**
   * Firmware PWM → stateChannels (canvas). Does not write the pin.
   * @see SimulationPlugin.onDutyChange
   */
  onDutyChange(channel: number, dutyPercent: number): void {
    const props = this.getResolvedProps();
    if (channel !== props.pwmChannel) return;
    const next = this.clampAngle(pwmDutyToAngle(dutyPercent / 100, props));
    if (Math.abs(next - this.angle) < 0.01) return;
    this.publishAngle(next);
  }

  onDestroy(): void {
    if (this.ctx?.gpio) {
      this.ctx.gpio.releasePin(this.pwmPinName);
    } else {
      this.ctx?.releasePin(this.pwmPinName);
    }
  }
}

export default {
  manifest: rcServoManifest,
  manifestFactory: rcServoManifestFactory,
  PluginClass: RcServoPlugin,
};
