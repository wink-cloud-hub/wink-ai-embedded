import { expect, test, describe } from 'bun:test';
import { LogicStates } from '@wink-ai/unisim';

import {
  SegDisplayPlugin,
  GHOST_MAX_BRIGHT,
  LOGIC_THRESHOLD,
} from '../simulation';
import { CHAR_TO_SEG_MASK } from '../seg-font';

function createMockCtx() {
  let currentTimeUs = 0n;
  const publishes: Array<{ channel: string; value: unknown }> = [];
  const deferred: Array<{ atUs: bigint; callback: () => void }> = [];
  const warnings: string[] = [];

  const ctx = {
    nowUs: () => currentTimeUs,
    publish: (ch: string, val: unknown) => {
      publishes.push({ channel: ch, value: val });
    },
    deferUs: (delayUs: bigint, cb: () => void) => {
      deferred.push({ atUs: currentTimeUs + delayUs, callback: cb });
    },
    system: {
      log: {
        warn: (msg: string) => warnings.push(msg),
      },
      time: {
        nowUs: () => currentTimeUs,
      },
    },
    advanceTime: (deltaUs: bigint) => {
      const targetTimeUs = currentTimeUs + deltaUs;
      while (deferred.length > 0) {
        deferred.sort((a, b) => (a.atUs < b.atUs ? -1 : a.atUs > b.atUs ? 1 : 0));
        if (deferred[0].atUs <= targetTimeUs) {
          const item = deferred.shift()!;
          currentTimeUs = item.atUs;
          item.callback();
        } else {
          break;
        }
      }
      currentTimeUs = targetTimeUs;
    },
    getLatestPublish: (ch: string) => {
      for (let i = publishes.length - 1; i >= 0; i--) {
        if (publishes[i].channel === ch) return publishes[i].value;
      }
      return undefined;
    },
    publishes,
    deferred,
    warnings,
  };

  return ctx;
}

// AT89C52 Port mapping to peripheral pins:
// P0 (pins 0..7)   -> Segments A, B, C, D, E, F, G, DP
// P2 (pins 16..23) -> Digits DIG1..DIG8
function create8051PinMapping(): Record<string, number> {
  const map: Record<string, number> = {
    A: 0,
    B: 1,
    C: 2,
    D: 3,
    E: 4,
    F: 5,
    G: 6,
    DP: 7,
  };
  for (let d = 0; d < 8; d++) {
    map[`DIG${d + 1}`] = 16 + d;
  }
  return map;
}

const SEGMENT_NAMES = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'DP'] as const;

// Keil C51 segment table (0-9)
const C51_SEG_TABLE = [
  0x3f, // 0
  0x06, // 1
  0x5b, // 2
  0x4f, // 3
  0x66, // 4
  0x6d, // 5
  0x7d, // 6
  0x07, // 7
  0x7f, // 8
  0x6f, // 9
];

/**
 * Drive a single 8051 port write (P0 or P2) into the peripheral plugin,
 * simulating js_pal_gpio_write on pin edge transitions.
 */
function drive8051Port(
  plugin: SegDisplayPlugin,
  basePin: number,
  oldVal: number,
  newVal: number,
  atUs: bigint,
): void {
  const diff = (oldVal ^ newVal) & 0xff;
  for (let bit = 0; bit < 8; bit++) {
    if ((diff & (1 << bit)) !== 0) {
      const pinNum = basePin + bit;
      const level = (newVal & (1 << bit)) !== 0 ? LogicStates.HIGH : LogicStates.LOW;
      plugin.onPinChange(pinNum, level, atUs);
    }
  }
}

describe('Part 2: 8051 Bare-Metal Firmware End-to-End Simulation Suite', () => {
  test('E2E-1: standard 8051 Timer0 ISR with blanking drives 8 digits to 12345678', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = create8051PinMapping();

    plugin.onBind(ctx as any, pinMap, {
      variant: 'direct_gpio_8d',
      segActiveLevel: 'high',
      digitActiveLevel: 'low',
    });

    const displayBuf = [1, 2, 3, 4, 5, 6, 7, 8];
    let p0 = 0x00;
    let p2 = 0xff;
    let tUs = 0n;

    // Simulate 10 full frames (10 * 8ms = 80ms of 8051 execution)
    for (let frame = 0; frame < 10; frame++) {
      for (let d = 0; d < 8; d++) {
        // --- 8051 Timer0 ISR execution (1ms interval) ---
        // Step 1: Blanking - P2 = 0xFF (turn off old digit)
        const nextP2Blank = 0xff;
        drive8051Port(plugin, 16, p2, nextP2Blank, tUs);
        p2 = nextP2Blank;

        // Step 2: P0 = SEG_TABLE[buf[d]]
        const nextP0 = C51_SEG_TABLE[displayBuf[d]];
        drive8051Port(plugin, 0, p0, nextP0, tUs);
        p0 = nextP0;

        // Step 3: P2 = ~(1 << d) (activate new digit)
        const nextP2 = (~(1 << d)) & 0xff;
        drive8051Port(plugin, 16, p2, nextP2, tUs);
        p2 = nextP2;

        // Hold for 1ms (1000us)
        tUs += 1000n;
        ctx.advanceTime(1000n);
      }
    }

    // Assert decoded text
    const text = ctx.getLatestPublish('text') as string;
    expect(text).toBe('12345678');

    // Assert segment masks
    const segMaskStr = ctx.getLatestPublish('segMask') as string;
    const segMask = JSON.parse(segMaskStr) as number[];
    expect(segMask.length).toBe(8);
    for (let d = 0; d < 8; d++) {
      expect(segMask[d]).toBe(C51_SEG_TABLE[displayBuf[d]]);
    }

    // Assert steady-state brightness
    const bright = ctx.getLatestPublish('bright') as Uint8Array;
    expect(bright).toBeDefined();
    for (let d = 0; d < 8; d++) {
      const mask = C51_SEG_TABLE[displayBuf[d]];
      for (let s = 0; s < 8; s++) {
        const isLit = (mask & (1 << s)) !== 0;
        const b = bright[d * 8 + s];
        if (isLit) {
          expect(b).toBeGreaterThanOrEqual(200);
        } else {
          // With blanking, unlit segments must be pure black (0)
          expect(b).toBe(0);
        }
      }
    }

    // Assert scan frequency is ~1000Hz (1ms period)
    const scanHz = ctx.getLatestPublish('scanHz') as number;
    expect(scanHz).toBeGreaterThanOrEqual(800);
    expect(scanHz).toBeLessThanOrEqual(1200);
  });

  test('E2E-2: teaching fault: omitting blanking injects measurable ghosting', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = create8051PinMapping();

    plugin.onBind(ctx as any, pinMap, {
      variant: 'direct_gpio_8d',
      segActiveLevel: 'high',
      digitActiveLevel: 'low',
    });

    // Display '8' on digit 0, and blank ' ' on digit 1
    let p0 = 0x00;
    let p2 = 0xff;
    let tUs = 0n;

    for (let frame = 0; frame < 10; frame++) {
      // Digit 0: shows '8' (all segments 0x7F)
      drive8051Port(plugin, 0, p0, 0x7f, tUs);
      p0 = 0x7f;
      drive8051Port(plugin, 16, p2, 0xfe, tUs); // DIG1 active (bit 0 = 0)
      p2 = 0xfe;

      tUs += 950n;
      ctx.advanceTime(950n);

      // Fault: Digit 1 activates 50us BEFORE clearing segment bus (leakage window)
      drive8051Port(plugin, 16, p2, 0xfc, tUs); // both DIG1 and DIG2 momentarily active
      p2 = 0xfc;

      tUs += 50n;
      ctx.advanceTime(50n);

      // Now shut down DIG1 and blank P0 for digit 1
      drive8051Port(plugin, 0, p0, 0x00, tUs);
      p0 = 0x00;
      drive8051Port(plugin, 16, p2, 0xfd, tUs); // only DIG2 active
      p2 = 0xfd;

      tUs += 950n;
      ctx.advanceTime(950n);

      // Turn off DIG2
      drive8051Port(plugin, 16, p2, 0xff, tUs);
      p2 = 0xff;

      // Rest of frame (6ms)
      tUs += 6000n;
      ctx.advanceTime(6000n);
    }

    const bright = ctx.getLatestPublish('bright') as Uint8Array;
    // Digit 0 is cleanly lit (>= LOGIC_THRESHOLD)
    expect(bright[0 * 8 + 0]).toBeGreaterThanOrEqual(LOGIC_THRESHOLD);

    // Digit 1 has residual ghosting on segment A due to the 50us overlap!
    const ghostBright = bright[1 * 8 + 0];
    expect(ghostBright).toBeGreaterThan(0);
    expect(ghostBright).toBeLessThan(GHOST_MAX_BRIGHT);

    // The ghost segment must NOT be accepted into the logic segMask
    const segMaskStr = ctx.getLatestPublish('segMask') as string;
    const segMask = JSON.parse(segMaskStr) as number[];
    expect(segMask[1] & 1).toBe(0);
  });

  test('E2E-3: dynamic display buffer update switches displayed text with smooth transition', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = create8051PinMapping();

    plugin.onBind(ctx as any, pinMap, {
      variant: 'direct_gpio_8d',
      segActiveLevel: 'high',
      digitActiveLevel: 'low',
    });

    let displayBuf = [1, 2, 3, 4, 5, 6, 7, 8];
    let p0 = 0x00;
    let p2 = 0xff;
    let tUs = 0n;

    // Phase 1: 5 frames of "12345678"
    for (let frame = 0; frame < 5; frame++) {
      for (let d = 0; d < 8; d++) {
        drive8051Port(plugin, 16, p2, 0xff, tUs);
        p2 = 0xff;
        drive8051Port(plugin, 0, p0, C51_SEG_TABLE[displayBuf[d]], tUs);
        p0 = C51_SEG_TABLE[displayBuf[d]];
        drive8051Port(plugin, 16, p2, (~(1 << d)) & 0xff, tUs);
        p2 = (~(1 << d)) & 0xff;

        tUs += 1000n;
        ctx.advanceTime(1000n);
      }
    }
    expect(ctx.getLatestPublish('text')).toBe('12345678');

    // Phase 2: 8051 code dynamically changes buffer to "87654321"
    displayBuf = [8, 7, 6, 5, 4, 3, 2, 1];
    for (let frame = 0; frame < 8; frame++) {
      for (let d = 0; d < 8; d++) {
        drive8051Port(plugin, 16, p2, 0xff, tUs);
        p2 = 0xff;
        drive8051Port(plugin, 0, p0, C51_SEG_TABLE[displayBuf[d]], tUs);
        p0 = C51_SEG_TABLE[displayBuf[d]];
        drive8051Port(plugin, 16, p2, (~(1 << d)) & 0xff, tUs);
        p2 = (~(1 << d)) & 0xff;

        tUs += 1000n;
        ctx.advanceTime(1000n);
      }
    }

    // Now text reflects new buffer seamlessly
    expect(ctx.getLatestPublish('text')).toBe('87654321');
  });

  test('E2E-4: MCU reset recovery resets display and resumes normal scan', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = create8051PinMapping();

    plugin.onBind(ctx as any, pinMap, {
      variant: 'direct_gpio_8d',
      segActiveLevel: 'high',
      digitActiveLevel: 'low',
    });

    let p0 = 0x00;
    let p2 = 0xff;
    let tUs = 0n;

    // Run 3 frames
    for (let frame = 0; frame < 3; frame++) {
      for (let d = 0; d < 8; d++) {
        drive8051Port(plugin, 16, p2, 0xff, tUs);
        p2 = 0xff;
        drive8051Port(plugin, 0, p0, C51_SEG_TABLE[d + 1], tUs);
        p0 = C51_SEG_TABLE[d + 1];
        drive8051Port(plugin, 16, p2, (~(1 << d)) & 0xff, tUs);
        p2 = (~(1 << d)) & 0xff;
        tUs += 1000n;
        ctx.advanceTime(1000n);
      }
    }
    expect(ctx.getLatestPublish('text')).toBe('12345678');

    // 8051 Hardware Reset: P0 -> 0xFF (quasi-bidirectional reset state), P2 -> 0xFF
    plugin.onReset();
    const brightReset = ctx.getLatestPublish('bright') as Uint8Array;
    expect(brightReset.every((b) => b === 0)).toBe(true);
    expect((ctx.getLatestPublish('text') as string).trim()).toBe('');

    // Resume scanning after reset
    p0 = 0x00;
    p2 = 0xff;
    for (let frame = 0; frame < 5; frame++) {
      for (let d = 0; d < 8; d++) {
        drive8051Port(plugin, 16, p2, 0xff, tUs);
        p2 = 0xff;
        drive8051Port(plugin, 0, p0, C51_SEG_TABLE[d + 1], tUs);
        p0 = C51_SEG_TABLE[d + 1];
        drive8051Port(plugin, 16, p2, (~(1 << d)) & 0xff, tUs);
        p2 = (~(1 << d)) & 0xff;
        tUs += 1000n;
        ctx.advanceTime(1000n);
      }
    }
    expect(ctx.getLatestPublish('text')).toBe('12345678');
  });
});
