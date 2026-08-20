import {
  definePeripheral,
  type PeripheralDefinition,
  type PeripheralPropsSchema,
} from '@wink-ai/unisim-ui';
import { resolvePluginIdentity } from '@wink-ai/unisim';

import CanvasGlyph from './CanvasGlyph.vue';
import WorldWidget from './WorldWidget.vue';
import { BUTTON_TOPOLOGIES } from './variants';

const identity = resolvePluginIdentity(import.meta.url, 'button', '1.0.0', 'input');

const defaultTopology = BUTTON_TOPOLOGIES.default;

const buttonProps: PeripheralPropsSchema = {
  variant: {
    type: 'string',
    default: 'default',
    description: 'Button topology variant',
  },
  appearanceId: {
    type: 'string',
    default: 'button_default',
    description: 'Display appearance id',
  },
  color: {
    type: 'string',
    default: 'red',
    description: 'Button color',
    options: ['red', 'green', 'blue', 'yellow', 'white', 'black'],
  },
  label: {
    type: 'string',
    default: '',
    description: 'Label text',
  },
  xray: {
    type: 'boolean',
    default: false,
    description: 'Show internal structure',
  },
  activeLow: {
    type: 'boolean',
    default: true,
    description: 'Active low mode (pull-up)',
  },
};

export const buttonDefinition: PeripheralDefinition = definePeripheral({
  type: identity.type,
  size: { width: 80, height: 60 },
  wireColor: '#38bdf8',
  pinsOverlay: defaultTopology.pinsOverlay,
  props: buttonProps,
  canvas: CanvasGlyph,
  world: WorldWidget,
  ui: {
    canvasProps: comp => ({
      color: comp.props.color,
      label: comp.props.label,
      xray: comp.props.xray,
    }),
    worldProps: comp => ({
      pinConnections: comp.pinConnections,
      color: comp.props.color,
      label: comp.props.label,
      xray: comp.props.xray,
      activeLow: comp.props.activeLow,
      ...(comp.props.pluginInstanceId
        ? { pluginInstanceId: comp.props.pluginInstanceId as string }
        : {}),
    }),
  },
});

export default buttonDefinition;
