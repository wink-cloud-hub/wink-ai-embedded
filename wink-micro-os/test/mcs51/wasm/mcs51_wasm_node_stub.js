// Minimal emscripten JS library for the M1 mcs51 wasm node test. The real
// wink_sim_stub.js uses CommonJS require() (worker_threads/fs) which emcc's
// library evaluator rejects under ESM; the fiber scheduler path never calls
// these js_ imports (they only fire on the main thread with s_main_ctx==NULL),
// so no-op stubs suffice for the bounded node test.
mergeInto(LibraryManager.library, {
  js_pal_os_sleep_ms: function (ms) {},
  js_pal_log_write: function (ptr, len) {},
  js_pal_log_vprintf: function (level, ptr) {},
  js_pal_log: function (level, msgPtr) {},
  // No external interrupts in the bounded test: drain loop stops immediately.
  js_pal_poll_interrupt: function (outCbPtr, outArgPtr) { return false; },
  js_pal_notify_pin_edge: function (pin, level, tUs) {},
});
