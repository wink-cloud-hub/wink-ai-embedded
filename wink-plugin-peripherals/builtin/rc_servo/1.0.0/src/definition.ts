import {
  definePeripheral,
  resolvePluginInstanceId,
  type PeripheralDefinition,
} from '@wink-ai/unisim-ui';
import { resolvePluginIdentity } from '@wink-ai/unisim';

import CanvasGlyph from './CanvasGlyph.vue';
import { RC_SERVO_TOPOLOGIES } from './variants';

const identity = resolvePluginIdentity(import.meta.url, 'rc_servo', '1.0.0', 'actuator');

const defaultTopology = RC_SERVO_TOPOLOGIES.sg90;

export const servoDefinition: PeripheralDefinition = definePeripheral({
  type: identity.type,
  size: { width: 80, height: 60 },
  wireColor: '#3b82f6',
  pinsOverlay: defaultTopology.pinsOverlay,
  props: {
    variant: {
      type: 'string',
      default: 'sg90',
      description: 'RC servo topology variant',
    },
    appearanceId: {
      type: 'string',
      default: 'rc_servo_sg90',
      description: 'Display appearance id',
    },
    minAngle: {
      type: 'number',
      default: 0,
      description: 'Min Angle (degrees)',
      range: { min: 0, max: 180, step: 1 },
    },
    maxAngle: {
      type: 'number',
      default: 180,
      description: 'Max Angle (degrees)',
      range: { min: 0, max: 180, step: 1 },
    },
    minPulseMs: {
      type: 'number',
      default: 0.5,
      description: 'Min Pulse Width (ms)',
    },
    maxPulseMs: {
      type: 'number',
      default: 2.5,
      description: 'Max Pulse Width (ms)',
    },
    framePeriodMs: {
      type: 'number',
      default: 20,
      description: 'Frame Period (ms)',
    },
    pwmChannel: {
      type: 'number',
      default: 0,
      description: 'PWM Channel',
      range: { min: 0, max: 15, step: 1 },
    },
  },
  canvas: CanvasGlyph,
  ui: {
    canvasProps: (comp, ctx) => {
      const id = resolvePluginInstanceId(comp, 'rc_servo');
      const pluginAngle = ctx.pluginChannels?.[id]?.angle;
      const angle = typeof pluginAngle === 'number' ? pluginAngle : 90;
      return {
        id: comp.id,
        label: comp.props.label ?? comp.id,
        pwmChannel: comp.props.pwmChannel,
        angle,
      };
    },
  },
});

export default servoDefinition;
