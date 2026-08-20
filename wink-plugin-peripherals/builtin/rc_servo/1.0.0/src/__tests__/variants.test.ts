import { expect, it } from 'vitest';

import {
  RC_SERVO_APPEARANCES,
  RC_SERVO_TOPOLOGIES,
  RC_SERVO_TOPOLOGY_EQUIVALENCE,
} from '../variants';

it('tables are frozen', () => {
  expect(Object.isFrozen(RC_SERVO_TOPOLOGIES)).toBe(true);
  expect(Object.isFrozen(RC_SERVO_APPEARANCES)).toBe(true);
});

it('overlay keys ⊆ sg90 pin names', () => {
  const binderNames = new Set(['PWM', 'VCC', 'GND']);
  for (const name of Object.keys(RC_SERVO_TOPOLOGIES.sg90.pinsOverlay)) {
    expect(binderNames.has(name)).toBe(true);
  }
});

it('defaultAppearanceId exists in APPEARANCES', () => {
  const id = RC_SERVO_TOPOLOGIES.sg90.defaultAppearanceId;
  expect(RC_SERVO_APPEARANCES[id]?.variant).toBe('sg90');
});

it('eQUIVALENCE empty for single topology', () => {
  expect(RC_SERVO_TOPOLOGY_EQUIVALENCE).toEqual({});
  expect(Object.keys(RC_SERVO_TOPOLOGIES)).toEqual(['sg90']);
});
