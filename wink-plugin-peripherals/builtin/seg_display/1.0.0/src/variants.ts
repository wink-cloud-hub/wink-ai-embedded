import type { PinsOverlayMap, GeneratedBinderPin } from '@wink-ai/unisim-ui';
import { pinsFromBinderVariant } from '@wink-ai/unisim-ui';
import { normalizeVariantKey } from '@wink-ai/unisim';

export type SegVariantKey =
  | 'direct_gpio_8d'
  | 'direct_gpio_4d'
  | 'direct_gpio_2d'
  | 'direct_gpio_1d';

export const SEG_VARIANTS: readonly SegVariantKey[] = Object.freeze([
  'direct_gpio_8d',
  'direct_gpio_4d',
  'direct_gpio_2d',
  'direct_gpio_1d',
] as const);

export const SEG_VARIANT_DIGITS: Readonly<Record<SegVariantKey, number>> = Object.freeze({
  direct_gpio_8d: 8,
  direct_gpio_4d: 4,
  direct_gpio_2d: 2,
  direct_gpio_1d: 1,
});

export function resolveSegVariant(raw?: string): SegVariantKey {
  const normalized = normalizeVariantKey(raw);
  if (normalized && normalized in SEG_VARIANT_DIGITS) {
    return normalized as SegVariantKey;
  }
  return 'direct_gpio_8d';
}

export const SEG_TOPOLOGY_EQUIVALENCE: Readonly<Record<string, readonly string[]>> = Object.freeze({
  '8d': Object.freeze(['direct_gpio_8d'] as const),
  '4d': Object.freeze(['direct_gpio_4d'] as const),
  '2d': Object.freeze(['direct_gpio_2d'] as const),
  '1d': Object.freeze(['direct_gpio_1d'] as const),
});

const SEGMENT_NAMES = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'DP'] as const;
const SEGMENT_X_COORDS = [23, 47, 70, 93, 117, 140, 163, 187] as const;

function createSegmentOverlay(): Record<string, { relX: number; relY: number; wireNet: 'primary'; required: false }> {
  const overlay: Record<string, { relX: number; relY: number; wireNet: 'primary'; required: false }> = {};
  for (let i = 0; i < SEGMENT_NAMES.length; i++) {
    overlay[SEGMENT_NAMES[i]] = Object.freeze({
      relX: SEGMENT_X_COORDS[i],
      relY: 96,
      wireNet: 'primary' as const,
      required: false,
    });
  }
  return overlay;
}

function createOverlay(variant: SegVariantKey): Readonly<PinsOverlayMap> {
  const segOverlay = createSegmentOverlay();
  const digitOverlay: Record<string, { relX: number; relY: number; wireNet: 'secondary'; required: boolean }> = {};
  const nDigits = SEG_VARIANT_DIGITS[variant];

  if (nDigits === 8) {
    for (let d = 0; d < 8; d++) {
      digitOverlay[`DIG${d + 1}`] = Object.freeze({
        relX: SEGMENT_X_COORDS[d],
        relY: 0,
        wireNet: 'secondary' as const,
        required: true,
      });
    }
  } else if (nDigits === 4) {
    const xCoords = [42, 84, 126, 168];
    for (let d = 0; d < 4; d++) {
      digitOverlay[`DIG${d + 1}`] = Object.freeze({
        relX: xCoords[d],
        relY: 0,
        wireNet: 'secondary' as const,
        required: true,
      });
    }
  } else if (nDigits === 2) {
    const xCoords = [70, 140];
    for (let d = 0; d < 2; d++) {
      digitOverlay[`DIG${d + 1}`] = Object.freeze({
        relX: xCoords[d],
        relY: 0,
        wireNet: 'secondary' as const,
        required: true,
      });
    }
  } else {
    // 1 digit: DIG1 is optional for static drive
    digitOverlay['DIG1'] = Object.freeze({
      relX: 105,
      relY: 0,
      wireNet: 'secondary' as const,
      required: false,
    });
  }

  return Object.freeze({
    ...segOverlay,
    ...digitOverlay,
  });
}

export const SEG_TOPOLOGIES = Object.freeze({
  direct_gpio_8d: Object.freeze({
    variant: 'direct_gpio_8d' as const,
    getPins: (): readonly GeneratedBinderPin[] => pinsFromBinderVariant('seg_display', 'direct_gpio_8d'),
    pinsOverlay: createOverlay('direct_gpio_8d'),
    defaultAppearanceId: 'seg_display_8',
  }),
  direct_gpio_4d: Object.freeze({
    variant: 'direct_gpio_4d' as const,
    getPins: (): readonly GeneratedBinderPin[] => pinsFromBinderVariant('seg_display', 'direct_gpio_4d'),
    pinsOverlay: createOverlay('direct_gpio_4d'),
    defaultAppearanceId: 'seg_display_4',
  }),
  direct_gpio_2d: Object.freeze({
    variant: 'direct_gpio_2d' as const,
    getPins: (): readonly GeneratedBinderPin[] => pinsFromBinderVariant('seg_display', 'direct_gpio_2d'),
    pinsOverlay: createOverlay('direct_gpio_2d'),
    defaultAppearanceId: 'seg_display_2',
  }),
  direct_gpio_1d: Object.freeze({
    variant: 'direct_gpio_1d' as const,
    getPins: (): readonly GeneratedBinderPin[] => pinsFromBinderVariant('seg_display', 'direct_gpio_1d'),
    pinsOverlay: createOverlay('direct_gpio_1d'),
    defaultAppearanceId: 'seg_display_1',
  }),
});

export const SEG_APPEARANCES = Object.freeze({
  seg_display_8: Object.freeze({
    appearanceId: 'seg_display_8',
    variant: 'direct_gpio_8d' as const,
    displayName: '8-Digit 7-Segment Display',
    searchAliases: Object.freeze(['seg', '7seg', '数码管', '8d', 'digital tube'] as const),
  }),
  seg_display_4: Object.freeze({
    appearanceId: 'seg_display_4',
    variant: 'direct_gpio_4d' as const,
    displayName: '4-Digit 7-Segment Display',
    searchAliases: Object.freeze(['seg', '7seg', '数码管', '4d', 'digital tube'] as const),
  }),
  seg_display_2: Object.freeze({
    appearanceId: 'seg_display_2',
    variant: 'direct_gpio_2d' as const,
    displayName: '2-Digit 7-Segment Display',
    searchAliases: Object.freeze(['seg', '7seg', '数码管', '2d', 'digital tube'] as const),
  }),
  seg_display_1: Object.freeze({
    appearanceId: 'seg_display_1',
    variant: 'direct_gpio_1d' as const,
    displayName: '1-Digit 7-Segment Display',
    searchAliases: Object.freeze(['seg', '7seg', '数码管', '1d', 'digital tube'] as const),
  }),
});
