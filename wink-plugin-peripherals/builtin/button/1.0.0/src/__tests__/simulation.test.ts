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
