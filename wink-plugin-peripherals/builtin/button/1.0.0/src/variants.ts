import type { PinsOverlayMap, GeneratedBinderPin } from '@wink-ai/unisim-ui';
import { pinsFromBinderVariant } from '@wink-ai/unisim-ui';

export type ButtonVariantKey = 'default';

export const BUTTON_TOPOLOGY_EQUIVALENCE: Readonly<Record<string, readonly string[]>> =
  Object.freeze({});

const BUTTON_OVERLAY: Readonly<PinsOverlayMap> = Object.freeze({
  '1.l': Object.freeze({ relX: -5, relY: 20, wireNet: 'primary' as const }),
  '2.l': Object.freeze({
    relX: -5,
    relY: 40,
    wireNet: 'gnd' as const,
    defaultConnection: 'GND' as const,
  }),
  '1.r': Object.freeze({ relX: 75, relY: 13, wireNet: 'primary' as const }),
  '2.r': Object.freeze({ relX: 75, relY: 33, wireNet: 'gnd' as const }),
});

export const BUTTON_TOPOLOGIES = Object.freeze({
  default: Object.freeze({
    variant: 'default' as const,
    getPins: (): readonly GeneratedBinderPin[] => pinsFromBinderVariant('button', 'default'),
    pinsOverlay: BUTTON_OVERLAY,
    defaultAppearanceId: 'button_default',
  }),
});

export const BUTTON_APPEARANCES = Object.freeze({
  button_default: Object.freeze({
    appearanceId: 'button_default',
    variant: 'default' as const,
    displayName: 'GPIO Push Button',
    searchAliases: Object.freeze(['button', 'gpio', 'push', 'input', 'default'] as const),
  }),
});
