/**
 * Hobby-servo PWM percent → angle adapter for electricalMirror convert.
 *
 * SSOT formula: `packages/unisim/src/plugin/base/pwm-angle-duty.ts`
 * (`pwmDutyToAngle`). Keep this copy in sync — embedded-frontend does not
 * depend on `@wink-ai/unisim` yet.
 *
 * Worker / ActuatorOutputBatch.pwm uses percent 0..100; unisim plugin side
 * uses duty fraction 0..1. This adapter stays in percent space so goldens
 * like 7.5 → 90 stay exact (avoid 0.075 float drift).
 */

export interface HobbyServoPulseParams {
  minAngle: number;
  maxAngle: number;
  minPulseMs: number;
  maxPulseMs: number;
  framePeriodMs: number;
}

/** Worker PWM percent (0..100) → angle degrees */
export function pwmPercentToAngle(dutyPercent: number, params: HobbyServoPulseParams): number {
  const frame = params.framePeriodMs || 20;
  const minDuty = (params.minPulseMs / frame) * 100;
  const maxDuty = (params.maxPulseMs / frame) * 100;
  const span = maxDuty - minDuty || 1;
  const t = Math.min(1, Math.max(0, (dutyPercent - minDuty) / span));
  return params.minAngle + t * (params.maxAngle - params.minAngle);
}
