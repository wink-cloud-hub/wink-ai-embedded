import { expect, test } from 'bun:test';
import { LedPlugin as LedGpioPlugin, ledManifest as ledGpioManifest } from '../simulation';
import { LogicStates } from '@wink-ai/unisim';

test('manifest type is led and declares activeHigh property', () => {
  expect(ledGpioManifest.type).toBe('led');
  expect(ledGpioManifest.properties.activeHigh.default).toBe(true);
});

test('onBind publishes on=false and maps anode pin', () => {
  const plugin = new LedGpioPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const pinMapping: Record<string, number> = { gpio: 2 };

  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
  } as any;

  plugin.onBind(ctx, pinMapping, { activeHigh: true });
  expect(publishes).toContainEqual({ ch: 'on', v: false });
  expect(pinMapping['A']).toBe(2);
});

test('onBind does not writePin anode', () => {
  const writes: unknown[] = [];
  const plugin = new LedGpioPlugin();
  plugin.onBind(
    {
      publish() {},
      writePin(...args: unknown[]) {
        writes.push(args);
      },
    } as any,
    { gpio: 2 },
    { activeHigh: true },
  );
  expect(writes).toEqual([]);
});

test('onPinChange updates state channel based on activeHigh property', () => {
  const plugin = new LedGpioPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
  } as any;

  plugin.onBind(ctx, { gpio: 2 }, { activeHigh: true });
  publishes.length = 0;

  // Pin goes HIGH -> LED lit
  plugin.onPinChange(2, LogicStates.HIGH, 100n);
  expect(publishes).toEqual([{ ch: 'on', v: true }]);

  // Pin goes LOW -> LED unlit
  plugin.onPinChange(2, LogicStates.LOW, 200n);
  expect(publishes).toContainEqual({ ch: 'on', v: false });
});

test('activeLow (activeHigh=false): pin LOW -> on=true', () => {
  const plugin = new LedGpioPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
  } as any;

  plugin.onBind(ctx, { gpio: 2 }, { activeHigh: false });
  publishes.length = 0;

  plugin.onPinChange(2, LogicStates.LOW, 100n);
  expect(publishes).toEqual([{ ch: 'on', v: true }]);

  plugin.onPinChange(2, LogicStates.HIGH, 200n);
  expect(publishes).toContainEqual({ ch: 'on', v: false });
});
