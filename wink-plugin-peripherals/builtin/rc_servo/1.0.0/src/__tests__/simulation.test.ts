import { beforeEach, describe, expect, test } from 'bun:test';
import { RcServoPlugin, rcServoManifest } from '../simulation';
import { SimulationPluginHost, PluginRegistry, PinArbiter, VirtualClock } from '@wink-ai/unisim';

describe('SG90 Servo analog duty pathway', () => {
  let arbiter: PinArbiter;
  let clock: VirtualClock;
  let host: SimulationPluginHost;
  let plugin: RcServoPlugin;

  beforeEach(() => {
    PluginRegistry.clear();
    plugin = new RcServoPlugin();
    PluginRegistry.register(
      rcServoManifest,
      class {
        constructor() {
          return plugin;
        }
      } as any,
    );
    arbiter = new PinArbiter();
    clock = new VirtualClock();
    host = new SimulationPluginHost(arbiter, clock);
    host.registerFromConfig({
      instanceId: 'rc_servo:0',
      type: 'rc_servo',
      pinMapping: { PWM: 5 },
    });
  });

  test('_angle(90) writes mid-range analog duty on pwm pin', () => {
    plugin._angle(90);
    expect(arbiter.readAnalog(5)).toBeCloseTo(0.075);
  });

  test('_angle(0) and _angle(180) write min and max duty', () => {
    plugin._angle(0);
    expect(arbiter.readAnalog(5)).toBeCloseTo(0.5 / 20); // 0.025
    plugin._angle(180);
    expect(arbiter.readAnalog(5)).toBeCloseTo(2.5 / 20); // 0.125
  });

  test('onDutyChange publishes angle without writing PWM pin', () => {
    host.dispatchEvent('rc_servo:0', 'SET_ANGLE', { angle: 90 });
    const dutyAfterSet = arbiter.readAnalog(5);

    plugin.onDutyChange(0, 2.5);
    expect(host.getStateSnapshot()['rc_servo:0']?.angle).toBeCloseTo(0);
    expect(arbiter.readAnalog(5)).toBeCloseTo(dutyAfterSet);

    plugin.onDutyChange(0, 12.5);
    expect(host.getStateSnapshot()['rc_servo:0']?.angle).toBeCloseTo(180);
    expect(arbiter.readAnalog(5)).toBeCloseTo(dutyAfterSet);

    plugin.onDutyChange(1, 2.5);
    expect(host.getStateSnapshot()['rc_servo:0']?.angle).toBeCloseTo(180);
  });

  test('notifyDutyChange push path updates angle via registered duty listeners', () => {
    host.notifyDutyChange(0, 2.5);
    expect(host.getStateSnapshot()['rc_servo:0']?.angle).toBeCloseTo(0);

    host.notifyDutyChange(0, 12.5);
    expect(host.getStateSnapshot()['rc_servo:0']?.angle).toBeCloseTo(180);
  });
});
