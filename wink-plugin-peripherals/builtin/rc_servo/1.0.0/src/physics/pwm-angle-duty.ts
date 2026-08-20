export type PwmAngleDutyParams = {
  minAngle: number;
  maxAngle: number;
  minPulseMs: number;
  maxPulseMs: number;
  framePeriodMs: number;
};

/**
 * Map angle to PWM duty in [0, 1] using a linear pulse-width model
 * (hobby servo: pulseMs / framePeriodMs).
 */
export function angleToPwmDuty(angle: number, params: PwmAngleDutyParams): number {
  const range = params.maxAngle - params.minAngle || 1;
  const t = Math.min(1, Math.max(0, (angle - params.minAngle) / range));
  const pulseMs = params.minPulseMs + t * (params.maxPulseMs - params.minPulseMs);
  const frame = params.framePeriodMs || 20;
  return Math.min(1, Math.max(0, pulseMs / frame));
}

/**
 * Inverse of {@link angleToPwmDuty}: map PWM duty in [0, 1] back to angle
 * using the same linear pulse-width model.
 */
export function pwmDutyToAngle(duty01: number, params: PwmAngleDutyParams): number {
  const frame = params.framePeriodMs || 20;
  const minDuty = params.minPulseMs / frame;
  const maxDuty = params.maxPulseMs / frame;
  const span = maxDuty - minDuty || 1;
  const t = Math.min(1, Math.max(0, (duty01 - minDuty) / span));
  return params.minAngle + t * (params.maxAngle - params.minAngle);
}
