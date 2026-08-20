import { describe, expect, it } from 'vitest';
import { pwmPercentToAngle } from '../pwm-duty-to-angle';

const SG90 = {
  minAngle: 0,
  maxAngle: 180,
  minPulseMs: 0.5,
  maxPulseMs: 2.5,
  framePeriodMs: 20,
};

describe('pwmPercentToAngle', () => {
  it('sG90 percent→angle goldens', () => {
    expect(pwmPercentToAngle(7.5, SG90)).toBeCloseTo(90);
    expect(pwmPercentToAngle(2.5, SG90)).toBeCloseTo(0);
    expect(pwmPercentToAngle(12.5, SG90)).toBeCloseTo(180);
  });

  it('respects custom framePeriodMs', () => {
    const p = { ...SG90, framePeriodMs: 10 };
    // mid pulse 1.5ms / 10ms = 15%
    expect(pwmPercentToAngle(15, p)).toBeCloseTo(90);
  });

  it('respects custom angle range', () => {
    const p = { ...SG90, minAngle: 0, maxAngle: 90 };
    expect(pwmPercentToAngle(7.5, p)).toBeCloseTo(45);
  });
});
