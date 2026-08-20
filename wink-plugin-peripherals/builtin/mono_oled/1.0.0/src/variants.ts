import type { PinsOverlayMap, GeneratedBinderPin } from '@wink-ai/unisim-ui';
import { pinsFromBinderVariant } from '@wink-ai/unisim-ui';

export type MonoOledVariantKey = 'ssd1306_i2c' | 'ssd1306_spi';

export const MONO_OLED_TOPOLOGY_EQUIVALENCE: Readonly<
  Record<MonoOledVariantKey, readonly MonoOledVariantKey[]>
> = Object.freeze({
  ssd1306_i2c: [],
  ssd1306_spi: [],
});

const I2C_OVERLAY: Readonly<PinsOverlayMap> = Object.freeze({
  DATA: Object.freeze({
    relX: 40,
    relY: 75,
    wireNet: 'primary' as const,
    defaultConnection: 21 as const,
    required: true,
  }),
  CLK: Object.freeze({
    relX: 50,
    relY: 75,
    wireNet: 'secondary' as const,
    defaultConnection: 22 as const,
    required: true,
  }),
  '3V3': Object.freeze({
    relX: 90,
    relY: 75,
    wireNet: 'vcc' as const,
    defaultConnection: '3V3' as const,
    required: false,
  }),
  GND: Object.freeze({
    relX: 110,
    relY: 75,
    wireNet: 'gnd' as const,
    defaultConnection: 'GND' as const,
    required: false,
  }),
});

const SPI_OVERLAY: Readonly<PinsOverlayMap> = Object.freeze({
  CLK: Object.freeze({
    relX: 30,
    relY: 75,
    wireNet: 'secondary' as const,
    defaultConnection: 18 as const,
    required: true,
  }),
  DIN: Object.freeze({
    relX: 45,
    relY: 75,
    wireNet: 'primary' as const,
    defaultConnection: 23 as const,
    required: true,
  }),
  CS: Object.freeze({
    relX: 60,
    relY: 75,
    wireNet: 'secondary' as const,
    defaultConnection: 5 as const,
    required: false,
  }),
  DC: Object.freeze({
    relX: 75,
    relY: 75,
    wireNet: 'secondary' as const,
    defaultConnection: 17 as const,
    required: true,
  }),
  RES: Object.freeze({
    relX: 90,
    relY: 75,
    wireNet: 'secondary' as const,
    defaultConnection: 16 as const,
    required: false,
  }),
  '3V3': Object.freeze({
    relX: 105,
    relY: 75,
    wireNet: 'vcc' as const,
    defaultConnection: '3V3' as const,
    required: false,
  }),
  GND: Object.freeze({
    relX: 120,
    relY: 75,
    wireNet: 'gnd' as const,
    defaultConnection: 'GND' as const,
    required: false,
  }),
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
