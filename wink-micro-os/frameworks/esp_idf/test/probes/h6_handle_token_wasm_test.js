/* SPDX-License-Identifier: GPL-3.0-only */
const assert = require('node:assert/strict');
const fs = require('node:fs');
const wasmModule = new WebAssembly.Module(fs.readFileSync(process.argv[2]));
function createModule() {
  const memory = new WebAssembly.Memory({ initial: 256 });
  return new WebAssembly.Instance(wasmModule, { env: { memory } }).exports;
}

try {
  const first = createModule();
  assert.equal(first.h6_create(1, 0), 0);
  assert.equal(first.h6_init(0), 1);
  assert.equal(first.h6_init(0), 0);
  const old = first.h6_create(1, 63) >>> 0;
  assert.notEqual(old, 0);
  assert.equal(first.h6_valid(1, old), 1);
  assert.equal(first.h6_create(1, 64), 0);
  assert.equal(first.h6_delete(1, old), 1);
  const reused = first.h6_create(1, 63) >>> 0;
  assert.notEqual(reused, old);
  assert.equal(first.h6_valid(1, old), 0);
  assert.equal(first.h6_valid(2, reused), 0);
  first.h6_reset();
  assert.equal(first.h6_valid(1, reused), 0);
  const beforeReplacement = first.h6_create(1, 63) >>> 0;
  const handoff = first.h6_sequence() >>> 0;

  const second = createModule(); // A new Wasm memory and static pool.
  assert.equal(second.h6_init(handoff), 1);
  const afterReplacement = second.h6_create(1, 63) >>> 0;
  assert.notEqual(afterReplacement, beforeReplacement);
  assert.equal(second.h6_valid(1, beforeReplacement), 0);
  assert.equal(second.h6_valid(1, afterReplacement), 1);

  const third = createModule();
  assert.equal(third.h6_init((1 << 22) - 2), 1);
  assert.notEqual(third.h6_create(7, 63) >>> 0, 0);
  assert.equal(third.h6_create(7, 62), 0);
  console.log('H6 token probe passed: Wasm32, real module replacement');
} catch (error) {
  console.error(error);
  process.exitCode = 1;
}
