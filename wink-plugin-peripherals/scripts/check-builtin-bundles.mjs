#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-only
/**
 * check-builtin-bundles.mjs — programmatic load smoke for the builtin peripheral
 * simulation bundles (Phase 4/5 browser-acceptance companion).
 *
 * The three demo apps (mcs51_health_pot / oled_dashboard / avoidance_car) cover
 * the browser blob path for the 8 UI-capable builtins. This smoke closes the
 * remaining matrix programmatically, without a browser:
 *
 *   1. every builtin `dist/simulation.js` imports cleanly under Node ESM;
 *   2. no legacy `@wink-ai/unisim` / `@unisim` specifier survives (zero
 *      compatibility), and only the open SDK keys are referenced;
 *   3. each bundle exposes an SDK plugin class whose manifest type matches the
 *      directory, and the class identity is shared with the SDK package loaded
 *      here (extends the same `BaseSimulationPlugin`);
 *   4. `createPluginStubHost().bind()` succeeds with a pin mapping derived from
 *      the manifest and produces state channels — this also exercises the
 *      `registerI2cDevice`/UART/buffer contract members added in SDK 1.0.1.
 *
 * Usage: node scripts/check-builtin-bundles.mjs [--json]
 */
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

import { BaseSimulationPlugin, createPluginStubHost } from '@wink-ai/unisim-sdk';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const BUILTIN_DIR = join(ROOT, 'builtin');
const VERSION_RE = /^\d+\.\d+\.\d+$/;

const SDK_PREFIX = '@wink-ai/unisim-sdk';
const LEGACY_RE = /^@wink-ai\/unisim(\/|$)|^@unisim(\/|$)/;

const SPEC_RE =
  /\bfrom\s*['"]([^'"]+)['"]|\bimport\s*\(\s*['"]([^'"]+)['"]\s*\)|\bimport\s+['"]([^'"]+)['"]/g;

function listBundles() {
  const bundles = [];
  for (const type of readdirSync(BUILTIN_DIR).sort()) {
    const typeDir = join(BUILTIN_DIR, type);
    if (!statSync(typeDir).isDirectory()) continue;
    for (const version of readdirSync(typeDir).sort()) {
      if (!VERSION_RE.test(version)) continue;
      const distFile = join(typeDir, version, 'dist', 'simulation.js');
      try {
        if (statSync(distFile).isFile()) {
          bundles.push({ type, version, distFile });
        }
      } catch {
        // not built for this version dir
      }
    }
  }
  return bundles;
}

function extractSpecifiers(code) {
  const found = new Set();
  for (const match of code.matchAll(SPEC_RE)) {
    const spec = match[1] ?? match[2] ?? match[3];
    if (spec) found.add(spec);
  }
  return [...found];
}

function findPluginClass(mod) {
  const candidates = Object.values(mod).filter(
    value =>
      typeof value === 'function' &&
      value.prototype &&
      typeof value.prototype.onBind === 'function',
  );
  return (
    candidates.find(value => value.prototype instanceof BaseSimulationPlugin) ??
    candidates[0] ??
    null
  );
}

async function checkBundle({ type, version, distFile }) {
  const problems = [];
  const code = readFileSync(distFile, 'utf-8');

  // 1. Specifier hygiene: only the open SDK, no legacy engine keys.
  const specifiers = extractSpecifiers(code);
  for (const spec of specifiers) {
    if (LEGACY_RE.test(spec)) {
      problems.push(`legacy engine specifier survived: ${spec}`);
    } else if (spec !== SDK_PREFIX && !spec.startsWith(`${SDK_PREFIX}/`)) {
      problems.push(`unexpected bare specifier (not in the vendor allowlist): ${spec}`);
    }
  }

  // 2. Load + class identity (same SDK module instance as this script).
  let klass = null;
  let manifestType = null;
  try {
    const mod = await import(pathToFileURL(distFile).href);
    klass = findPluginClass(mod);
    if (!klass) {
      problems.push('no plugin class (prototype.onBind) among module exports');
    } else {
      if (!(klass.prototype instanceof BaseSimulationPlugin)) {
        problems.push('plugin class does not extend the SDK BaseSimulationPlugin (identity fork)');
      }
      manifestType = klass.manifest?.type ?? null;
      if (manifestType !== type) {
        problems.push(`manifest.type=${manifestType ?? 'missing'} does not match directory "${type}"`);
      }
    }
  } catch (err) {
    problems.push(`import failed: ${err.message.split('\n')[0]}`);
  }

  // 3. Stub-host bind + state channels.
  let bound = false;
  if (klass && !problems.length) {
    try {
      const host = createPluginStubHost();
      const pins = Array.isArray(klass.manifest?.pins) ? klass.manifest.pins : [];
      const pinMapping = Object.fromEntries(pins.map((pin, idx) => [pin.name, 100 + idx]));
      const instanceId = `${type}:0`;
      host.bind(klass, { instanceId, pinMapping });
      const snapshot = host.getStateSnapshot();
      bound = Boolean(snapshot?.[instanceId]);
      if (!bound) problems.push('bind() produced no state channels');
      await host.destroy();
    } catch (err) {
      problems.push(`bind failed: ${err.message.split('\n')[0]}`);
    }
  }

  return { type, version, specifiers, klass: klass?.name ?? null, manifestType, bound, problems };
}

async function main() {
  const asJson = process.argv.includes('--json');
  const bundles = listBundles();
  if (bundles.length === 0) {
    console.error('[check-builtin-bundles] no built simulation bundles found; build the suite first.');
    process.exit(1);
  }

  const results = [];
  for (const bundle of bundles) {
    results.push(await checkBundle(bundle));
  }

  const failed = results.filter(result => result.problems.length > 0);
  if (asJson) {
    console.log(JSON.stringify({ ok: failed.length === 0, results }, null, 2));
  } else {
    console.log('\n' + '='.repeat(72));
    console.log(' Builtin peripheral bundle load smoke (programmatic)');
    console.log('='.repeat(72));
    for (const result of results) {
      const status = result.problems.length === 0 ? '✔' : '✗';
      console.log(
        `  ${status} ${result.type.padEnd(14)} class=${String(result.klass).padEnd(20)} ` +
          `manifest=${result.manifestType} bound=${result.bound} ` +
          `specifiers=[${result.specifiers.join(', ') || '-'}]`,
      );
      for (const problem of result.problems) console.log(`      - ${problem}`);
    }
    console.log('='.repeat(72));
    console.log(
      failed.length === 0
        ? ` ✅ ${results.length}/${results.length} bundles load, bind and stay SDK-identity clean`
        : ` ❌ ${failed.length}/${results.length} bundle(s) failed`,
    );
    console.log('='.repeat(72) + '\n');
  }

  process.exit(failed.length === 0 ? 0 : 1);
}

main().catch(err => {
  console.error(`[check-builtin-bundles] ❌ ${err.message}`);
  process.exit(1);
});
