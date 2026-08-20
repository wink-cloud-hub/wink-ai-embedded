import { describe, expect, test } from 'bun:test';
import { UltrasonicPlugin as UltrasonicHcSr04Plugin } from '../simulation';

describe('UltrasonicHcSr04Plugin', () => {
  test('onBind publishes distanceCm=100 and echoUs=0 by default', () => {
    const published: Record<string, unknown> = {};
    const ctx = {
      publish(key: string, val: unknown) {
        published[key] = val;
      },
    } as any;

    const plugin = new UltrasonicHcSr04Plugin();
    plugin.onBind(ctx, { TRIG: 5, ECHO: 18 }, {});

    expect(published).toEqual({
      distanceCm: 100,
      echoUs: 0,
    });
  });

  test('onBind clamps distanceCm when maxDistanceCm property is smaller', () => {
    const published: Record<string, unknown> = {};
    const ctx = {
      publish(key: string, val: unknown) {
        published[key] = val;
      },
    } as any;

    const plugin = new UltrasonicHcSr04Plugin();
    plugin.onBind(ctx, { TRIG: 5, ECHO: 18 }, { maxDistanceCm: 50 });

    expect(published).toEqual({
      distanceCm: 50,
      echoUs: 0,
    });
  });

  test('onPinChange on TRIG rising edge generates ECHO high pulse and defers low', () => {
    const published: Record<string, unknown> = {};
    const pinWrites: Array<{ pin: string; level: boolean }> = [];
    let deferredUs = 0n;
    let deferredCallback: (() => void) | null = null;

    const ctx = {
      publish(key: string, val: unknown) {
        published[key] = val;
      },
      writePin(pin: string, level: boolean) {
        pinWrites.push({ pin, level });
      },
      deferUs(us: bigint, cb: () => void) {
        deferredUs = us;
        deferredCallback = cb;
      },
    } as any;

    const plugin = new UltrasonicHcSr04Plugin();
    plugin.onBind(ctx, { TRIG: 5, ECHO: 18 }, { maxDistanceCm: 100 });
    plugin._distanceCm(50); // 50cm distance -> ~2915us echo pulse

    // Trigger TRIG rising edge
    plugin.onPinChange(5, 1, 0n);

    expect(pinWrites).toContainEqual({ pin: 'ECHO', level: true });
    expect(deferredUs).toBeGreaterThan(0n);
    expect(published.echoUs).toBeGreaterThan(0);

    // Execute deferred callback (ECHO falling edge)
    if (deferredCallback) {
      (deferredCallback as () => void)();
    }
    expect(pinWrites).toContainEqual({ pin: 'ECHO', level: false });
  });
});
