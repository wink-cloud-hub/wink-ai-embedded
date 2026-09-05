import {
  definePeripheral,
  resolvePluginInstanceId,
  type CircuitComponentInstance,
  type PeripheralDefinition,
  type PeripheralPropsSchema,
  type SimViewContext,
} from '@wink-ai/unisim-ui';
import { resolvePluginIdentity } from '@wink-ai/unisim';

import CanvasGlyph from './CanvasGlyph.vue';
import WorldWidget from './WorldWidget.vue';
import {
  SEG_TOPOLOGIES,
  resolveSegVariant,
  SEG_VARIANT_DIGITS,
  type SegVariantKey,
} from './variants';

const identity = resolvePluginIdentity(import.meta.url, 'seg_display', '1.0.0', 'display');

function resolveChannel(comp: CircuitComponentInstance, ctx: SimViewContext, channel: string): unknown {
  const id = resolvePluginInstanceId(comp, identity.type);
  return (
    ctx.pluginChannels?.[comp.id]?.[channel] ??
    ctx.pluginChannels?.[id]?.[channel] ??
    ctx.pluginChannels?.[`${identity.type}:0`]?.[channel] ??
    ctx.pluginChannels?.[identity.type]?.[channel]
  );
}

function resolveBright(comp: CircuitComponentInstance, ctx: SimViewContext): Uint8Array | null {
  const val = resolveChannel(comp, ctx, 'bright');
  if (val instanceof Uint8Array) return val;
  if (Array.isArray(val)) return new Uint8Array(val);
  if (val && typeof val === 'object') {
    return new Uint8Array(Object.values(val) as number[]);
  }
  return null;
}

function resolveSegMask(comp: CircuitComponentInstance, ctx: SimViewContext): number[] {
  const val = resolveChannel(comp, ctx, 'segMask');
  if (Array.isArray(val)) return val;
  if (typeof val === 'string') {
    try {
      const parsed = JSON.parse(val);
      if (Array.isArray(parsed)) return parsed;
    } catch {
      // Fall through to empty array
    }
  }
  return [];
}

function resolveText(comp: CircuitComponentInstance, ctx: SimViewContext): string {
  const val = resolveChannel(comp, ctx, 'text');
  return typeof val === 'string' ? val : '';
}

function resolveNDigits(comp: CircuitComponentInstance): number {
  const key = resolveSegVariant(comp.props?.variant as string | undefined);
  return SEG_VARIANT_DIGITS[key] ?? 8;
}

function resolveValues(comp: CircuitComponentInstance, ctx: SimViewContext): number[] {
  const nDigits = resolveNDigits(comp);
  const total = nDigits * 8;
  const bright = resolveBright(comp, ctx);
  if (bright && bright.length > 0) {
    const vals: number[] = [];
    for (let i = 0; i < total; i++) {
      const b = i < bright.length ? bright[i] : 0;
      vals.push(b >= 30 ? 1 : 0);
    }
    return vals;
  }
  const segMask = resolveSegMask(comp, ctx);
  if (segMask && segMask.length > 0) {
    const vals: number[] = [];
    for (let d = 0; d < nDigits; d++) {
      const m = segMask[d] ?? 0;
      for (let s = 0; s < 8; s++) {
        vals.push((m >> s) & 1);
      }
    }
    return vals;
  }
  return new Array(total).fill(0);
}

const defaultTopology = SEG_TOPOLOGIES.direct_gpio_8d;

export const segDisplayProps: PeripheralPropsSchema = {
  variant: {
    type: 'string',
    default: 'direct_gpio_8d',
    description: 'Segment display topology variant',
    options: ['direct_gpio_8d', 'direct_gpio_4d', 'direct_gpio_2d', 'direct_gpio_1d'],
  },
  appearanceId: {
    type: 'string',
    default: 'seg_display_8',
    description: 'Display appearance id',
  },
  segActiveLevel: {
    type: 'string',
    default: 'high',
    description: 'Active level for segment pins (high/low)',
    options: ['high', 'low'],
  },
  digitActiveLevel: {
    type: 'string',
    default: 'low',
    description: 'Active level for digit select pins (high/low)',
    options: ['high', 'low'],
  },
  commonAnode: {
    type: 'boolean',
    default: false,
    description: 'Common anode preset (sets seg=low, dig=high if levels not explicitly overridden)',
  },
  color: {
    type: 'string',
    default: 'red',
    description: 'LED segment color',
    options: ['red', 'green', 'blue', 'yellow', 'white', 'orange', 'purple'],
  },
  glow: {
    type: 'boolean',
    default: true,
    description: 'Enable phosphor glow effect',
  },
  brightness: {
    type: 'number',
    default: 1.0,
    description: 'Overall brightness multiplier (0.0 - 1.0)',
  },
  label: {
    type: 'string',
    default: '',
    description: 'Label text',
  },
  flip: {
    type: 'boolean',
    default: false,
    description: 'Flip orientation',
  },
};

export const segDisplayDefinition: PeripheralDefinition = definePeripheral({
  type: identity.type,
  size: { width: 210, height: 96 },
  wireColor: '#ff0055',
  pinsOverlay: defaultTopology.pinsOverlay,
  props: segDisplayProps,
  canvas: CanvasGlyph,
  world: WorldWidget,
  ui: {
    canvasProps: (comp, ctx) => ({
      pinConnections: comp.pinConnections,
      variant: comp.props.variant,
      color: comp.props.color,
      brightness: comp.props.brightness,
      glow: comp.props.glow,
      label: comp.props.label,
      flip: comp.props.flip,
      bright: resolveBright(comp, ctx),
      segMask: resolveSegMask(comp, ctx),
      text: resolveText(comp, ctx),
      nDigits: resolveNDigits(comp),
      values: resolveValues(comp, ctx),
    }),
    worldProps: (comp, ctx) => ({
      pinConnections: comp.pinConnections,
      variant: comp.props.variant,
      color: comp.props.color,
      brightness: comp.props.brightness,
      label: comp.props.label,
      text: resolveText(comp, ctx),
      bright: resolveBright(comp, ctx),
      segMask: resolveSegMask(comp, ctx),
      nDigits: resolveNDigits(comp),
      values: resolveValues(comp, ctx),
    }),
  },
});

export default segDisplayDefinition;
export * from './variants';
