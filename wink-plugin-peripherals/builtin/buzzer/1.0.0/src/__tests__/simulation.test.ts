import { expect, test } from 'bun:test';
import {
  BuzzerPlugin,
  buzzerManifest,
  createBuzzerManifest,
  buzzerManifestFactory,
  SILENCE_QUANTA,
} from '../simulation';
import { LogicStates } from '@wink-ai/unisim-sdk';

test('manifest type is buzzer, category is output, has defaultFreqHz and variant', () => {
  expect(buzzerManifest.type).toBe('buzzer');
  expect(buzzerManifest.category).toBe('output');
  expect(buzzerManifest.properties.defaultFreqHz.default).toBe(2000);
  expect(buzzerManifest.properties.variant.default).toBe('passive_pwm');
  expect(buzzerManifest.pins.length).toBe(2);
  expect(buzzerManifest.pins[0].name).toBe('1');
  expect(buzzerManifest.pins[1].name).toBe('2');
});

test('manifest variants: passive_pwm and active_gpio declare expected pin types', () => {
  const passive = createBuzzerManifest('passive_pwm');
  expect(passive.pins[0].catalogType).toBe('pwm');

  const active = createBuzzerManifest('active_gpio');
  expect(active.pins[0].catalogType).toBe('gpio');

  const viaFactory = buzzerManifestFactory('active_gpio');
  expect(viaFactory.pins[0].catalogType).toBe('gpio');
});

test('onBound publishes hasSignal=false, frequency=0, duty=0', () => {
  const plugin = new BuzzerPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];

  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
  } as any;

  plugin.onBind(ctx, { '1': 25, '2': 0 }, { variant: 'passive_pwm', defaultFreqHz: 2000 });

  expect(publishes).toContainEqual({ ch: 'hasSignal', v: false });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 0 });
  expect(publishes).toContainEqual({ ch: 'duty', v: 0 });
});

test('passive_pwm: onDutyChange updates hasSignal, duty, and frequency', () => {
  const plugin = new BuzzerPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
  } as any;

  plugin.onBind(ctx, { '1': 25 }, { variant: 'passive_pwm', defaultFreqHz: 2500, pwmChannel: 0 });
  publishes.length = 0;

  // PWM channel 0 activated at 50% duty
  plugin.onDutyChange(0, 50);
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: true });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 2500 });
  expect(publishes).toContainEqual({ ch: 'duty', v: 50 });

  publishes.length = 0;

  // PWM duty goes to 0 (quiet)
  plugin.onDutyChange(0, 0);
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: false });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 0 });
  expect(publishes).toContainEqual({ ch: 'duty', v: 0 });

  publishes.length = 0;

  // Unrelated PWM channel should be ignored
  plugin.onDutyChange(1, 50);
  expect(publishes.length).toBe(0);
});

test('active_gpio: periodic square wave (10kHz CMS8S78xx style) auto-detects 10000Hz frequency', () => {
  const plugin = new BuzzerPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];

  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
  } as any;

  plugin.onBind(ctx, { '1': 3 }, { variant: 'active_gpio', defaultFreqHz: 2000 });
  publishes.length = 0;

  // Simulate CMS8S78xx 10kHz square wave on pin 3 (half-period 50µs):
  // Edge 0: at 1000µs, HIGH
  plugin.onPinChange!(3, LogicStates.HIGH, 1000n);
  // Edge 1: at 1050µs, LOW
  plugin.onPinChange!(3, LogicStates.LOW, 1050n);
  // Edge 2: at 1100µs, HIGH
  plugin.onPinChange!(3, LogicStates.HIGH, 1100n);
  // Edge 3: at 1150µs, LOW
  plugin.onPinChange!(3, LogicStates.LOW, 1150n);

  // Quantum step calculates frequency: (4 * 1_000_000) / (2 * 200) = 10000Hz
  plugin.onStep!(1200n, 200n);

  // Multi-edge confirmation detects 10000Hz
  const hasSignalEvents = publishes.filter(p => p.ch === 'hasSignal' && p.v === true);
  const freqEvents = publishes.filter(p => p.ch === 'frequency' && p.v === 10000);
  expect(hasSignalEvents.length).toBeGreaterThan(0);
  expect(freqEvents.length).toBeGreaterThan(0);

  // Verify watchdog silence when pulses stop (SILENCE_QUANTA quiet quanta)
  publishes.length = 0;
  for (let i = 0; i < SILENCE_QUANTA; i++) {
    plugin.onStep!(BigInt(2000 + i * 1000), 1000n);
  }

  expect(publishes).toContainEqual({ ch: 'hasSignal', v: false });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 0 });
});

test('onStep fallback silence watchdog works when edge train stops', () => {
  const plugin = new BuzzerPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];

  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
  } as any;

  plugin.onBind(ctx, { '1': 3 }, { variant: 'active_gpio', defaultFreqHz: 2000 });
  publishes.length = 0;

  // Pulse train: 5kHz (half period 100µs)
  plugin.onPinChange!(3, LogicStates.HIGH, 1000n);
  plugin.onPinChange!(3, LogicStates.LOW, 1100n);
  plugin.onPinChange!(3, LogicStates.HIGH, 1200n);
  plugin.onPinChange!(3, LogicStates.LOW, 1300n);

  // Trigger quantum calculation (4 edges in 400µs = 5000Hz)
  plugin.onStep!(1400n, 400n);

  expect(publishes).toContainEqual({ ch: 'frequency', v: 5000 });
  publishes.length = 0;

  // After SILENCE_QUANTA quiet steps (~15ms without pulses), onStep should silence buzzer
  for (let i = 0; i < SILENCE_QUANTA; i++) {
    plugin.onStep!(BigInt(2000 + i * 1000), 1000n);
  }
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: false });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 0 });
});

test('active_gpio: DC on/off works with active-high polarity', () => {
  const plugin = new BuzzerPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
  } as any;

  plugin.onBind(
    ctx,
    { '1': 18 },
    { variant: 'active_gpio', defaultFreqHz: 2000, activeHigh: true },
  );
  publishes.length = 0;

  // Pin 18 goes HIGH at t=0. The first edge on a held level is ambiguous
  // (DC vs pulse train), so DC is confirmed one edge-free quantum later.
  plugin.onPinChange!(18, LogicStates.HIGH, 0n);
  plugin.onStep!(1000n, 1000n);
  expect(publishes.filter(p => p.ch === 'frequency' && p.v === 2000)).toHaveLength(0);
  plugin.onStep!(2000n, 1000n);
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: true });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 2000 });

  publishes.length = 0;

  // Pin 18 goes LOW after 50ms (DC transition, not audio pulse train)
  plugin.onPinChange!(18, LogicStates.LOW, 50000n);
  plugin.onStep!(51000n, 1000n);
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: false });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 0 });
});

test('active_gpio: slow pulse train (< 1 edge/quantum) is not misclassified as DC', () => {
  const plugin = new BuzzerPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
  } as any;

  plugin.onBind(ctx, { '1': 9 }, { variant: 'active_gpio', defaultFreqHz: 2000, activeHigh: true });
  publishes.length = 0;

  // 500Hz square wave toggled every 1000µs: each 1ms quantum carries exactly
  // one edge, so per-quantum edge counting alone cannot distinguish it from a
  // DC latch. The plugin must resolve the real frequency instead of sticking
  // to `defaultFreqHz`.
  let t = 0n;
  for (let i = 0; i < 8; i++) {
    t += 1000n;
    plugin.onPinChange!(9, i % 2 === 0 ? LogicStates.HIGH : LogicStates.LOW, t);
    plugin.onStep!(t, 1000n);
  }

  const positiveFreqs = publishes
    .filter(p => p.ch === 'frequency' && typeof p.v === 'number' && (p.v as number) > 0)
    .map(p => p.v as number);
  expect(positiveFreqs.length).toBeGreaterThan(0);
  expect(positiveFreqs[positiveFreqs.length - 1]).toBeGreaterThanOrEqual(450);
  expect(positiveFreqs[positiveFreqs.length - 1]).toBeLessThanOrEqual(550);
  // Never stuck on the DC default frequency.
  expect(positiveFreqs).not.toContain(2000);
});

test('event methods: _playTone, _stopTone, and _signal', () => {
  const plugin = new BuzzerPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
  } as any;

  plugin.onBind(ctx, { '1': 18 }, { defaultFreqHz: 2000 });
  publishes.length = 0;

  // _playTone with frequency number
  plugin._playTone(440);
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: true });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 440 });
  expect(publishes).toContainEqual({ ch: 'duty', v: 50 });

  publishes.length = 0;

  // _playTone with params object
  plugin._playTone({ freqHz: 880 });
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: true });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 880 });

  publishes.length = 0;

  // _stopTone
  plugin._stopTone();
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: false });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 0 });
  expect(publishes).toContainEqual({ ch: 'duty', v: 0 });

  publishes.length = 0;

  // _signal toggle
  plugin._signal(true);
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: true });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 2000 });

  publishes.length = 0;
  plugin._signal(false);
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: false });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 0 });
});
