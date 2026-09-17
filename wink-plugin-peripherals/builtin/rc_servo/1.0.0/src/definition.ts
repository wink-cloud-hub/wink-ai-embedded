import {
  definePeripheral,
  resolvePluginInstanceId,
  type PeripheralDefinition,
} from '@wink-ai/unisim-ui';
import { resolvePluginIdentity } from '@wink-ai/unisim-sdk';

import CanvasGlyph from './CanvasGlyph.vue';
import { RC_SERVO_TOPOLOGIES, SG90_SIZE } from './variants';
import { createRcServoManifest } from './simulation';

const identity = resolvePluginIdentity(import.meta.url, 'rc_servo', '1.0.0', 'actuator');

const defaultTopology = RC_SERVO_TOPOLOGIES.sg90;

export const servoDefinition: PeripheralDefinition = definePeripheral({
  type: identity.type,
  displayName: 'RC Servo Motor',
  category: 'actuator',
  manifest: createRcServoManifest(),
  size: SG90_SIZE,
  wireColor: '#3b82f6',
  pinsOverlay: defaultTopology.pinsOverlay,
  props: {
    angle: {
      type: 'number',
      default: 90,
      description: 'Current Angle (degrees)',
      range: { min: 0, max: 180, step: 1 },
    },
    invert: {
      type: 'boolean',
      default: false,
      description: 'Invert direction',
    },
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
        rotation: comp.rotation ?? 0,
      };
    },
  },
});

export default servoDefinition;
