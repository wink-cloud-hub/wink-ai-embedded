import { definePeripheral, type PeripheralPropsSchema } from '@wink-ai/unisim-ui';
import { resolvePluginIdentity } from '@wink-ai/unisim';

import CanvasGlyph from './CanvasGlyph.vue';
import WorldWidget from './WorldWidget.vue';
import { ultrasonicPinsOverlay } from './variants';

const identity = resolvePluginIdentity(import.meta.url, 'ultrasonic', '1.0.0', 'sensor');

const ultrasonicProps: PeripheralPropsSchema = {
  variant: {
    type: 'string',
    default: 'default',
    description: 'Sensor variant',
  },
  maxDistanceCm: {
    type: 'number',
    default: 400,
    description: 'Max Distance (cm)',
    range: { min: 2, max: 400, step: 1 },
  },
  minDistanceCm: {
    type: 'number',
    default: 2,
    description: 'Min Distance (cm)',
    range: { min: 0, max: 10, step: 1 },
  },
};

export const ultrasonicDefinition = definePeripheral({
  type: identity.type,
  catalog: { id: identity.type, worldCoupling: 'required' },
  size: { width: 180, height: 100 },
  wireColor: '#eab308',
  pinsOverlay: ultrasonicPinsOverlay,
  props: ultrasonicProps,
  canvas: CanvasGlyph,
  world: WorldWidget,
  ui: {
    worldProps: comp => ({
      pinConnections: comp.pinConnections,
      ...(comp.props.pluginInstanceId
        ? { pluginInstanceId: comp.props.pluginInstanceId as string }
        : {}),
    }),
  },
});

export default ultrasonicDefinition;
