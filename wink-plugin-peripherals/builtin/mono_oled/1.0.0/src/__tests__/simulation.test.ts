import { describe, expect, test } from 'bun:test';
import { createPluginStubHost } from '@wink-ai/unisim-sdk';

import { MonoOledPlugin } from '../simulation';

/**
 * Sanctioned test harness: wires a virtual clock + pin arbiter + I2C bus
 * exactly like headless, without reaching into @wink-ai/unisim/core internals
 * (those subpaths are not part of the public exports map).
 */
function createPlugin() {
  const host = createPluginStubHost();
  const binding = host.bind(MonoOledPlugin, { instanceId: 'mono_oled:0' });
  return { host, plugin: binding.instance };
}

describe('OledSsd1306Plugin', () => {
  test('parses addressing commands and publishes a framebuffer snapshot after data transfer', () => {
    const { host } = createPlugin();

    expect(
      host.i2cTransfer(0x3c, new Uint8Array([0x00, 0x20, 0x00, 0x21, 0x02, 0x03, 0x22, 0x01, 0x01]))
        .ack,
    ).toBe(true);
    expect(host.i2cTransfer(0x3c, new Uint8Array([0x40, 0xaa, 0xbb])).ack).toBe(true);

    expect(host.lastPublish('displayKind')).toBe('ssd1306_fb');
    const framebuffer = host.lastPublish('fb') as Uint8Array;
    expect(framebuffer).toEqual(expect.any(Uint8Array));
    expect(framebuffer.slice(128 + 2, 128 + 4)).toEqual(new Uint8Array([0xaa, 0xbb]));
    expect(host.publishes.map(({ channel }) => channel)).toEqual(
      expect.arrayContaining(['width', 'height', 'fb', 'colorFormat', 'displayKind']),
    );
  });

  test('defers a second dirty framebuffer publish until the 16ms throttle expires', () => {
    const { host } = createPlugin();

    host.i2cTransfer(0x3c, new Uint8Array([0x40, 0x01]));
    const firstPublishCount = host.publishes.filter(({ channel }) => channel === 'fb').length;

    host.i2cTransfer(0x3c, new Uint8Array([0x40, 0x02]));
    expect(host.publishes.filter(({ channel }) => channel === 'fb')).toHaveLength(
      firstPublishCount,
    );

    host.advance(16_000n);
    expect(host.publishes.filter(({ channel }) => channel === 'fb')).toHaveLength(
      firstPublishCount + 1,
    );
  });

  test('parses SH1106 page addressing commands correctly', () => {
    const { host } = createPlugin();

    /* SH1106 Page 1 command: 0xB1, col lower 0x02, col upper 0x10 */
    expect(host.i2cTransfer(0x3c, new Uint8Array([0x00, 0xb1, 0x02, 0x10])).ack).toBe(true);
    expect(host.i2cTransfer(0x3c, new Uint8Array([0x40, 0xcc, 0xdd])).ack).toBe(true);

    const framebuffer = host.lastPublish('fb') as Uint8Array;
    expect(framebuffer).toEqual(expect.any(Uint8Array));
    expect(framebuffer.slice(128 + 2, 128 + 4)).toEqual(new Uint8Array([0xcc, 0xdd]));
  });
});
