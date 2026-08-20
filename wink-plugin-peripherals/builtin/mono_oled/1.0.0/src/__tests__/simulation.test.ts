import { describe, expect, test } from 'bun:test';
import { I2CBus, PinArbiter, VirtualClock, PluginContext } from '@wink-ai/unisim';

import { MonoOledPlugin } from '../simulation';

function createPlugin() {
  const clock = new VirtualClock();
  const bus = new I2CBus();
  bus.setTimeSource({
    nowUs: () => Number(clock.getUs()),
    advanceUs: delta => clock.advance(BigInt(Math.ceil(delta))),
  });
  const publishes: Array<{ channel: string; value: unknown }> = [];
  const ctx = new PluginContext(
    'mono_oled:0',
    new PinArbiter(),
    {},
    clock,
    undefined,
    (_id, channel, value) => publishes.push({ channel, value }),
    undefined,
    bus,
  );
  const plugin = new MonoOledPlugin();
  plugin.onBind(ctx, {}, {});
  return { bus, clock, plugin, publishes };
}

describe('OledSsd1306Plugin', () => {
  test('parses addressing commands and publishes a framebuffer snapshot after data transfer', () => {
    const { bus, publishes } = createPlugin();

    expect(
      bus.transfer(
        0,
        0x3c,
        new Uint8Array([0x00, 0x20, 0x00, 0x21, 0x02, 0x03, 0x22, 0x01, 0x01]),
        new Uint8Array(),
      ),
    ).toBe(true);
    expect(bus.transfer(0, 0x3c, new Uint8Array([0x40, 0xaa, 0xbb]), new Uint8Array())).toBe(true);

    expect(publishes).toContainEqual({ channel: 'displayKind', value: 'ssd1306_fb' });
    const framebuffer = publishes.filter(({ channel }) => channel === 'fb').at(-1)?.value;
    expect(framebuffer).toEqual(expect.any(Uint8Array));
    expect((framebuffer as Uint8Array).slice(128 + 2, 128 + 4)).toEqual(
      new Uint8Array([0xaa, 0xbb]),
    );
    expect(publishes.map(({ channel }) => channel)).toEqual(
      expect.arrayContaining(['width', 'height', 'fb', 'colorFormat', 'displayKind']),
    );
  });

  test('defers a second dirty framebuffer publish until the 16ms throttle expires', () => {
    const { bus, clock, publishes } = createPlugin();

    bus.transfer(0, 0x3c, new Uint8Array([0x40, 0x01]), new Uint8Array());
    const firstPublishCount = publishes.filter(({ channel }) => channel === 'fb').length;

    bus.transfer(0, 0x3c, new Uint8Array([0x40, 0x02]), new Uint8Array());
    expect(publishes.filter(({ channel }) => channel === 'fb')).toHaveLength(firstPublishCount);

    clock.advance(16_000n);
    expect(publishes.filter(({ channel }) => channel === 'fb')).toHaveLength(firstPublishCount + 1);
  });

  test('parses SH1106 page addressing commands correctly', () => {
    const { bus, publishes } = createPlugin();

    /* SH1106 Page 1 command: 0xB1, col lower 0x02, col upper 0x10 */
    expect(bus.transfer(0, 0x3c, new Uint8Array([0x00, 0xb1, 0x02, 0x10]), new Uint8Array())).toBe(
      true,
    );
    expect(bus.transfer(0, 0x3c, new Uint8Array([0x40, 0xcc, 0xdd]), new Uint8Array())).toBe(true);

    const framebuffer = publishes.filter(({ channel }) => channel === 'fb').at(-1)?.value;
    expect(framebuffer).toEqual(expect.any(Uint8Array));
    expect((framebuffer as Uint8Array).slice(128 + 2, 128 + 4)).toEqual(
      new Uint8Array([0xcc, 0xdd]),
    );
  });
});
