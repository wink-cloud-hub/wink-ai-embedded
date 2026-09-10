import type { PinsOverlayMap, GeneratedBinderPin } from '@wink-ai/unisim-ui';
import { pinsFromBinderVariant } from '@wink-ai/unisim-ui';

export type ButtonVariantKey = 'default';

export const BUTTON_TOPOLOGY_EQUIVALENCE: Readonly<Record<string, readonly string[]>> =
  Object.freeze({});

import { BUTTON_AUTOGEN_OVERLAY, BUTTON_AUTOGEN_SIZE } from './variants.generated';

export const BUTTON_OVERLAY = BUTTON_AUTOGEN_OVERLAY;
export const BUTTON_SIZE = BUTTON_AUTOGEN_SIZE;

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
