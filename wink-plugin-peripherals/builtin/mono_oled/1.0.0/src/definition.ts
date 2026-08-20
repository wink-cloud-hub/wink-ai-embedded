import {
  definePeripheral,
  mapGeneratedBinderPinsToUnified,
  mergePinsWithOverlay,
  type DisplayFrame,
  type CircuitComponentInstance,
  type PeripheralDefinition,
  type PeripheralPropsSchema,
  type SimViewContext,
} from '@wink-ai/unisim-ui';
import { resolvePluginIdentity } from '@wink-ai/unisim';

const OLED_WIDTH = 128;
const OLED_HEIGHT = 64;

import CanvasGlyph from './CanvasGlyph.vue';
import WorldWidget from './WorldWidget.vue';
import { MONO_OLED_TOPOLOGIES } from './variants';

const identity = resolvePluginIdentity(import.meta.url, 'mono_oled', '1.0.0', 'display');

type DisplayFrameTarget =
  | string
  | Pick<CircuitComponentInstance, 'id' | 'type' | 'props'>
  | undefined;

const OLED_FRAME_KINDS = new Set(['ssd1306_fb', 'framebuffer']);
const PLUGIN_ID_KEYS = [
  'displayFrameInstanceId',
  'pluginInstanceId',
  'runtimeInstanceId',
  'simulationInstanceId',
  'pluginId',
] as const;
const PLUGIN_INDEX_KEYS = ['displayIndex', 'pluginIndex', 'instanceIndex'] as const;

function isOledFrame(frame: DisplayFrame): boolean {
  return (frame as any).kind ? OLED_FRAME_KINDS.has((frame as any).kind) : true;
}

function pluginTypeForComponentType(type: string): string {
  return type || 'mono_oled';
}

function getStringProp(props: Record<string, unknown>, keys: readonly string[]): string | null {
  for (const key of keys) {
    const value = props[key];
    if (typeof value === 'string' && value.trim()) return value;
  }
  return null;
}

function getNumberProp(props: Record<string, unknown>, keys: readonly string[]): number | null {
  for (const key of keys) {
    const value = props[key];
    if (typeof value === 'number' && Number.isInteger(value) && value >= 0) return value;
    if (typeof value === 'string' && /^\d+$/.test(value)) return Number(value);
  }
  return null;
}

function targetIds(target: DisplayFrameTarget): string[] {
  if (!target) return [];
  if (typeof target === 'string') return [target];

  const ids: string[] = [];
  const props = (target.props || {}) as Record<string, unknown>;
  const pluginId = getStringProp(props, PLUGIN_ID_KEYS);
  if (pluginId) ids.push(pluginId);

  const pluginIndex = getNumberProp(props, PLUGIN_INDEX_KEYS);
  if (pluginIndex !== null) ids.push(`${pluginTypeForComponentType(target.type)}:${pluginIndex}`);

  ids.push(target.id);
  return Array.from(new Set(ids));
}

export function pickDisplayFrame(
  ctx: SimViewContext,
  target?: DisplayFrameTarget,
): DisplayFrame | null {
  const frames = ctx.displayFrames ?? [];
  const ids = targetIds(target);
  for (const id of ids) {
    const frame = ctx.getDisplayFrame?.(id) ?? frames.find(f => f.instanceId === id) ?? null;
    if (frame) return frame;
  }

  const oledFrames = frames.filter(isOledFrame);
  if (!target || ids.length === 0) return oledFrames[0] ?? frames[0] ?? null;
  return oledFrames.length === 1 ? oledFrames[0] : null;
}

function pickOledFrame(ctx: SimViewContext, target?: DisplayFrameTarget): Uint8Array | null {
  const frame = pickDisplayFrame(ctx, target);
  return frame?.fb ?? null;
}

const DEFAULT_VARIANT = 'ssd1306_i2c' as const;
const defaultTopology = MONO_OLED_TOPOLOGIES[DEFAULT_VARIANT];

const oledProps: PeripheralPropsSchema = {
  variant: {
    type: 'string',
    default: DEFAULT_VARIANT,
    description: 'OLED topology variant (ssd1306_i2c | ssd1306_spi)',
  },
  panel_ic: {
    type: 'string',
    default: 'ssd1306',
    description: 'OLED controller IC (ssd1306 | sh1106)',
  },
  pluginInstanceId: {
    type: 'string',
    default: '',
    description: 'Simulation plugin instance id, e.g. ssd1306_i2c:0',
    advanced: true,
  },
  pluginIndex: {
    type: 'number',
    default: -1,
    description: 'Simulation plugin instance index; -1 means unspecified',
    advanced: true,
  },
};

const oledPinsOverlay = defaultTopology.pinsOverlay;

export const oledDefinition: PeripheralDefinition = definePeripheral({
  type: identity.type,
  size: { width: OLED_WIDTH, height: OLED_HEIGHT },
  wireColor: '#a855f7',
  pinsOverlay: oledPinsOverlay,
  props: oledProps,
  canvas: CanvasGlyph,
  world: WorldWidget,
  ui: {
    canvasProps: (comp, ctx) => {
      const displayFrame = pickDisplayFrame(ctx, comp);
      return {
        displayFrame,
        framebuffer: pickOledFrame(ctx, comp),
      };
    },
    worldProps: (comp, ctx) => {
      const displayFrame = pickDisplayFrame(ctx, comp);
      return {
        pinConnections: comp.pinConnections,
        displayFrame,
        framebuffer: pickOledFrame(ctx, comp),
      };
    },
  },
});

export default oledDefinition;
