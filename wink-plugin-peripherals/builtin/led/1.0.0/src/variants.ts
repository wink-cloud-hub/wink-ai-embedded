import type { PinsOverlayMap, GeneratedBinderPin } from '@wink-ai/unisim-ui';
import { pinsFromBinderVariant } from '@wink-ai/unisim-ui';

export type LedVariantKey = 'default';

export const LED_TOPOLOGY_EQUIVALENCE: Readonly<Record<string, readonly string[]>> = Object.freeze(
  {},
);

const LED_OVERLAY: Readonly<PinsOverlayMap> = Object.freeze({
  A: Object.freeze({
    relX: 30,
    relY: 50,
    wireNet: 'primary' as const,
    defaultConnection: 13 as const,
    required: true,
  }),
  C: Object.freeze({
    relX: 10,
    relY: 50,
    wireNet: 'gnd' as const,
    defaultConnection: 'GND' as const,
    required: false,
  }),
});

export const LED_TOPOLOGIES = Object.freeze({
  default: Object.freeze({
    variant: 'default' as const,
    getPins: (): readonly GeneratedBinderPin[] => pinsFromBinderVariant('led', 'default'),
    pinsOverlay: LED_OVERLAY,
    defaultAppearanceId: 'led_default',
  }),
});

export const LED_APPEARANCES = Object.freeze({
  led_default: Object.freeze({
    appearanceId: 'led_default',
    variant: 'default' as const,
    displayName: 'GPIO LED',
    searchAliases: Object.freeze(['led', 'gpio', 'indicator', 'default'] as const),
  }),
});
