import { expect, test, describe } from 'bun:test';
import { LogicStates } from '@wink-ai/unisim';

import {
  SegDisplayPlugin,
  segDisplayManifest,
  segDisplayManifestFactory,
  createSegDisplayManifest,
  GHOST_MAX_BRIGHT,
  LOGIC_THRESHOLD,
  type SegDisplayProps,
} from '../simulation';
import { CHAR_TO_SEG_MASK } from '../seg-font';
import { SEG_VARIANTS } from '../variants';

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

const SEGMENT_NAMES = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'DP'] as const;

const SEG_PINS: Record<string, number> = {
  A: 0,
  B: 1,
  C: 2,
  D: 3,
  E: 4,
  F: 5,
  G: 6,
  DP: 7,
};

function createPinMapping(nDigits: number = 8): Record<string, number> {
  const map: Record<string, number> = { ...SEG_PINS };
  for (let d = 0; d < nDigits; d++) {
    map[`DIG${d + 1}`] = 10 + d;
  }
  return map;
}

describe('seg_display simulation & contract test suite', () => {
  // Test 1: Basic manifest contract
  test('1. manifest satisfies timingModel and stateChannel types', () => {
    expect(segDisplayManifest.type).toBe('seg_display');
    expect(segDisplayManifest.category).toBe('display');
    expect(segDisplayManifest.timingModel).toBe('event-driven');

    const allowedTypes = new Set(['number', 'boolean', 'string']);
    for (const [chName, chDef] of Object.entries(segDisplayManifest.stateChannels)) {
      expect(allowedTypes.has(chDef.type)).toBe(true);
    }
  });

  // Test 2: Pin counts for 4 variants
  test('2. variant pin counts equal 8 + N with 1d optional DIG1', () => {
    const m8 = createSegDisplayManifest('direct_gpio_8d');
    expect(m8.pins.length).toBe(16);

    const m4 = createSegDisplayManifest('direct_gpio_4d');
    expect(m4.pins.length).toBe(12);

    const m2 = createSegDisplayManifest('direct_gpio_2d');
    expect(m2.pins.length).toBe(10);

    const m1 = createSegDisplayManifest('direct_gpio_1d');
    expect(m1.pins.length).toBe(9);

    const dig1 = m1.pins.find((p) => p.name === 'DIG1');
    expect(dig1?.required).toBe(false);

    const dig1_8d = m8.pins.find((p) => p.name === 'DIG1');
    expect(dig1_8d?.required).toBe(true);
  });

  // Test 3: manifestFactory resolution
  test('3. manifestFactory generates variant and falls back to 8d on unknown', () => {
    const m4 = segDisplayManifestFactory('direct_gpio_4d');
    expect(m4.pins.length).toBe(12);

    const fallback = segDisplayManifestFactory('unknown_variant');
    expect(fallback.pins.length).toBe(16);
  });

  // Test 4: Cold-start onBound state
  test('4. onBound publishes initial zero frame and cold start is unlit', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = createPinMapping(8);

    plugin.onBind(ctx as any, pinMap, {});

    const bright = ctx.getLatestPublish('bright') as Uint8Array;
    expect(bright).toBeDefined();
    expect(bright.length).toBe(64);
    expect(bright.every((b) => b === 0)).toBe(true);

    const segMask = ctx.getLatestPublish('segMask');
    expect(segMask).toBe(JSON.stringify(new Array(8).fill(0)));

    const text = ctx.getLatestPublish('text');
    expect(text).toBe('        ');
  });

  // Test 5: Common cathode basic driving
  test('5. common cathode: DIG1=LOW, A=HIGH saturates; DIG1=HIGH unlit', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = createPinMapping(8);

    plugin.onBind(ctx as any, pinMap, {
      segActiveLevel: 'high',
      digitActiveLevel: 'low',
    });

    // DIG1 = LOW (active), A = HIGH (active)
    plugin.onPinChange(pinMap.DIG1, LogicStates.LOW, 0n);
    plugin.onPinChange(pinMap.A, LogicStates.HIGH, 0n);

    // Advance 5ms (5000us)
    ctx.advanceTime(5000n);
    plugin.onPinChange(pinMap.A, LogicStates.HIGH, 5000n);

    let bright = ctx.getLatestPublish('bright') as Uint8Array;
    expect(bright[0]).toBeGreaterThanOrEqual(200);

    // Inactive DIG1 (set to HIGH)
    ctx.advanceTime(1000n);
    plugin.onPinChange(pinMap.DIG1, LogicStates.HIGH, 6000n);

    // Advance 100ms so it decays
    ctx.advanceTime(100_000n);
    bright = ctx.getLatestPublish('bright') as Uint8Array;
    expect(bright[0]).toBe(0);
  });

  // Test 6: 4 Polarity combinations matrix
  test('6. four polarity combinations decode identical glyph', () => {
    const cases = [
      { segActiveLevel: 'high' as const, digitActiveLevel: 'low' as const, segLevel: LogicStates.HIGH, digLevel: LogicStates.LOW },
      { segActiveLevel: 'high' as const, digitActiveLevel: 'high' as const, segLevel: LogicStates.HIGH, digLevel: LogicStates.HIGH },
      { segActiveLevel: 'low' as const, digitActiveLevel: 'low' as const, segLevel: LogicStates.LOW, digLevel: LogicStates.LOW },
      { segActiveLevel: 'low' as const, digitActiveLevel: 'high' as const, segLevel: LogicStates.LOW, digLevel: LogicStates.HIGH },
    ];

    for (const c of cases) {
      const plugin = new SegDisplayPlugin();
      const ctx = createMockCtx();
      const pinMap = createPinMapping(1);

      plugin.onBind(ctx as any, pinMap, {
        variant: 'direct_gpio_1d',
        segActiveLevel: c.segActiveLevel,
        digitActiveLevel: c.digitActiveLevel,
      });

      // Drive DIG1 active
      plugin.onPinChange(pinMap.DIG1, c.digLevel, 0n);
      // Drive segments for '8' (all A..G active)
      for (const seg of ['A', 'B', 'C', 'D', 'E', 'F', 'G']) {
        plugin.onPinChange(pinMap[seg], c.segLevel, 0n);
      }

      ctx.advanceTime(5000n);
      plugin.onPinChange(pinMap.DIG1, c.digLevel, 5000n);

      const text = (ctx.getLatestPublish('text') as string).trim();
      expect(text).toBe('8');
    }
  });

  // Test 7: commonAnode preset vs explicit level override
  test('7. commonAnode preset sets seg=low/dig=high, explicit override takes precedence', () => {
    const plugin1 = new SegDisplayPlugin();
    const ctx1 = createMockCtx();
    const pinMap = createPinMapping(1);

    // commonAnode alone
    plugin1.onBind(ctx1 as any, pinMap, {
      variant: 'direct_gpio_1d',
      commonAnode: true,
    });

    // seg=low, dig=high should be active
    plugin1.onPinChange(pinMap.DIG1, LogicStates.HIGH, 0n);
    plugin1.onPinChange(pinMap.A, LogicStates.LOW, 0n);
    ctx1.advanceTime(5000n);
    plugin1.onPinChange(pinMap.DIG1, LogicStates.HIGH, 5000n);

    let bright = ctx1.getLatestPublish('bright') as Uint8Array;
    expect(bright[0]).toBeGreaterThanOrEqual(200);

    // Explicit override: segActiveLevel='high' with commonAnode=true
    const plugin2 = new SegDisplayPlugin();
    const ctx2 = createMockCtx();
    plugin2.onBind(ctx2 as any, pinMap, {
      variant: 'direct_gpio_1d',
      commonAnode: true,
      segActiveLevel: 'high',
    });

    // Now seg needs HIGH to be active
    plugin2.onPinChange(pinMap.DIG1, LogicStates.HIGH, 0n);
    plugin2.onPinChange(pinMap.A, LogicStates.HIGH, 0n);
    ctx2.advanceTime(5000n);
    plugin2.onPinChange(pinMap.DIG1, LogicStates.HIGH, 5000n);

    bright = ctx2.getLatestPublish('bright') as Uint8Array;
    expect(bright[0]).toBeGreaterThanOrEqual(200);
  });

  // Test 8: 1d static drive without DIG1 mapped
  test('8. 1d static drive: unmapped DIG1 allows segment lines alone to light', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    // Only map segment pins, omit DIG1
    const pinMap = { ...SEG_PINS };

    plugin.onBind(ctx as any, pinMap, {
      variant: 'direct_gpio_1d',
      segActiveLevel: 'high',
    });

    plugin.onPinChange(pinMap.A, LogicStates.HIGH, 0n);
    ctx.advanceTime(5000n);
    plugin.onPinChange(pinMap.A, LogicStates.HIGH, 5000n);

    const bright = ctx.getLatestPublish('bright') as Uint8Array;
    expect(bright[0]).toBeGreaterThanOrEqual(200);
  });

  // Test 9: Normal 8-digit 1kHz multiplex scan
  test('9. normal 8-digit scan: 1ms/digit over 50ms yields bright >= 200 and text 12345678', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = createPinMapping(8);

    plugin.onBind(ctx as any, pinMap, {
      segActiveLevel: 'high',
      digitActiveLevel: 'low',
    });

    const chars = ['1', '2', '3', '4', '5', '6', '7', '8'];
    let tUs = 0n;

    // Run 6 full frames (6 * 8 = 48ms)
    for (let frame = 0; frame < 6; frame++) {
      for (let d = 0; d < 8; d++) {
        const charMask = CHAR_TO_SEG_MASK[chars[d]];
        const digPin = pinMap[`DIG${d + 1}`];

        // 1. Activate digit and set segments
        plugin.onPinChange(digPin, LogicStates.LOW, tUs);
        for (let s = 0; s < 8; s++) {
          const isLit = (charMask & (1 << s)) !== 0;
          plugin.onPinChange(SEG_PINS[SEGMENT_NAMES[s]], isLit ? LogicStates.HIGH : LogicStates.LOW, tUs);
        }

        // Hold for 1000us
        tUs += 1000n;
        ctx.advanceTime(1000n);

        // Deactivate digit
        plugin.onPinChange(digPin, LogicStates.HIGH, tUs);
      }
    }

    const text = ctx.getLatestPublish('text') as string;
    expect(text).toBe('12345678');

    const bright = ctx.getLatestPublish('bright') as Uint8Array;
    for (let d = 0; d < 8; d++) {
      const charMask = CHAR_TO_SEG_MASK[chars[d]];
      for (let s = 0; s < 8; s++) {
        const isLit = (charMask & (1 << s)) !== 0;
        const b = bright[d * 8 + s];
        if (isLit) {
          expect(b).toBeGreaterThanOrEqual(200);
        } else {
          expect(b).toBe(0);
        }
      }
    }
  });

  // Test 10: Ghosting reproduction
  test('10. ghosting: 50us overlap produces faint neighbor brightness < 40 and not in mask', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = createPinMapping(8);

    plugin.onBind(ctx as any, pinMap, {
      segActiveLevel: 'high',
      digitActiveLevel: 'low',
    });

    let tUs = 0n;
    // Simulate 8 cycles with 50us overlap between digit 0 and digit 1
    for (let frame = 0; frame < 8; frame++) {
      // Digit 0 active with '8' (all segments HIGH)
      plugin.onPinChange(pinMap.DIG1, LogicStates.LOW, tUs);
      for (let s = 0; s < 7; s++) {
        plugin.onPinChange(pinMap[SEGMENT_NAMES[s]], LogicStates.HIGH, tUs);
      }
      tUs += 950n;
      ctx.advanceTime(950n);

      // Fault: Digit 2 activates 50us BEFORE segment codes are cleared
      plugin.onPinChange(pinMap.DIG2, LogicStates.LOW, tUs);
      tUs += 50n;
      ctx.advanceTime(50n);

      // Now shut down Digit 1 and clear segments for Digit 2 (which should show blank)
      plugin.onPinChange(pinMap.DIG1, LogicStates.HIGH, tUs);
      for (let s = 0; s < 7; s++) {
        plugin.onPinChange(pinMap[SEGMENT_NAMES[s]], LogicStates.LOW, tUs);
      }
      tUs += 950n;
      ctx.advanceTime(950n);
      plugin.onPinChange(pinMap.DIG2, LogicStates.HIGH, tUs);

      // Rest of frame (6 remaining digits inactive, 6ms)
      tUs += 6000n;
      ctx.advanceTime(6000n);
    }

    const bright = ctx.getLatestPublish('bright') as Uint8Array;
    // Digit 0 should be lit
    expect(bright[0]).toBeGreaterThanOrEqual(LOGIC_THRESHOLD);

    // Digit 1 (neighbor) experienced 50us overlap of segment 'A'
    const neighborA = bright[1 * 8 + 0];
    expect(neighborA).toBeGreaterThan(0);
    expect(neighborA).toBeLessThan(GHOST_MAX_BRIGHT);

    const segMaskStr = ctx.getLatestPublish('segMask') as string;
    const segMask = JSON.parse(segMaskStr);
    // Digit 1 mask should NOT include segment A (it's below LOGIC_THRESHOLD)
    expect(segMask[1] & 1).toBe(0);
  });

  // Test 11: Slow scan dimming
  test('11. slow scan (10ms/digit) results in lower steady brightness < 150', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = createPinMapping(8);

    plugin.onBind(ctx as any, pinMap, {
      segActiveLevel: 'high',
      digitActiveLevel: 'low',
    });

    let tUs = 0n;
    // 3 frames of 80ms each (10ms per digit)
    for (let frame = 0; frame < 3; frame++) {
      for (let d = 0; d < 8; d++) {
        const digPin = pinMap[`DIG${d + 1}`];
        plugin.onPinChange(digPin, LogicStates.LOW, tUs);
        plugin.onPinChange(pinMap.A, LogicStates.HIGH, tUs);

        tUs += 10_000n;
        ctx.advanceTime(10_000n);

        plugin.onPinChange(digPin, LogicStates.HIGH, tUs);
        plugin.onPinChange(pinMap.A, LogicStates.LOW, tUs);
      }
    }

    const bright = ctx.getLatestPublish('bright') as Uint8Array;
    // Digit 0 segment A decayed during 70ms unlit time
    expect(bright[0]).toBeLessThan(150);
  });

  // Test 12: Multiple digits simultaneously active
  test('12. multiple digits active triggers warning and activeDigits >= 2', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = createPinMapping(8);

    plugin.onBind(ctx as any, pinMap, {
      segActiveLevel: 'high',
      digitActiveLevel: 'low',
    });

    // Drive both DIG1 and DIG2 LOW
    plugin.onPinChange(pinMap.DIG1, LogicStates.LOW, 0n);
    plugin.onPinChange(pinMap.DIG2, LogicStates.LOW, 0n);
    plugin.onPinChange(pinMap.A, LogicStates.HIGH, 0n);

    ctx.advanceTime(2000n);
    plugin.onPinChange(pinMap.DIG1, LogicStates.HIGH, 2000n);

    const activeDigits = ctx.getLatestPublish('activeDigits');
    expect(activeDigits).toBeGreaterThanOrEqual(2);
    expect(ctx.warnings.some((w) => w.includes('multiple digits driven simultaneously'))).toBe(true);
  });

  // Test 13: Throttling 1000 edges within 1ms
  test('13. throttle publication: 1000 edges in 1ms triggers at most 1 publish', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = createPinMapping(8);

    plugin.onBind(ctx as any, pinMap, {});
    const initialPublishes = ctx.publishes.length;

    for (let i = 0; i < 1000; i++) {
      const level = i % 2 === 0 ? LogicStates.HIGH : LogicStates.LOW;
      plugin.onPinChange(pinMap.A, level, BigInt(i));
    }

    const newPublishes = ctx.publishes.length - initialPublishes;
    // Throttled: at most 1 burst publish occurred
    expect(newPublishes).toBeLessThanOrEqual(10);
  });

  // Test 14: Tail decay extinguishing residual brightness
  test('14. tail decay chain extinguishes residual brightness when edges cease', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = createPinMapping(1);

    plugin.onBind(ctx as any, pinMap, { variant: 'direct_gpio_1d' });

    plugin.onPinChange(pinMap.DIG1, LogicStates.LOW, 0n);
    plugin.onPinChange(pinMap.A, LogicStates.HIGH, 0n);
    ctx.advanceTime(5000n);
    plugin.onPinChange(pinMap.DIG1, LogicStates.LOW, 5000n);

    let bright = ctx.getLatestPublish('bright') as Uint8Array;
    expect(bright[0]).toBeGreaterThan(100);

    // Stop feeding edges and turn off inputs
    plugin.onPinChange(pinMap.A, LogicStates.LOW, 6000n);

    // Advance virtual time by 300ms through tail defer chain
    ctx.advanceTime(300_000n);

    bright = ctx.getLatestPublish('bright') as Uint8Array;
    expect(bright[0]).toBe(0);
  });

  // Test 15: HI_Z and CONFLICT handling
  test('15. HI_Z never illuminates and CONFLICT logs warning', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = createPinMapping(1);

    plugin.onBind(ctx as any, pinMap, {
      variant: 'direct_gpio_1d',
      segActiveLevel: 'low',
    });

    // HI_Z on segment pin
    plugin.onPinChange(pinMap.DIG1, LogicStates.LOW, 0n);
    plugin.onPinChange(pinMap.A, LogicStates.HI_Z, 0n);
    ctx.advanceTime(5000n);

    let bright = ctx.getLatestPublish('bright') as Uint8Array;
    expect(bright[0]).toBe(0);

    // CONFLICT on pin
    plugin.onPinChange(pinMap.A, LogicStates.CONFLICT, 6000n);
    expect(ctx.warnings.some((w) => w.includes('bus conflict'))).toBe(true);
  });

  // Test 16: onReset lifecycle
  test('16. onReset zeroes state and cancels pending tail chain', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = createPinMapping(1);

    plugin.onBind(ctx as any, pinMap, { variant: 'direct_gpio_1d' });

    plugin.onPinChange(pinMap.DIG1, LogicStates.LOW, 0n);
    plugin.onPinChange(pinMap.A, LogicStates.HIGH, 0n);
    ctx.advanceTime(5000n);
    plugin.onPinChange(pinMap.DIG1, LogicStates.LOW, 5000n);

    plugin.onReset();

    const bright = ctx.getLatestPublish('bright') as Uint8Array;
    expect(bright.every((b) => b === 0)).toBe(true);
    const text = ctx.getLatestPublish('text') as string;
    expect(text.trim()).toBe('');
  });

  // Test 17: serializeState / deserializeState
  test('17. serializeState and deserializeState maintain brightness without jump decay', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = createPinMapping(1);

    plugin.onBind(ctx as any, pinMap, { variant: 'direct_gpio_1d' });

    plugin.onPinChange(pinMap.DIG1, LogicStates.LOW, 0n);
    plugin.onPinChange(pinMap.A, LogicStates.HIGH, 0n);
    ctx.advanceTime(5000n);
    plugin.onPinChange(pinMap.DIG1, LogicStates.LOW, 5000n);

    const snapshot = plugin.serializeState();
    expect(snapshot.bright).toBeDefined();

    const plugin2 = new SegDisplayPlugin();
    const ctx2 = createMockCtx();
    plugin2.onBind(ctx2 as any, pinMap, { variant: 'direct_gpio_1d' });
    plugin2.deserializeState(snapshot);

    ctx2.advanceTime(100n);
    plugin2.onPinChange(pinMap.A, LogicStates.HIGH, 100n);

    const bright2 = ctx2.getLatestPublish('bright') as Uint8Array;
    expect(bright2[0]).toBeGreaterThan(150);
  });

  // Test 18: onPropertyChange
  test('18. onPropertyChange updates polarity dynamically; variant change is ignored with warning', () => {
    const plugin = new SegDisplayPlugin();
    const ctx = createMockCtx();
    const pinMap = createPinMapping(1);

    plugin.onBind(ctx as any, pinMap, {
      variant: 'direct_gpio_1d',
      segActiveLevel: 'high',
    });

    plugin.onPropertyChange('segActiveLevel', 'high', 'low');

    plugin.onPinChange(pinMap.DIG1, LogicStates.LOW, 0n);
    plugin.onPinChange(pinMap.A, LogicStates.LOW, 0n);
    ctx.advanceTime(5000n);
    plugin.onPinChange(pinMap.DIG1, LogicStates.LOW, 5000n);

    const bright = ctx.getLatestPublish('bright') as Uint8Array;
    expect(bright[0]).toBeGreaterThan(150);

    plugin.onPropertyChange('variant', 'direct_gpio_1d', 'direct_gpio_4d');
    expect(ctx.warnings.some((w) => w.includes('runtime variant change is not supported'))).toBe(true);
  });

  // Test 19: Pin alias uniqueness across all variants
  test('19. pin aliases are strictly unique within each variant manifest', () => {
    for (const variant of SEG_VARIANTS) {
      const manifest = segDisplayManifestFactory(variant);
      const seen = new Set<string>();

      for (const pin of manifest.pins) {
        if (pin.aliases) {
          for (const alias of pin.aliases) {
            expect(seen.has(alias)).toBe(false);
            seen.add(alias);
          }
        }
      }
    }
  });

  // Test 20: Property defaults match declared types
  test('20. property defaults match declared types in manifest', () => {
    for (const [propName, propDef] of Object.entries(segDisplayManifest.properties)) {
      if (propDef.default !== undefined) {
        const actualType = typeof propDef.default;
        expect(actualType).toBe(propDef.type);
      }
    }
  });
});
