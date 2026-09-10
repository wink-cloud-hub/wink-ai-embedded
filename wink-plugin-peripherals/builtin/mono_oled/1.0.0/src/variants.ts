import type { PinsOverlayMap, GeneratedBinderPin } from '@wink-ai/unisim-ui';
import { pinsFromBinderVariant } from '@wink-ai/unisim-ui';

export type MonoOledVariantKey = 'ssd1306_i2c' | 'ssd1306_spi';

export const MONO_OLED_TOPOLOGY_EQUIVALENCE: Readonly<
  Record<MonoOledVariantKey, readonly MonoOledVariantKey[]>
> = Object.freeze({
  ssd1306_i2c: [],
  ssd1306_spi: [],
});

import { MONO_OLED_AUTOGEN_OVERLAY, MONO_OLED_AUTOGEN_SIZE } from './variants.generated';

export const MONO_OLED_SIZE = MONO_OLED_AUTOGEN_SIZE;
export const MONO_OLED_OVERLAY = MONO_OLED_AUTOGEN_OVERLAY;

const I2C_OVERLAY: Readonly<PinsOverlayMap> = Object.freeze({
  DATA: Object.freeze({
    ...MONO_OLED_AUTOGEN_OVERLAY.DATA,
    defaultConnection: 21 as const,
  }),
  CLK: Object.freeze({
    ...MONO_OLED_AUTOGEN_OVERLAY.CLK,
    defaultConnection: 22 as const,
    required: true,
  }),
  '3V3': MONO_OLED_AUTOGEN_OVERLAY['3V3'],
  GND: MONO_OLED_AUTOGEN_OVERLAY.GND,
});

const SPI_OVERLAY: Readonly<PinsOverlayMap> = Object.freeze({
  CLK: Object.freeze({
    ...MONO_OLED_AUTOGEN_OVERLAY.CLK,
    defaultConnection: 18 as const,
  }),
  DIN: Object.freeze({
    ...MONO_OLED_AUTOGEN_OVERLAY.DATA,
    defaultConnection: 23 as const,
  }),
  CS: Object.freeze({
    ...MONO_OLED_AUTOGEN_OVERLAY.CS,
    defaultConnection: 5 as const,
  }),
  DC: Object.freeze({
    ...MONO_OLED_AUTOGEN_OVERLAY.DC,
    defaultConnection: 17 as const,
  }),
  RES: Object.freeze({
    ...MONO_OLED_AUTOGEN_OVERLAY.RST,
    defaultConnection: 16 as const,
  }),
  '3V3': MONO_OLED_AUTOGEN_OVERLAY['3V3'],
  GND: MONO_OLED_AUTOGEN_OVERLAY.GND,
});

export const MONO_OLED_TOPOLOGIES: Readonly<
  Record<
    MonoOledVariantKey,
    {
      readonly variant: MonoOledVariantKey;
      readonly getPins: () => readonly GeneratedBinderPin[];
      readonly pinsOverlay: Readonly<PinsOverlayMap>;
      readonly defaultAppearanceId: string;
    }
  >
> = Object.freeze({
  ssd1306_i2c: Object.freeze({
    variant: 'ssd1306_i2c',
    getPins: () => pinsFromBinderVariant('mono_oled', 'ssd1306_i2c'),
    pinsOverlay: I2C_OVERLAY,
    defaultAppearanceId: 'mono_oled_ssd1306_i2c',
  }),
  ssd1306_spi: Object.freeze({
    variant: 'ssd1306_spi',
    getPins: () => pinsFromBinderVariant('mono_oled', 'ssd1306_spi'),
    pinsOverlay: SPI_OVERLAY,
    defaultAppearanceId: 'mono_oled_ssd1306_spi',
  }),
});

export const MONO_OLED_APPEARANCES: Readonly<
  Record<
    string,
    {
      readonly appearanceId: string;
      readonly variant: MonoOledVariantKey;
      readonly displayName: string;
      readonly searchAliases: readonly string[];
      readonly size?: { readonly width: number; readonly height: number };
    }
  >
> = Object.freeze({
  mono_oled_ssd1306_i2c: Object.freeze({
    appearanceId: 'mono_oled_ssd1306_i2c',
    variant: 'ssd1306_i2c' as const,
    displayName: 'SSD1306 0.96" I2C OLED',
    searchAliases: Object.freeze(['0.96', 'i2c', 'ssd1306']),
  }),
  mono_oled_ssd1306_spi: Object.freeze({
    appearanceId: 'mono_oled_ssd1306_spi',
    variant: 'ssd1306_spi' as const,
    displayName: 'SSD1306 0.96" SPI OLED',
    searchAliases: Object.freeze(['0.96', 'spi', 'ssd1306']),
  }),
});
