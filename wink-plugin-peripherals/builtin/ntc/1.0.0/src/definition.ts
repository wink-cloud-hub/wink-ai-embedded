import {
  definePeripheral,
  resolvePluginInstanceId,
  type PeripheralPropsSchema,
  type SimViewContext,
} from '@wink-ai/unisim-ui';
import { resolvePluginIdentity } from '@wink-ai/unisim';

import CanvasGlyph from './CanvasGlyph.vue';
import WorldWidget from './WorldWidget.vue';
import { NTC_OVERLAY, NTC_SIZE } from './variants';

const identity = resolvePluginIdentity(import.meta.url, 'ntc', '1.0.0', 'sensor');

const ntcProps: PeripheralPropsSchema = {
  variant: {
    type: 'string',
    default: 'default',
    description: 'Sensor variant',
  },
  temperature: {
    type: 'number',
    default: 25,
    description: 'Measured temperature in °C',
    range: { min: -40, max: 200, step: 1 },
  },
  r25: {
    type: 'number',
    default: 10000,
    description: 'Resistance at 25°C in ohms (e.g. 10000 or 100000)',
  },
  bValue: {
    type: 'number',
    default: 3950,
    description: 'Beta coefficient in Kelvins (e.g. 3950)',
  },
  pullUpResistor: {
    type: 'number',
    default: 10000,
    description: 'Pull-up/divider resistor in ohms (e.g. 10000)',
  },
  minTemp: {
    type: 'number',
    default: -20,
    description: 'Min slider temperature in °C',
  },
  maxTemp: {
    type: 'number',
    default: 120,
    description: 'Max slider temperature in °C',
  },
};

export const ntcDefinition = definePeripheral({
  type: identity.type,
  catalog: { id: identity.type, worldCoupling: 'optional' },
  size: NTC_SIZE,
  wireColor: '#38bdf8',
  pinsOverlay: NTC_OVERLAY,
  props: ntcProps,
  canvas: CanvasGlyph,
  world: WorldWidget,
  ui: {
    // M2-T9-U: live temperature badge — prefer the plugin's published channel
    // (fed by the plant closed loop), fall back to the design-time prop.
    canvasProps: (comp, ctx: SimViewContext) => {
      const id = resolvePluginInstanceId(comp, identity.type);
      const live = ctx.pluginChannels?.[id]?.temperature;
      return {
        temperature: typeof live === 'number' ? live : comp.props.temperature,
      };
    },
    worldProps: comp => ({
      pinConnections: comp.pinConnections,
      temperature: Number(comp.props.temperature ?? 25),
      minTemp: Number(comp.props.minTemp ?? -20),
      maxTemp: Number(comp.props.maxTemp ?? 120),
      ...(comp.props.pluginInstanceId
        ? { pluginInstanceId: comp.props.pluginInstanceId as string }
        : {}),
    }),
  },
});

export default ntcDefinition;
