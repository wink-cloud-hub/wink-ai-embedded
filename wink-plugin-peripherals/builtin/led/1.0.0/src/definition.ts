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
import { LED_TOPOLOGIES } from './variants';

const identity = resolvePluginIdentity(import.meta.url, 'led', '1.0.0', 'output');

function resolveLedLit(comp: CircuitComponentInstance, ctx: SimViewContext): boolean {
  const id = resolvePluginInstanceId(comp, identity.type);
  return ctx.pluginChannels?.[id]?.on === true;
}

const defaultTopology = LED_TOPOLOGIES.default;

const ledProps: PeripheralPropsSchema = {
  variant: {
    type: 'string',
    default: 'default',
    description: 'LED topology variant',
  },
  appearanceId: {
    type: 'string',
    default: 'led_default',
    description: 'Display appearance id',
  },
  color: {
    type: 'string',
    default: 'red',
    description: 'LED color',
    options: ['red', 'green', 'blue', 'yellow', 'white', 'orange', 'purple'],
  },
  brightness: {
    type: 'number',
    default: 1.0,
    description: 'Brightness (0-1)',
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

export const ledDefinition: PeripheralDefinition = definePeripheral({
  type: identity.type,
  size: { width: 50, height: 60 },
  wireColor: '#00ff88',
  pinsOverlay: defaultTopology.pinsOverlay,
  props: ledProps,
  canvas: CanvasGlyph,
  world: WorldWidget,
  ui: {
    canvasProps: (comp, ctx) => ({
      pinConnections: comp.pinConnections,
      color: comp.props.color,
      brightness: comp.props.brightness,
      label: comp.props.label,
      flip: comp.props.flip,
      level: resolveLedLit(comp, ctx),
    }),
    worldProps: (comp, ctx) => ({
      pinConnections: comp.pinConnections,
      color: comp.props.color,
      level: resolveLedLit(comp, ctx),
      brightness: comp.props.brightness,
      label: comp.props.label,
      flip: comp.props.flip,
    }),
  },
});

export default ledDefinition;
