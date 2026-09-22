/**
 * emit-sim-manifests.mjs — Emit `dist/manifest.json` for simulation-only
 * builtin plugins from their `src/simulation.ts` manifest export.
 *
 * UI plugins get `dist/manifest.json` from their vite UI build; simulation-only
 * plugins (no `src/definition.ts`) only produced `dist/simulation.js`, so
 * `winkcli build sim` could not resolve their pin roles/properties during
 * device-tree generation ("<type> has no registered Manifest").
 *
 * Run with bun (imports TypeScript directly):
 *   bun scripts/emit-sim-manifests.mjs
 */
import { existsSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";

const root = resolve(import.meta.dir, "..");
const builtinDir = join(root, "builtin");

let emitted = 0;
for (const type of readdirSync(builtinDir)) {
  const typeDir = join(builtinDir, type);
  if (!statSync(typeDir).isDirectory()) continue;
  for (const version of readdirSync(typeDir)) {
    const versionDir = join(typeDir, version);
    if (!statSync(versionDir).isDirectory()) continue;

    const src = join(versionDir, "src", "simulation.ts");
    const dist = join(versionDir, "dist");
    const simConfig = join(versionDir, "vite.config.sim.ts");
    const uiDefinition = join(versionDir, "src", "definition.ts");
    // Only simulation-only plugins: UI plugins own their manifest artifact.
    if (!existsSync(src) || !existsSync(dist) || !existsSync(simConfig)) continue;
    if (existsSync(uiDefinition)) continue;

    let manifest;
    try {
      const mod = await import(new URL(`file://${src}`).href);
      manifest = mod.default?.manifest ?? mod.manifest;
    } catch (err) {
      console.warn(`[emit-manifest] ${type}@${version}: import failed: ${err}`);
      continue;
    }
    if (!manifest) {
      console.warn(`[emit-manifest] ${type}@${version}: no manifest export`);
      continue;
    }

    const out = {
      _generated:
        "DO NOT EDIT - generated from src/simulation.ts by scripts/emit-sim-manifests.mjs",
      ...manifest,
    };
    writeFileSync(join(dist, "manifest.json"), JSON.stringify(out, null, 2) + "\n");
    emitted++;
    console.log(`[emit-manifest] wrote ${type}@${version}/dist/manifest.json`);
  }
}
console.log(`[emit-manifest] done: ${emitted} manifest(s)`);
