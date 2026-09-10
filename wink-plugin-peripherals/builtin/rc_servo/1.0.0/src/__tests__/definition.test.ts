import { describe, expect, it } from 'bun:test';
import { servoDefinition } from '../definition';
import type { CircuitComponentInstance } from '@wink-ai/unisim-ui';

describe('rc_servo peripheral definition', () => {
  it('has correct type and metadata', () => {
    const def = servoDefinition;
    expect(def).toBeDefined();
    expect(def?.displayName).toBe('RC Servo Motor');
    expect(def?.category).toBe('actuator');
    expect(def?.catalog?.id).toBe('rc_servo');
    expect(def?.pins.map(p => p.name)).toEqual(['PWM', 'VCC', 'GND']);
  });

  it('does not declare redundant actuatorObserve in frontend definition (SSOT in unisim binder)', () => {
    const def = servoDefinition;
    expect(def?.actuatorObserve).toBeUndefined();
  });

  it('does not declare redundant simulation.observe in frontend definition (SSOT in unisim binder)', () => {
    const def:any = servoDefinition;
    expect(def?.simulation?.observe).toBeUndefined();
  });

  it('declares props schema with angle and invert defaults', () => {
    const def = servoDefinition;
    expect(def?.props.angle?.default).toBe(90);
    expect(def?.props.invert?.default).toBe(false);
  });

  it('has precise 1:1 box-model dimensions and accurate pin coordinates matching Wokwi SVG', () => {
    const def = servoDefinition;
    expect(def.size).toEqual({ width: 171, height: 120 });

    const pinMap = Object.fromEntries(def.pins.map(p => [p.name, p]));
    expect(pinMap.GND?.relX).toBe(0);
    expect(pinMap.GND?.relY).toBe(50);
    expect(pinMap.GND?.wireNet).toBe('gnd');

    expect(pinMap.VCC?.relX).toBe(0);
    expect(pinMap.VCC?.relY).toBe(60);
    expect(pinMap.VCC?.wireNet).toBe('vcc');

    expect(pinMap.PWM?.relX).toBe(0);
    expect(pinMap.PWM?.relY).toBe(69);
    expect(pinMap.PWM?.wireNet).toBe('primary');

    // 验证 180° 旋转后的刚体几何变换（avoidance_car 部署场景）
    // 旋转 180° 公式：newX = width - relX; newY = height - relY
    const rot180GndX = def.size.width - pinMap.GND.relX;
    const rot180GndY = def.size.height - pinMap.GND.relY;
    expect(rot180GndX).toBe(171);
    expect(rot180GndY).toBe(70);

    const rot180PwmX = def.size.width - pinMap.PWM.relX;
    const rot180PwmY = def.size.height - pinMap.PWM.relY;
    expect(rot180PwmX).toBe(171);
    expect(rot180PwmY).toBe(51);

    // 严格确保所有引脚旋转后都在 [0, W] 和 [0, H] 之内，绝对不越界飞出
    for (const pin of def.pins) {
      const rx = def.size.width - pin.relX;
      const ry = def.size.height - pin.relY;
      expect(rx).toBeGreaterThanOrEqual(0);
      expect(rx).toBeLessThanOrEqual(def.size.width);
      expect(ry).toBeGreaterThanOrEqual(0);
      expect(ry).toBeLessThanOrEqual(def.size.height);
    }
  });
});
