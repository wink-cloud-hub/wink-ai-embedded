import { expect, test, describe } from 'bun:test';

import {
  SEG_VARIANTS,
  SEG_VARIANT_DIGITS,
  SEG_TOPOLOGIES,
  SEG_APPEARANCES,
  SEG_TOPOLOGY_EQUIVALENCE,
  resolveSegVariant,
  type SegVariantKey,
} from '../variants';

describe('seg_display variants & topology test suite', () => {
  test('constants and tables are frozen', () => {
    expect(Object.isFrozen(SEG_VARIANTS)).toBe(true);
    expect(Object.isFrozen(SEG_VARIANT_DIGITS)).toBe(true);
    expect(Object.isFrozen(SEG_TOPOLOGIES)).toBe(true);
    expect(Object.isFrozen(SEG_APPEARANCES)).toBe(true);
    expect(Object.isFrozen(SEG_TOPOLOGY_EQUIVALENCE)).toBe(true);
  });

  test('declares exactly 4 variants with expected digit counts', () => {
    expect(SEG_VARIANTS).toEqual([
      'direct_gpio_8d',
      'direct_gpio_4d',
      'direct_gpio_2d',
      'direct_gpio_1d',
    ]);
    expect(SEG_VARIANT_DIGITS.direct_gpio_8d).toBe(8);
    expect(SEG_VARIANT_DIGITS.direct_gpio_4d).toBe(4);
    expect(SEG_VARIANT_DIGITS.direct_gpio_2d).toBe(2);
    expect(SEG_VARIANT_DIGITS.direct_gpio_1d).toBe(1);
  });

  test('resolveSegVariant normalizes keys and falls back to 8d', () => {
    expect(resolveSegVariant('direct_gpio_8d')).toBe('direct_gpio_8d');
    expect(resolveSegVariant('direct_gpio_4d')).toBe('direct_gpio_4d');
    expect(resolveSegVariant('direct_gpio_2d')).toBe('direct_gpio_2d');
    expect(resolveSegVariant('direct_gpio_1d')).toBe('direct_gpio_1d');
    expect(resolveSegVariant('unknown_variant')).toBe('direct_gpio_8d');
    expect(resolveSegVariant(undefined)).toBe('direct_gpio_8d');
  });

  test('appearances are properly configured and map to valid variants', () => {
    const appearanceIds = Object.keys(SEG_APPEARANCES);
    expect(appearanceIds).toEqual([
      'seg_display_8',
      'seg_display_4',
      'seg_display_2',
      'seg_display_1',
    ]);

    for (const [id, app] of Object.entries(SEG_APPEARANCES)) {
      expect(app.appearanceId).toBe(id);
      expect(SEG_VARIANTS).toContain(app.variant);
      expect(app.searchAliases.length).toBeGreaterThan(0);
      expect(app.searchAliases).toContain('seg');
      expect(app.searchAliases).toContain('数码管');
    }
  });

  test('pinsOverlay has proper counts and coordinates for each variant', () => {
    const expectedSegmentPins = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'DP'];

    for (const variant of SEG_VARIANTS) {
      const top = SEG_TOPOLOGIES[variant];
      expect(top).toBeDefined();
      expect(top.variant).toBe(variant);
      expect(top.defaultAppearanceId).toBeDefined();

      const overlay = top.pinsOverlay;
      const nDigits = SEG_VARIANT_DIGITS[variant];
      const totalPins = 8 + nDigits;
      expect(Object.keys(overlay).length).toBe(totalPins);

      // Verify segment pins
      const segXCoords = new Set<number>();
      for (const seg of expectedSegmentPins) {
        const pin = overlay[seg];
        expect(pin).toBeDefined();
        expect(pin.relY).toBe(96);
        expect(pin.wireNet).toBe('primary');
        expect(pin.required).toBe(false);
        expect(segXCoords.has(pin.relX)).toBe(false);
        segXCoords.add(pin.relX);
      }

      // Verify digit pins
      const digXCoords = new Set<number>();
      for (let d = 0; d < nDigits; d++) {
        const digName = `DIG${d + 1}`;
        const pin = overlay[digName];
        expect(pin).toBeDefined();
        expect(pin.relY).toBe(0);
        expect(pin.wireNet).toBe('secondary');
        if (variant === 'direct_gpio_1d') {
          expect(pin.required).toBe(false);
        } else {
          expect(pin.required).toBe(true);
        }
        expect(digXCoords.has(pin.relX)).toBe(false);
        digXCoords.add(pin.relX);
      }
    }
  });
});
