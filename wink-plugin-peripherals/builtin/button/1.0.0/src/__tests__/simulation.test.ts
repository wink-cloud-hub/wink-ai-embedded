import { expect, test } from 'bun:test';
import {
  ButtonPlugin as ButtonGpioPlugin,
  buttonManifest as buttonGpioManifest,
  buildPressEdges,
} from '../simulation';

test('manifest type is button and declares activeLow property', () => {
  expect(buttonGpioManifest.type).toBe('button');
  expect(buttonGpioManifest.properties.activeLow.default).toBe(true);
});

test('onBind publishes pressed=false and drives initial pin level', () => {
  const plugin = new ButtonGpioPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const pinWrites: Array<{ pin: string; level: boolean }> = [];
  const pinMapping: Record<string, number> = { '1.l': 10 };

  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
    writePin: (pin: string, level: boolean) => pinWrites.push({ pin, level }),
  } as any;

  plugin.onBind(ctx, pinMapping, { activeLow: true });
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

  plugin.onBind(ctx, { '1.l': 10 }, { activeLow: true });
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

  plugin.onBind(ctx, { '1.l': 10 }, { activeLow: false });
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

test('buildPressEdges emits a deterministic glitch train settling at the press level', () => {
  const edges = buildPressEdges(1_000_000n, 8_000n, 8, 1, 0);
  expect(edges[edges.length - 1]).toEqual({ tUs: 1_008_000n, level: 0 });
  // Strictly monotonic offsets and alternating idle/press levels
  for (let i = 1; i < edges.length; i++) {
    expect(edges[i].tUs > edges[i - 1].tUs).toBe(true);
  }
  expect(edges[0].level).toBe(1);
  expect(edges[1].level).toBe(0);
  // Deterministic across calls (no RNG)
  expect(buildPressEdges(1_000_000n, 8_000n, 8, 1, 0)).toEqual(edges);
});

test('buildPressEdges without bounce is a single immediate press edge', () => {
  expect(buildPressEdges(500n, 0n, 8, 1, 0)).toEqual([{ tUs: 500n, level: 0 }]);
});

test('timing mode press injects one atomic pair with the scheduled release', () => {
  const plugin = new ButtonGpioPlugin();
  const injected: Array<{ pin: string; waveform: any }> = [];
  const ctx = {
    publish: () => {},
    writePin: () => {},
    nowUs: () => 1_000_000n,
    accuracyMode: 'timing',
    injectWaveform: (pin: string, waveform: any) => injected.push({ pin, waveform }),
  } as any;

  plugin.onBind(ctx, {}, { activeLow: true });
  plugin._pressed({ pressed: true, bounceUs: 8000, bounceCount: 8, pressDurationUs: 300000 });

  expect(injected.length).toBe(1);
  expect(injected[0].pin).toBe('1.l');
  const edges = injected[0].waveform.edges;
  expect(edges[edges.length - 1]).toEqual({ tUs: 1_308_000n, level: 1 });
  expect(injected[0].waveform.generation).toBe(1);
});

test('early release re-injects a single release edge above the 30 ms debounce floor', () => {
  const plugin = new ButtonGpioPlugin();
  const injected: Array<{ pin: string; waveform: any }> = [];
  let nowUs = 1_000_000n;
  const ctx = {
    publish: () => {},
    writePin: () => {},
    nowUs: () => nowUs,
    accuracyMode: 'timing',
    injectWaveform: (pin: string, waveform: any) => injected.push({ pin, waveform }),
  } as any;

  plugin.onBind(ctx, {}, { activeLow: true });
  plugin._pressed({ pressed: true, pressDurationUs: 300000 });
  // Release after only 5 ms: must be clamped to press + 30 ms
  nowUs = 1_005_000n;
  plugin._release();

  expect(injected.length).toBe(2);
  expect(injected[1].waveform.edges).toEqual([{ tUs: 1_030_000n, level: 1 }]);
  expect(injected[1].waveform.generation).toBe(2);
});

test('release after the scheduled pair does not inject a second waveform', () => {
  const plugin = new ButtonGpioPlugin();
  const injected: Array<{ pin: string; waveform: any }> = [];
  let nowUs = 2_000_000n;
  const ctx = {
    publish: () => {},
    writePin: () => {},
    nowUs: () => nowUs,
    accuracyMode: 'timing',
    injectWaveform: (pin: string, waveform: any) => injected.push({ pin, waveform }),
  } as any;

  plugin.onBind(ctx, {}, { activeLow: true });
  plugin._pressed({ pressed: true, pressDurationUs: 300000 });
  nowUs = 2_400_000n; // after the scheduled release at 2.3 s
  plugin._release();
  expect(injected.length).toBe(1);
});

test('manifest declares the atomic-pair parameters for SET_PRESSED', () => {
  const params = buttonGpioManifest.events.SET_PRESSED.params;
  expect(params.pressed.required).toBe(true);
  expect(params.pressDurationUs.default).toBe(0);
  expect(params.bounceUs.default).toBe(0);
});

test('press without an explicit duration injects the press edge only (hold semantics)', () => {
  const plugin = new ButtonGpioPlugin();
  const injected: Array<{ pin: string; waveform: any }> = [];
  let nowUs = 1_000_000n;
  const ctx = {
    publish: () => {},
    writePin: () => {},
    nowUs: () => nowUs,
    accuracyMode: 'timing',
    injectWaveform: (pin: string, waveform: any) => injected.push({ pin, waveform }),
  } as any;

  plugin.onBind(ctx, {}, { activeLow: true });
  plugin._pressed(true);
  expect(injected.length).toBe(1);
  expect(injected[0].waveform.edges).toEqual([{ tUs: 1_000_000n, level: 0 }]);

  // Held forever: no scheduled release edge is emitted.
  nowUs = 5_000_000n;
  plugin._pressed(false);
  expect(injected.length).toBe(2);
  expect(injected[1].waveform.edges).toEqual([{ tUs: 5_000_000n, level: 1 }]);
});

test('createMulberry32 produces reproducible deterministic float stream', () => {
  const { createMulberry32 } = require('../simulation');
  const rng1 = createMulberry32(12345);
  const rng2 = createMulberry32(12345);
  for (let i = 0; i < 20; i++) {
    const v1 = rng1();
    const v2 = rng2();
    expect(v1).toBe(v2);
    expect(v1 >= 0 && v1 < 1).toBe(true);
  }
});

test('buildStochasticFrettingEdges emits monotonic chatter edges settling at target level', () => {
  const { createMulberry32, buildStochasticFrettingEdges } = require('../simulation');
  const prng = createMulberry32(42);
  const startUs = 100_000n;
  const chatterUs = 6_000n;
  const edges = buildStochasticFrettingEdges(startUs, chatterUs, prng, 1, 0, true, [500, 5000], 32000);

  expect(edges.length).toBeGreaterThanOrEqual(2);
  // Monotonic timestamps
  for (let i = 1; i < edges.length; i++) {
    expect(edges[i].tUs >= edges[i - 1].tUs).toBe(true);
  }
  // Settles at pressLevel (0)
  expect(edges[edges.length - 1]).toEqual({ tUs: startUs + chatterUs, level: 0 });
});

test('stochastic_fretting injects waveform with non-symmetric chatter and scheduled release', () => {
  const plugin = new ButtonGpioPlugin();
  const injected: Array<{ pin: string; waveform: any }> = [];
  const nowUs = 2_000_000n;
  const ctx = {
    publish: () => {},
    writePin: () => {},
    nowUs: () => nowUs,
    accuracyMode: 'timing',
    injectWaveform: (pin: string, waveform: any) => injected.push({ pin, waveform }),
  } as any;

  plugin.onBind(ctx, {}, { activeLow: true });
  plugin._pressed({
    pressed: true,
    bounceModel: 'stochastic_fretting',
    chatterDurationRangeUs: [2000, 8000],
    pressDurationUs: 50000,
    seed: 9999,
  });

  expect(injected.length).toBe(1);
  const edges = injected[0].waveform.edges;
  expect(edges.length).toBeGreaterThan(3);
  // Final edge of release must settle at idleLevel (1)
  expect(edges[edges.length - 1].level).toBe(1);
});

