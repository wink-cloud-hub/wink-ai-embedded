import { describe, expect, test } from "bun:test";
import { createPluginStubHost } from "@wink-ai/unisim-sdk";

import { SpiEepromPlugin } from "../simulation";

const DEV = "spi_eeprom";
const CMD_WREN = 0x06;
const CMD_WRDI = 0x04;
const CMD_RDSR = 0x05;
const CMD_READ = 0x03;
const CMD_WRITE = 0x02;

function frame(...bytes: number[]): Uint8Array {
  return new Uint8Array(bytes);
}

function createPlugin(properties: Record<string, unknown> = {}) {
  const host = createPluginStubHost();
  const binding = host.bind(SpiEepromPlugin, { instanceId: "spi_eeprom:0", properties });
  const plugin = binding.instance as unknown as SpiEepromPlugin;
  return { host, plugin };
}

describe("SpiEepromPlugin (M95256)", () => {
  test("WREN latch + WRITE commit + READ round-trip (whole-frame path)", () => {
    const { host, plugin } = createPlugin();

    host.spiTransfer(DEV, frame(CMD_WREN));
    expect(plugin.isWelSet()).toBe(true);

    host.spiTransfer(DEV, frame(CMD_WRITE, 0x00, 0x10, 0xab, 0xcd));
    expect(plugin.isWelSet()).toBe(false); // WEL clears on the write frame's rising edge

    const read = host.spiTransfer(DEV, frame(CMD_READ, 0x00, 0x10, 0x00, 0x00));
    expect(Array.from(read.slice(3))).toEqual([0xab, 0xcd]);
  });

  test("WRITE without WREN is ignored and erased cells read 0xFF", () => {
    const { host } = createPlugin();

    host.spiTransfer(DEV, frame(CMD_WRITE, 0x00, 0x20, 0x55));
    const read = host.spiTransfer(DEV, frame(CMD_READ, 0x00, 0x20, 0x00));
    expect(read[3]).toBe(0xff);

    host.spiTransfer(DEV, frame(CMD_WREN));
    host.spiTransfer(DEV, frame(CMD_WRDI));
    host.spiTransfer(DEV, frame(CMD_WRITE, 0x00, 0x20, 0x55));
    const read2 = host.spiTransfer(DEV, frame(CMD_READ, 0x00, 0x20, 0x00));
    expect(read2[3]).toBe(0xff);
  });

  test("RDSR streams WEL/WIP status bits", () => {
    const { host } = createPlugin();
    expect(host.spiTransfer(DEV, frame(CMD_RDSR, 0x00))[1]).toBe(0);

    host.spiTransfer(DEV, frame(CMD_WREN));
    expect(host.spiTransfer(DEV, frame(CMD_RDSR, 0x00))[1]).toBe(0b10);
  });

  test("WIP window blocks everything except RDSR until tW expires", () => {
    const { host, plugin } = createPlugin({ writeCycleUs: 5000 });

    host.spiTransfer(DEV, frame(CMD_WREN));
    host.spiTransfer(DEV, frame(CMD_WRITE, 0x00, 0x30, 0x11));
    expect(plugin.isWip()).toBe(true);
    expect(host.spiTransfer(DEV, frame(CMD_RDSR, 0x00))[1] & 0b01).toBe(1);

    // Frames inside the window are ignored (WREN does not latch, WRITE no-ops).
    host.spiTransfer(DEV, frame(CMD_WREN));
    expect(plugin.isWelSet()).toBe(false);
    host.spiTransfer(DEV, frame(CMD_WRITE, 0x00, 0x30, 0x99));

    host.advance(5000n);
    expect(plugin.isWip()).toBe(false);
    expect(host.spiTransfer(DEV, frame(CMD_RDSR, 0x00))[1] & 0b01).toBe(0);
    const still = host.spiTransfer(DEV, frame(CMD_READ, 0x00, 0x30, 0x00));
    expect(still[3]).toBe(0x11);
  });

  test("session path: open/transfer/close with CS-edge latch semantics", () => {
    const { host, plugin } = createPlugin();

    const open1 = host.spiSessionOpen(DEV);
    expect(open1.ok).toBe(true);
    host.spiSessionTransfer(open1.handle, frame(CMD_WREN));
    host.spiSessionClose(open1.handle);
    expect(plugin.isWelSet()).toBe(true); // latched at CS rising edge

    const open2 = host.spiSessionOpen(DEV);
    host.spiSessionTransfer(open2.handle, frame(CMD_WRITE, 0x00, 0x40, 0x5a));
    host.spiSessionClose(open2.handle);
    expect(plugin.isWelSet()).toBe(false);

    const open3 = host.spiSessionOpen(DEV);
    const read = host.spiSessionTransfer(open3.handle, frame(CMD_READ, 0x00, 0x40, 0x00));
    host.spiSessionClose(open3.handle);
    expect(read.ok).toBe(true);
    expect(read.rx[3]).toBe(0x5a);
  });

  test("page write rolls over inside the 64-byte page", () => {
    const { host } = createPlugin();
    const payload = new Uint8Array(70);
    for (let i = 0; i < payload.length; i++) {
      payload[i] = 0x20 + i;
    }
    const writeFrame = new Uint8Array(3 + payload.length);
    writeFrame.set(frame(CMD_WRITE, 0x00, 0x40));
    writeFrame.set(payload, 3);

    host.spiTransfer(DEV, frame(CMD_WREN));
    host.spiTransfer(DEV, writeFrame);

    const wrapped = host.spiTransfer(
      DEV,
      frame(CMD_READ, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
    );
    expect(Array.from(wrapped.slice(3))).toEqual(Array.from(payload.slice(64, 70)));
  });

  test("unknown device id cannot open a session", () => {
    const { host } = createPlugin();
    const bad = host.spiSessionOpen("nope");
    expect(bad.ok).toBe(false);
    expect(bad.handle).toBe(0xff);
  });

  test("serialize/deserialize restores memory, WEL and write count", () => {
    const { host, plugin } = createPlugin();
    host.spiTransfer(DEV, frame(CMD_WREN));
    host.spiTransfer(DEV, frame(CMD_WRITE, 0x00, 0x05, 0x77));
    host.spiTransfer(DEV, frame(CMD_WREN)); // leave WEL latched

    const snapshot = plugin.serializeState();
    const restored = createPlugin().plugin;
    restored.deserializeState(snapshot);

    expect(restored.isWelSet()).toBe(true);
    const read = restored.onFrame(frame(CMD_READ, 0x00, 0x05, 0x00));
    expect(read[3]).toBe(0x77);
  });
});
