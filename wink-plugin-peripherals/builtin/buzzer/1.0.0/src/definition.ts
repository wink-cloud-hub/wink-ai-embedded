import {
  definePeripheral,
  resolvePluginInstanceId,
  type CircuitComponentInstance,
  type PeripheralDefinition,
  type PeripheralPropsSchema,
  type SimViewContext,
} from '@wink-ai/unisim-ui';
import { resolvePluginIdentity } from '@wink-ai/unisim-sdk';

import CanvasGlyph from './CanvasGlyph.vue';
import WorldWidget from './WorldWidget.vue';
import { BUZZER_TOPOLOGIES, BUZZER_SIZE } from './variants';

const identity = resolvePluginIdentity(import.meta.url, 'buzzer', '1.0.0', 'output');

function resolveBuzzerChannel(comp: CircuitComponentInstance, ctx: SimViewContext) {
  const id = resolvePluginInstanceId(comp, identity.type);
  const ch =
    ctx.pluginChannels?.[comp.id] ??
    ctx.pluginChannels?.[id] ??
    ctx.pluginChannels?.[`${identity.type}:0`] ??
    ctx.pluginChannels?.[identity.type] ??
    {};
  return ch;
}

const defaultTopology = BUZZER_TOPOLOGIES.passive_pwm;

const buzzerProps: PeripheralPropsSchema = {
  variant: {
    type: 'string',
    default: 'passive_pwm',
    options: ['passive_pwm', 'active_gpio'],
    description: 'Buzzer topology variant',
  },
  appearanceId: {
    type: 'string',
    default: 'buzzer_passive',
    description: 'Display appearance id',
  },
  defaultFreqHz: {
    type: 'number',
    default: 2000,
    range: { min: 20, max: 8000, step: 10 },
    description: 'Default sounding frequency in Hz',
  },
  pwmChannel: {
    type: 'number',
    default: 0,
    range: { min: 0, max: 15, step: 1 },
    description: 'PWM channel (for passive_pwm)',
  },
  activeHigh: {
    type: 'boolean',
    default: true,
    description: 'Trigger polarity (active-high vs active-low)',
  },
  label: {
    type: 'string',
    default: 'Buzzer',
    description: 'Label text',
  },
};

export const buzzerDefinition: PeripheralDefinition = definePeripheral({
  type: identity.type,
  size: BUZZER_SIZE,
  wireColor: '#f59e0b',
  pinsOverlay: defaultTopology.pinsOverlay,
  props: buzzerProps,
  canvas: CanvasGlyph,
  world: WorldWidget,
  ui: {
    canvasProps: (comp, ctx) => {
      const ch = resolveBuzzerChannel(comp, ctx);
      const isRunning = ctx.isRunning !== false;
      return {
        id: comp.id,
        hasSignal: isRunning && Boolean(ch.hasSignal),
        frequency: typeof ch.frequency === 'number' ? ch.frequency : 0,
        duty: typeof ch.duty === 'number' ? ch.duty : 0,
        label: comp.props?.label ?? 'Buzzer',
        pinConnections: comp.pinConnections,
      };
    },
    worldProps: (comp, ctx) => {
      const ch = resolveBuzzerChannel(comp, ctx);
      const isRunning = ctx.isRunning !== false;
      return {
        id: comp.id,
        hasSignal: isRunning && Boolean(ch.hasSignal),
        frequency: typeof ch.frequency === 'number' ? ch.frequency : 0,
        duty: typeof ch.duty === 'number' ? ch.duty : 0,
        label: comp.props?.label ?? 'Buzzer',
        variant: comp.props?.variant ?? 'passive_pwm',
      };
    },
  },
});

export default buzzerDefinition;
