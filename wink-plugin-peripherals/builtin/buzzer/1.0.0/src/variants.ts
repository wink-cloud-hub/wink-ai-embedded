import type { PinsOverlayMap, GeneratedBinderPin } from '@wink-ai/unisim-ui';
import { pinsFromBinderVariant } from '@wink-ai/unisim-ui';

export type BuzzerVariantKey = 'passive_pwm' | 'active_gpio';

export const BUZZER_TOPOLOGY_EQUIVALENCE: Readonly<Record<string, readonly string[]>> = Object.freeze({});

const BUZZER_OVERLAY: Readonly<PinsOverlayMap> = Object.freeze({
  '1': Object.freeze({
    relX: 30,
    relY: 82,
    wireNet: 'primary' as const,
    defaultConnection: null,
    required: true,
  }),
  '2': Object.freeze({
    relX: 34,
    relY: 82,
    wireNet: 'gnd' as const,
    defaultConnection: 'GND' as const,
    required: false,
  }),
});

const BUZZER_PASSIVE_FALLBACK_PINS: readonly GeneratedBinderPin[] = Object.freeze([
  {
    name: '1',
    direction: 'sink',
    signal: 'analog',
    catalogType: 'pwm',
    simRole: 'pwm',
    aliases: ['1', 'sig', 'signal', 'pwm', 'anode', 'pos'],
    required: true,
  },
  {
    name: '2',
    direction: 'ground',
    signal: 'power',
    catalogType: 'power',
    simRole: 'gnd',
    aliases: ['2', 'gnd', 'ground', 'cathode', 'neg'],
    required: false,
  },
]);

const BUZZER_ACTIVE_FALLBACK_PINS: readonly GeneratedBinderPin[] = Object.freeze([
  {
    name: '1',
    direction: 'sink',
    signal: 'digital',
    catalogType: 'gpio',
    simRole: 'signal',
    aliases: ['1', 'sig', 'signal', 'gpio', 'anode', 'pos'],
    required: true,
  },
  {
    name: '2',
    direction: 'ground',
    signal: 'power',
    catalogType: 'power',
    simRole: 'gnd',
    aliases: ['2', 'gnd', 'ground', 'cathode', 'neg'],
    required: false,
  },
]);

function resolveBuzzerPins(variant: BuzzerVariantKey): readonly GeneratedBinderPin[] {
  try {
    const pins = pinsFromBinderVariant('buzzer' as any, variant);
    if (pins && pins.length > 0) return pins;
  } catch {
    // fallback to static pins
  }
  return variant === 'active_gpio' ? BUZZER_ACTIVE_FALLBACK_PINS : BUZZER_PASSIVE_FALLBACK_PINS;
}

export const BUZZER_TOPOLOGIES = Object.freeze({
  passive_pwm: Object.freeze({
    variant: 'passive_pwm' as const,
    getPins: (): readonly GeneratedBinderPin[] => resolveBuzzerPins('passive_pwm'),
    pinsOverlay: BUZZER_OVERLAY,
    defaultAppearanceId: 'buzzer_passive',
  }),
  active_gpio: Object.freeze({
    variant: 'active_gpio' as const,
    getPins: (): readonly GeneratedBinderPin[] => resolveBuzzerPins('active_gpio'),
    pinsOverlay: BUZZER_OVERLAY,
    defaultAppearanceId: 'buzzer_active',
  }),
});

export const BUZZER_APPEARANCES = Object.freeze({
  buzzer_passive: Object.freeze({
    appearanceId: 'buzzer_passive',
    variant: 'passive_pwm' as const,
    displayName: 'Passive Piezo Buzzer (PWM)',
    searchAliases: Object.freeze([
      'buzzer',
      'passive_buzzer',
      'piezo',
      'speaker',
      'pwm',
      'passive_pwm',
      'default',
    ] as const),
  }),
  buzzer_active: Object.freeze({
    appearanceId: 'buzzer_active',
    variant: 'active_gpio' as const,
    displayName: 'Active Buzzer (GPIO)',
    searchAliases: Object.freeze([
      'buzzer',
      'active_buzzer',
      'beeper',
      'alarm',
      'gpio',
      'active_gpio',
    ] as const),
  }),
});
