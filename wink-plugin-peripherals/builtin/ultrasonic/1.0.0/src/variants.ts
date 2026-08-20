import type { PinsOverlayMap, GeneratedBinderPin } from '@wink-ai/unisim-ui';
import { pinsFromBinderVariant } from '@wink-ai/unisim-ui';

export type UltrasonicVariantKey = 'hcsr04' | 'single_pin_ping' | 'uart_stream' | 'i2c';

const HCSR04_OVERLAY: Readonly<PinsOverlayMap> = Object.freeze({
  TRIG: Object.freeze({
    relX: 82,
    relY: 95,
    wireNet: 'secondary' as const,
    defaultConnection: 12 as const,
  }),
  ECHO: Object.freeze({
    relX: 92,
    relY: 95,
    wireNet: 'primary' as const,
    defaultConnection: 13 as const,
  }),
  VCC: Object.freeze({
    relX: 72,
    relY: 95,
    wireNet: 'vcc' as const,
    defaultConnection: 'VCC' as const,
  }),
  GND: Object.freeze({
    relX: 102,
    relY: 95,
    wireNet: 'gnd' as const,
    defaultConnection: 'GND' as const,
  }),
});

const PING_OVERLAY: Readonly<PinsOverlayMap> = Object.freeze({
  SIG: Object.freeze({
    relX: 87,
    relY: 95,
    wireNet: 'primary' as const,
    defaultConnection: 12 as const,
  }),
  VCC: Object.freeze({
    relX: 72,
    relY: 95,
    wireNet: 'vcc' as const,
    defaultConnection: 'VCC' as const,
  }),
  GND: Object.freeze({
    relX: 102,
    relY: 95,
    wireNet: 'gnd' as const,
    defaultConnection: 'GND' as const,
  }),
});

const UART_OVERLAY: Readonly<PinsOverlayMap> = Object.freeze({
  TX: Object.freeze({
    relX: 82,
    relY: 95,
    wireNet: 'secondary' as const,
    defaultConnection: 17 as const,
  }),
  RX: Object.freeze({
    relX: 92,
    relY: 95,
    wireNet: 'primary' as const,
    defaultConnection: 16 as const,
  }),
  VCC: Object.freeze({
    relX: 72,
    relY: 95,
    wireNet: 'vcc' as const,
    defaultConnection: 'VCC' as const,
  }),
  GND: Object.freeze({
    relX: 102,
    relY: 95,
    wireNet: 'gnd' as const,
    defaultConnection: 'GND' as const,
  }),
});

const I2C_OVERLAY: Readonly<PinsOverlayMap> = Object.freeze({
  SDA: Object.freeze({
    relX: 82,
    relY: 95,
    wireNet: 'primary' as const,
    defaultConnection: 21 as const,
  }),
  SCL: Object.freeze({
    relX: 92,
    relY: 95,
    wireNet: 'secondary' as const,
    defaultConnection: 22 as const,
  }),
  VCC: Object.freeze({
    relX: 72,
    relY: 95,
    wireNet: 'vcc' as const,
    defaultConnection: 'VCC' as const,
  }),
  GND: Object.freeze({
    relX: 102,
    relY: 95,
    wireNet: 'gnd' as const,
    defaultConnection: 'GND' as const,
  }),
});

export const ultrasonicPinsOverlay = HCSR04_OVERLAY;

export const ULTRASONIC_TOPOLOGIES = Object.freeze({
  hcsr04: Object.freeze({
    variant: 'hcsr04' as const,
    getPins: () => pinsFromBinderVariant('ultrasonic', 'hcsr04'),
    pinsOverlay: HCSR04_OVERLAY,
    defaultAppearanceId: 'ultrasonic_hcsr04',
  }),
  single_pin_ping: Object.freeze({
    variant: 'single_pin_ping' as const,
    getPins: () => pinsFromBinderVariant('ultrasonic', 'single_pin_ping'),
    pinsOverlay: PING_OVERLAY,
    defaultAppearanceId: 'ultrasonic_ping',
  }),
  uart_stream: Object.freeze({
    variant: 'uart_stream' as const,
    getPins: () => pinsFromBinderVariant('ultrasonic', 'uart_stream'),
    pinsOverlay: UART_OVERLAY,
    defaultAppearanceId: 'ultrasonic_uart',
  }),
  i2c: Object.freeze({
    variant: 'i2c' as const,
    getPins: () => pinsFromBinderVariant('ultrasonic', 'i2c'),
    pinsOverlay: I2C_OVERLAY,
    defaultAppearanceId: 'ultrasonic_i2c',
  }),
});

export const ULTRASONIC_APPEARANCES = Object.freeze({
  ultrasonic_hcsr04: Object.freeze({
    appearanceId: 'ultrasonic_hcsr04',
    variant: 'hcsr04' as const,
    displayName: 'HC-SR04 Ultrasonic Sensor (4-Pin)',
    elementTag: 'wokwi-hc-sr04',
  }),
  ultrasonic_ping: Object.freeze({
    appearanceId: 'ultrasonic_ping',
    variant: 'single_pin_ping' as const,
    displayName: 'Parallax PING))) Ultrasonic (3-Pin)',
    elementTag: 'wink-custom-ping',
  }),
  ultrasonic_uart: Object.freeze({
    appearanceId: 'ultrasonic_uart',
    variant: 'uart_stream' as const,
    displayName: 'US-100 UART Ultrasonic Sensor',
    elementTag: 'wink-custom-ultrasonic-uart',
  }),
  ultrasonic_i2c: Object.freeze({
    appearanceId: 'ultrasonic_i2c',
    variant: 'i2c' as const,
    displayName: 'Devantech SRF02/SRF08 I2C Ultrasonic Sensor',
    elementTag: 'wink-custom-ultrasonic-i2c',
  }),
});
