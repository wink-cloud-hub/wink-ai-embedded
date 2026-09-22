import type { PinsOverlayMap, GeneratedBinderPin } from '@wink-ai/unisim-ui';
import { pinsFromBinderVariant } from '@wink-ai/unisim-ui';

export type RcServoVariantKey = 'sg90';

export const RC_SERVO_TOPOLOGY_EQUIVALENCE: Readonly<Record<string, readonly string[]>> =
  Object.freeze({});

import { RC_SERVO_AUTOGEN_OVERLAY, RC_SERVO_AUTOGEN_SIZE } from './variants.generated';

export const SG90_OVERLAY = RC_SERVO_AUTOGEN_OVERLAY;
export const SG90_SIZE = RC_SERVO_AUTOGEN_SIZE;

export const RC_SERVO_TOPOLOGIES = Object.freeze({
  sg90: Object.freeze({
    variant: 'sg90' as const,
    getPins: (): readonly GeneratedBinderPin[] => pinsFromBinderVariant('rc_servo', 'sg90'),
    pinsOverlay: SG90_OVERLAY,
    defaultAppearanceId: 'rc_servo_sg90',
  }),
});

export const RC_SERVO_APPEARANCES = Object.freeze({
  rc_servo_sg90: Object.freeze({
    appearanceId: 'rc_servo_sg90',
    variant: 'sg90' as const,
    displayName: 'SG90 9g Micro Servo',
    searchAliases: Object.freeze(['sg90', 'servo', 'pwm'] as const),
  }),
});
