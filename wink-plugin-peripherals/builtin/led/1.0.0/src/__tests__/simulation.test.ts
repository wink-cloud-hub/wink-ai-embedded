import { expect, test } from 'bun:test';
import { LedPlugin as LedGpioPlugin, ledManifest as ledGpioManifest } from '../simulation';
import { LogicStates } from '@wink-ai/unisim-sdk';

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

test('INJECT_FAULT CONTACT_WELDED: contactClosed stays true even when pin goes LOW', () => {
  const plugin = new LedGpioPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
  } as any;

  plugin.onBind(ctx, { gpio: 2 }, { activeHigh: true });
  publishes.length = 0;

  // Turn pin ON
  plugin.onPinChange(2, LogicStates.HIGH, 100n);
  expect(publishes).toContainEqual({ ch: 'on', v: true });

  // Inject CONTACT_WELDED fault
  plugin._injectFault({ faultType: 'CONTACT_WELDED' });
  expect(publishes).toContainEqual({ ch: 'welded', v: true });
  expect(publishes).toContainEqual({ ch: 'contactClosed', v: true });

  publishes.length = 0;

  // Pin goes LOW (MCU drives heater OFF), but contact remains physically closed!
  plugin.onPinChange(2, LogicStates.LOW, 200n);
  expect(publishes).toContainEqual({ ch: 'on', v: false }); // MCU commanded OFF
  expect(publishes).toContainEqual({ ch: 'contactClosed', v: true }); // Physical contact still closed!
  expect(publishes).toContainEqual({ ch: 'welded', v: true });

  // Clear fault
  plugin._clearFault();
  expect(publishes).toContainEqual({ ch: 'welded', v: false });
  expect(publishes).toContainEqual({ ch: 'contactClosed', v: false }); // Returns to pin state (LOW)
});
