import { expect, test } from 'bun:test';
import {
  BuzzerPlugin,
  buzzerManifest,
  createBuzzerManifest,
  buzzerManifestFactory,
} from '../simulation';
import { LogicStates } from '@wink-ai/unisim';

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
  const scheduledDefers: Array<{ us: bigint; cb: () => void }> = [];

  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
    deferUs: (us: bigint, cb: () => void) => scheduledDefers.push({ us, cb }),
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

  // Multi-edge confirmation should detect 10000Hz!
  const hasSignalEvents = publishes.filter(p => p.ch === 'hasSignal' && p.v === true);
  const freqEvents = publishes.filter(p => p.ch === 'frequency' && p.v === 10000);
  expect(hasSignalEvents.length).toBeGreaterThan(0);
  expect(freqEvents.length).toBeGreaterThan(0);

  // Verify watchdog silence when pulses stop
  publishes.length = 0;
  expect(scheduledDefers.length).toBeGreaterThan(0);
  // Fire the latest silence watchdog callback
  const latestWatchdog = scheduledDefers[scheduledDefers.length - 1];
  latestWatchdog.cb();

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

  expect(publishes).toContainEqual({ ch: 'frequency', v: 5000 });
  publishes.length = 0;

  // After 40ms without pulses, onStep should silence buzzer
  plugin.onStep!(45000n, 1000n);
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: false });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 0 });
});

test('active_gpio: DC on/off works with active-high polarity', () => {
  const plugin = new BuzzerPlugin();
  const publishes: Array<{ ch: string; v: unknown }> = [];
  const ctx = {
    publish: (ch: string, v: unknown) => publishes.push({ ch, v }),
  } as any;

  plugin.onBind(ctx, { '1': 18 }, { variant: 'active_gpio', defaultFreqHz: 2000, activeHigh: true });
  publishes.length = 0;

  // Pin 18 goes HIGH at t=0
  plugin.onPinChange!(18, LogicStates.HIGH, 0n);
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: true });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 2000 });

  publishes.length = 0;

  // Pin 18 goes LOW after 50ms (DC transition, not audio pulse train)
  plugin.onPinChange!(18, LogicStates.LOW, 50000n);
  expect(publishes).toContainEqual({ ch: 'hasSignal', v: false });
  expect(publishes).toContainEqual({ ch: 'frequency', v: 0 });
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
