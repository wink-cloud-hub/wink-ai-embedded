// Minimal emscripten JS library for the mcs51 wasm node tests (M1 blinky,
// M2 timer0). The real wink_sim_stub.js uses CommonJS require()
// (worker_threads/fs) which emcc's library evaluator rejects under ESM; the
// fiber scheduler path never calls these js_ imports (they only fire on the
// main thread with s_main_ctx==NULL), so no-op stubs suffice for the bounded
// node test.
//
// js_pal_os_busy_wait_us: production bridge (wink_sim_js.js) advances the
// platform virtual clock 1:1, async via the Worker. The bounded node test
// mirrors that synchronously by calling the exported clock entry — otherwise
// a duration-0 quota yield parks the fiber at wakeup 0 and it never resumes
// (the mcs51 layer bills 1 ms master per 1 ms virtual, AD-14, same as host).
mergeInto(LibraryManager.library, {
  js_pal_os_sleep_ms: function (ms) {},
  js_pal_os_busy_wait_us: function (us) {
    if (typeof Module !== 'undefined' &&
        typeof Module['_pal_wasm_advance_virtual_clock'] === 'function') {
      Module['_pal_wasm_advance_virtual_clock'](BigInt(us));
    }
  },
  js_pal_log_write: function (ptr, len) {},
  js_pal_log_vprintf: function (level, ptr) {},
  js_pal_log: function (level, msgPtr) {},
  // No external interrupts in the bounded test: drain loop stops immediately.
  js_pal_poll_interrupt: function (outCbPtr, outArgPtr) { return false; },
  js_pal_notify_pin_edge: function (pin, level, tUs) {},
  // M4 UniSim channels: channel-1 instant pin-edge notify is a no-op (edge
  // dispatch is asserted host-side via the pin traps), and channel-3 analog
  // pulls read 0.0 — bounded tests inject deterministically via the C rail
  // (mcs51_adc_set_value), so no JS-side analog source is needed.
  js_pal_gpio_write: function (pin, level) {},
  js_pal_adc_read_norm: function (pin) { return 0.0; },
});
