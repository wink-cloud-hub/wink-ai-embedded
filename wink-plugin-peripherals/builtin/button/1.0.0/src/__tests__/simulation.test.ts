import { expect, test } from 'bun:test';
import {
  ButtonPlugin as ButtonGpioPlugin,
  buttonManifest as buttonGpioManifest,
} from '../simulation';

test('manifest type is button and declares activeLow property', () => {
  expect(buttonGpioManifest.type).toBe('button');
  expect(buttonGpioManifest.properties.activeLow.default).toBe(true);
});

test('onBind publishes pressed=false and drives initial pin level', () => {
  const plugin = new ButtonGpioPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const pinWrites: Array<{ pin: string; level: boolean }> = [];
  const pinMapping: Record<string, number> = {};

  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
    writePin: (pin: string, level: boolean) => pinWrites.push({ pin, level }),
  } as any;

  plugin.onBind(ctx, pinMapping, { pin: 10, activeLow: true });
  expect(publishes).toContainEqual({ ch: 'pressed', v: false });
  expect(pinMapping['1.l']).toBe(10);
  expect(pinWrites).toEqual([{ pin: '1.l', level: true }]);
});

test('activeLow: press drives GPIO LOW and release restores idle HIGH', () => {
  const plugin = new ButtonGpioPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const pinWrites: Array<{ pin: string; level: boolean }> = [];

  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
    writePin: (pin: string, level: boolean) => pinWrites.push({ pin, level }),
  } as any;

  plugin.onBind(ctx, {}, { pin: 10, activeLow: true });
  publishes.length = 0;
  pinWrites.length = 0;

  plugin._pressed(true);
  expect(publishes).toEqual([{ ch: 'pressed', v: true }]);
  expect(pinWrites).toEqual([{ pin: '1.l', level: false }]);

  plugin._release();
  expect(publishes).toContainEqual({ ch: 'pressed', v: false });
  expect(pinWrites).toContainEqual({ pin: '1.l', level: true });
});

test('activeHigh: press drives GPIO HIGH and release restores idle LOW', () => {
  const plugin = new ButtonGpioPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const pinWrites: Array<{ pin: string; level: boolean }> = [];

  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
    writePin: (pin: string, level: boolean) => pinWrites.push({ pin, level }),
  } as any;

  plugin.onBind(ctx, {}, { pin: 10, activeLow: false });
  publishes.length = 0;
  pinWrites.length = 0;

  plugin._pressed(true);
  expect(publishes).toEqual([{ ch: 'pressed', v: true }]);
  expect(pinWrites).toEqual([{ pin: '1.l', level: true }]);

  plugin._release();
  expect(publishes).toContainEqual({ ch: 'pressed', v: false });
  expect(pinWrites).toContainEqual({ pin: '1.l', level: false });
});

test('long-press hold: _press maintains LOW indefinitely without spontaneous bounce', () => {
  const plugin = new ButtonGpioPlugin();
  const pinWrites: Array<{ pin: string; level: boolean }> = [];
  const ctx = {
    publish: () => {},
    writePin: (pin: string, level: boolean) => pinWrites.push({ pin, level }),
  } as any;

  plugin.onBind(ctx, { '1.l': 10 }, { activeLow: true });
  pinWrites.length = 0;

  // Press down
  plugin._press();
  expect(pinWrites).toEqual([{ pin: '1.l', level: false }]);

  // Simulate long duration: verify no spontaneous release
  for (let t = 0; t < 100; t++) {
    // Nothing should add to pinWrites during hold
    expect(pinWrites.length).toBe(1);
    expect(pinWrites[0].level).toBe(false);
  }

  // Release
  plugin._release();
  expect(pinWrites.length).toBe(2);
  expect(pinWrites[1]).toEqual({ pin: '1.l', level: true });
});

test('SET_PRESSED event handler supports boolean and object argument', () => {
  const plugin = new ButtonGpioPlugin();
  const pinWrites: Array<{ pin: string; level: boolean }> = [];
  const ctx = {
    publish: () => {},
    writePin: (pin: string, level: boolean) => pinWrites.push({ pin, level }),
  } as any;

  plugin.onBind(ctx, {}, { activeLow: true });
  pinWrites.length = 0;

  // Object arg
  plugin._pressed({ pressed: true });
  expect(pinWrites.at(-1)).toEqual({ pin: '1.l', level: false });

  plugin._pressed({ pressed: false });
  expect(pinWrites.at(-1)).toEqual({ pin: '1.l', level: true });

  // Boolean arg
  plugin._pressed(true);
  expect(pinWrites.at(-1)).toEqual({ pin: '1.l', level: false });

  plugin._pressed(false);
  expect(pinWrites.at(-1)).toEqual({ pin: '1.l', level: true });
});

test('repeated presses produce clean alternating level transitions', () => {
  const plugin = new ButtonGpioPlugin();
  const pinWrites: Array<{ pin: string; level: boolean }> = [];
  const ctx = {
    publish: () => {},
    writePin: (pin: string, level: boolean) => pinWrites.push({ pin, level }),
  } as any;

  plugin.onBind(ctx, {}, { activeLow: true });
  pinWrites.length = 0;

  for (let i = 0; i < 5; i++) {
    plugin._press();
    plugin._release();
  }

  expect(pinWrites.length).toBe(10);
  for (let i = 0; i < 10; i++) {
    expect(pinWrites[i].level).toBe(i % 2 === 0 ? false : true);
  }
});

