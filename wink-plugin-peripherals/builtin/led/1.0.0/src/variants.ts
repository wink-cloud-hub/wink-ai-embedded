import type { PinsOverlayMap, GeneratedBinderPin } from '@wink-ai/unisim-ui';
import { pinsFromBinderVariant } from '@wink-ai/unisim-ui';

export type LedVariantKey = 'default';

export const LED_TOPOLOGY_EQUIVALENCE: Readonly<Record<string, readonly string[]>> = Object.freeze(
  {},
);

import { LED_AUTOGEN_OVERLAY, LED_AUTOGEN_SIZE } from './variants.generated';

export const LED_OVERLAY = LED_AUTOGEN_OVERLAY;
export const LED_SIZE = LED_AUTOGEN_SIZE;

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
