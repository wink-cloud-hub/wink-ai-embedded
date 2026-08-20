import { describe, expect, it } from 'vitest';
import { servoDefinition } from '../definition';
import type { CircuitComponentInstance } from '@wink-ai/unisim-ui';

describe('rc_servo peripheral definition', () => {
  it('has correct type and metadata', () => {
    const def = servoDefinition;
    expect(def).toBeDefined();
    expect(def?.displayName).toBe('RC Servo Motor');
    expect(def?.category).toBe('actuator');
    expect(def?.catalog?.id).toBe('rc_servo');
    expect(def?.pins.map(p => p.name)).toEqual(['PWM', 'VCC', 'GND']);
  });

  it('does not declare redundant actuatorObserve in frontend definition (SSOT in unisim binder)', () => {
    const def = servoDefinition;
    expect(def?.actuatorObserve).toBeUndefined();
  });

  it('does not declare redundant simulation.observe in frontend definition (SSOT in unisim binder)', () => {
    const def = servoDefinition;
    expect(def?.simulation?.observe).toBeUndefined();
  });

  it('declares props schema with angle and invert defaults', () => {
    const def = servoDefinition;
    expect(def?.props.angle?.default).toBe(90);
    expect(def?.props.invert?.default).toBe(false);
  });
});
