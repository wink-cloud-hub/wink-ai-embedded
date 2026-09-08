import { beforeEach, describe, expect, test } from 'bun:test';
import { RcServoPlugin, rcServoManifest } from '../simulation';
import { createPluginTestHost, type PluginTestHost } from '@wink-ai/unisim/sdk';

describe('SG90 Servo analog duty pathway', () => {
  let host: PluginTestHost;
  let plugin: RcServoPlugin;

  beforeEach(() => {
    // Sanctioned harness: virtual clock + arbiter + buses wired like headless,
    // no imports from @wink-ai/unisim/core or /host (both unexported).
    host = createPluginTestHost();
    plugin = host.bind(RcServoPlugin, {
      instanceId: 'rc_servo:0',
      pinMapping: { PWM: 5 },
    }).instance as RcServoPlugin;
  });

  test('_angle(90) writes mid-range analog duty on pwm pin', () => {
    plugin._angle(90);
    expect(host.readAnalog(5)).toBeCloseTo(0.075);
  });

  test('_angle(0) and _angle(180) write min and max duty', () => {
    plugin._angle(0);
    expect(host.readAnalog(5)).toBeCloseTo(0.5 / 20); // 0.025
    plugin._angle(180);
    expect(host.readAnalog(5)).toBeCloseTo(2.5 / 20); // 0.125
  });

  test('onDutyChange publishes angle without writing PWM pin', () => {
    host.dispatchEvent('rc_servo:0', 'SET_ANGLE', { angle: 90 });
    const dutyAfterSet = host.readAnalog(5);

    plugin.onDutyChange(0, 2.5);
    expect(host.getStateSnapshot()['rc_servo:0']?.angle).toBeCloseTo(0);
    expect(host.readAnalog(5)).toBeCloseTo(dutyAfterSet);

    plugin.onDutyChange(0, 12.5);
    expect(host.getStateSnapshot()['rc_servo:0']?.angle).toBeCloseTo(180);
    expect(host.readAnalog(5)).toBeCloseTo(dutyAfterSet);

    plugin.onDutyChange(1, 2.5);
    expect(host.getStateSnapshot()['rc_servo:0']?.angle).toBeCloseTo(180);
  });

  test('notifyDutyChange push path updates angle via registered duty listeners', () => {
    host.notifyDutyChange(0, 2.5);
    expect(host.getStateSnapshot()['rc_servo:0']?.angle).toBeCloseTo(0);

    host.notifyDutyChange(0, 12.5);
    expect(host.getStateSnapshot()['rc_servo:0']?.angle).toBeCloseTo(180);
  });

  test('binds with the declared rc_servo manifest', () => {
    expect(plugin.manifest.type).toBe(rcServoManifest.type);
  });
});
