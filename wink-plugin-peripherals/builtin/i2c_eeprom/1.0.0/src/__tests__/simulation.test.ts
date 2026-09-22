import { describe, expect, test } from "bun:test";
import { createPluginStubHost } from "@wink-ai/unisim-sdk";

import { I2cEepromPlugin } from "../simulation";

const ADDR = 0x50;

function createPlugin(properties: Record<string, unknown> = {}) {
  const host = createPluginStubHost();
  const binding = host.bind(I2cEepromPlugin, { instanceId: "i2c_eeprom:0", properties });
  const plugin = binding.instance as unknown as I2cEepromPlugin;
  return { host, plugin };
}

describe("I2cEepromPlugin (AT24C256)", () => {
  test("legacy whole-frame write then current-address read", () => {
    const { host } = createPlugin();

    expect(host.i2cTransfer(ADDR, new Uint8Array([0x00, 0x10, 0xab])).ack).toBe(true);
    const read = host.i2cTransfer(ADDR, new Uint8Array([0x00, 0x10]), { readLength: 1 });
    expect(read.ack).toBe(true);
    expect(Array.from(read.readBytes)).toEqual([0xab]);
  });

  test("session random read: address write, repeated START, one byte", () => {
    const { host } = createPlugin();
    host.i2cTransfer(ADDR, new Uint8Array([0x00, 0x20, 0x11, 0x22, 0x33]));

    const open = host.i2cSessionOpen(ADDR, 0);
    expect(open.ok).toBe(true);
    expect(open.addrNack).toBe(false);
    expect(host.i2cSessionWrite(open.handle, new Uint8Array([0x00, 0x20])).ok).toBe(true);

    const restart = host.i2cSessionRestart(open.handle, ADDR, 1);
    expect(restart.ok).toBe(true);
    expect(restart.addrNack).toBe(false);

    const read = host.i2cSessionRead(open.handle, 3, 0);
    expect(read.ok).toBe(true);
    expect(Array.from(read.readBytes)).toEqual([0x11, 0x22, 0x33]);
    host.i2cSessionClose(open.handle);
  });

  test("sequential read advances the internal pointer", () => {
    const { host } = createPlugin();
    host.i2cTransfer(ADDR, new Uint8Array([0x00, 0x30, 0x0a, 0x0b, 0x0c]));

    const first = host.i2cTransfer(ADDR, new Uint8Array([0x00, 0x30]), { readLength: 1 });
    const second = host.i2cTransfer(ADDR, new Uint8Array(0), { readLength: 1 });
    const third = host.i2cTransfer(ADDR, new Uint8Array(0), { readLength: 1 });
    expect([first.readBytes[0], second.readBytes[0], third.readBytes[0]]).toEqual([
      0x0a, 0x0b, 0x0c,
    ]);
  });

  test("page write rolls over inside the 64-byte page", () => {
    const { host } = createPlugin();
    const payload = new Uint8Array(70);
    for (let i = 0; i < payload.length; i++) {
      payload[i] = 0xa0 + i;
    }
    const frame = new Uint8Array(2 + payload.length);
    frame[0] = 0x00;
    frame[1] = 0x40; // page-aligned start of page 1
    frame.set(payload, 2);
    expect(host.i2cTransfer(ADDR, frame).ack).toBe(true);

    // Bytes 64..69 wrapped back onto the page start (roll-over overwrite).
    const wrapped = host.i2cTransfer(ADDR, new Uint8Array([0x00, 0x40]), { readLength: 6 });
    expect(Array.from(wrapped.readBytes)).toEqual(Array.from(payload.slice(64, 70)));

    // The rest of the page kept the earlier bytes (offsets 6..63 unchanged).
    const rest = host.i2cTransfer(ADDR, new Uint8Array([0x00, 0x46]), { readLength: 58 });
    expect(Array.from(rest.readBytes)).toEqual(Array.from(payload.slice(6, 64)));
  });

  test("tWR window NACKs the address phase until the write cycle expires", () => {
    const { host } = createPlugin({ writeCycleUs: 5000 });
    expect(host.i2cTransfer(ADDR, new Uint8Array([0x00, 0x10, 0x77])).ack).toBe(true);
    expect(host.lastPublish("busy")).toBe(1);

    const blocked = host.i2cSessionOpen(ADDR, 1);
    expect(blocked.addrNack).toBe(true);
    host.i2cSessionClose(blocked.handle);

    host.advance(5000n);
    expect(host.lastPublish("busy")).toBe(0);

    const ready = host.i2cSessionOpen(ADDR, 1);
    expect(ready.addrNack).toBe(false);
    host.i2cSessionClose(ready.handle);
  });

  test("whole-frame transfer inside the tWR window fails with granular NACK bits", () => {
    const { host, plugin } = createPlugin({ writeCycleUs: 5000 });
    host.i2cTransfer(ADDR, new Uint8Array([0x00, 0x10, 0x01]));

    // The legacy whole-frame path maps the address-phase refusal onto bit 0.
    const blocked = plugin.onTransfer(new Uint8Array([0x00, 0x10]), 1);
    expect(blocked.ack).toBe(false);
    expect(blocked.nackBits).toBe(0b1);
  });

  test("writeCount state channel tracks committed transactions", () => {
    const { host } = createPlugin();
    expect(host.lastPublish("writeCount")).toBe(0);
    host.i2cTransfer(ADDR, new Uint8Array([0x00, 0x00, 0x01]));
    host.i2cTransfer(ADDR, new Uint8Array([0x00, 0x01, 0x02]));
    expect(host.lastPublish("writeCount")).toBe(2);
  });

  test("serialize/deserialize keeps the non-volatile image and pointer", () => {
    const { host, plugin } = createPlugin();
    host.i2cTransfer(ADDR, new Uint8Array([0x00, 0x05, 0x5a, 0x5b]));

    const snapshot = plugin.serializeState();
    const restored = createPlugin().plugin;
    restored.deserializeState(snapshot);

    expect(restored.addressPointer).toBe(7);
    const read = restored.onTransfer(new Uint8Array([0x00, 0x05]), 2);
    expect(Array.from(read.readBytes ?? [])).toEqual([0x5a, 0x5b]);
  });
});
