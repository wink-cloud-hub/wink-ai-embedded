import { angleToPwmDuty, pwmDutyToAngle } from '../pwm-angle-duty';

describe('pwm-angle-duty', () => {
  const params = {
    minAngle: 0,
    maxAngle: 180,
    minPulseMs: 0.5,
    maxPulseMs: 2.5,
    framePeriodMs: 20,
  };

  test('angleToPwmDuty maps min, mid, max angles to duty', () => {
    expect(angleToPwmDuty(0, params)).toBeCloseTo(0.025); // 0.5 / 20 = 0.025
    expect(angleToPwmDuty(90, params)).toBeCloseTo(0.075); // 1.5 / 20 = 0.075
    expect(angleToPwmDuty(180, params)).toBeCloseTo(0.125); // 2.5 / 20 = 0.125
  });

  test('pwmDutyToAngle maps min, mid, max duties back to angles', () => {
    expect(pwmDutyToAngle(0.025, params)).toBeCloseTo(0);
    expect(pwmDutyToAngle(0.075, params)).toBeCloseTo(90);
    expect(pwmDutyToAngle(0.125, params)).toBeCloseTo(180);
  });
});
