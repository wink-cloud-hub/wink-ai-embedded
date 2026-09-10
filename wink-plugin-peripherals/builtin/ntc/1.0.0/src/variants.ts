import type { PinsOverlayMap, GeneratedBinderPin } from '@wink-ai/unisim-ui';
import { NTC_AUTOGEN_OVERLAY, NTC_AUTOGEN_SIZE } from './variants.generated';

export type NtcVariantKey = 'default';

export const NTC_OVERLAY: Readonly<PinsOverlayMap> = NTC_AUTOGEN_OVERLAY;
export const NTC_SIZE = NTC_AUTOGEN_SIZE;

const NTC_FALLBACK_PINS: readonly GeneratedBinderPin[] = Object.freeze([
  {
    name: 'OUT',
    direction: 'source',
    signal: 'analog',
    catalogType: 'analog',
    simRole: 'signal',
    aliases: ['out', 'AO', 'ao', 'sig', 'SIG'],
    required: true,
  },
  {
    name: 'VCC',
    direction: 'power',
    signal: 'power',
    catalogType: 'power',
    simRole: 'vcc',
    aliases: ['vcc', '5v', '3v3'],
    required: false,
  },
  {
    name: 'GND',
    direction: 'ground',
    signal: 'power',
    catalogType: 'power',
    simRole: 'gnd',
    aliases: ['gnd', 'ground'],
    required: false,
  },
]);

export const NTC_TOPOLOGIES = Object.freeze({
  default: Object.freeze({
    variant: 'default' as const,
    getPins: (): readonly GeneratedBinderPin[] => NTC_FALLBACK_PINS,
    pinsOverlay: NTC_OVERLAY,
    defaultAppearanceId: 'ntc_default',
  }),
});

export const NTC_APPEARANCES = Object.freeze({
  ntc_default: Object.freeze({
    appearanceId: 'ntc_default',
    variant: 'default' as const,
    displayName: 'NTC Temperature Sensor',
    elementTag: 'wokwi-ntc-temperature-sensor',
    searchAliases: Object.freeze(['ntc', 'temp', 'temperature', 'thermistor', 'default'] as const),
  }),
});
