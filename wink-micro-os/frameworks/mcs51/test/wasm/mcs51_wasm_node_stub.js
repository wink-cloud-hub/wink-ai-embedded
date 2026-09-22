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
  js_pal_gpio_write: function (pin, level, strength) {},
  // P3: MCU-driver release (TRIS input / open-drain / reset) — no arbiter in
  // the bounded node test, no-op like the write channel.
  js_pal_gpio_release_mcu: function (pin) {},
  js_pal_adc_read_norm: function (pin) { return 0.0; },
  // CH2 Phase 2 bus sessions (ADR-0085/0086/0087): the bounded node tests never
  // drive I2C/SPI, so the status-returning entry points fail closed
  // (WINK_ERR_UNSUPPORTED = -7; close is idempotently WINK_OK). The production
  // library is wink_sim_js.js, where these forward to the UniSim engine.
  js_pal_i2c_transfer_ex: function (port, addr, wbuf, wlen, rbuf, rlen, resultPtr) {
    return -7;
  },
  js_pal_i2c_session_open: function (port, addr, direction, outSessionPtr, resultPtr) {
    return -7;
  },
  js_pal_i2c_session_restart: function (sessionId, addr, direction, resultPtr) {
    return -7;
  },
  js_pal_i2c_session_write: function (sessionId, bufPtr, len, resultPtr) {
    return -7;
  },
  js_pal_i2c_session_read: function (sessionId, bufPtr, len, ackMode, resultPtr) {
    return -7;
  },
  js_pal_i2c_session_close: function (sessionId) { return 0; },
  js_pal_spi_transfer_ex: function (port, deviceId, txbuf, len, rxbuf, mode, sckHz) {
    return -7;
  },
  js_pal_spi_session_open: function (port, deviceId, mode, sckHz, outSessionPtr) {
    return -7;
  },
  js_pal_spi_session_transfer: function (sessionId, txbuf, rxbuf, len) {
    return -7;
  },
  js_pal_spi_session_close: function (sessionId) { return 0; },
  // Channel-2 UART TX (SBUF write -> UARTBus): copy the byte run off the
  // WASM heap into a Module log the Node driver can assert.
  js_pal_uart_write: function (port, bufPtr, len) {
    if (typeof Module !== 'undefined') {
      if (!Module['mcs51UartTxLog']) Module['mcs51UartTxLog'] = [];
      const heap = (typeof HEAPU8 !== 'undefined') ? HEAPU8 : (Module && Module.HEAPU8);
      const bytes = heap ? Array.from(heap.subarray(bufPtr, bufPtr + len)) : [];
      Module['mcs51UartTxLog'] = Module['mcs51UartTxLog'].concat(bytes);
      // Optional C-side sink (kept alive by -sEXPORTED_FUNCTIONS on the
      // test): lets the driver assert the live route from C (exit-code gate),
      // mirroring the mcs51_wasm_ext_pin_state callback direction.
      if (typeof Module['_mcs51_wasm_uart_accept_byte'] === 'function') {
        for (const b of bytes) Module['_mcs51_wasm_uart_accept_byte'](b);
      }
    }
  },
  // Channel-1 read direction (external digital level, e.g. a button plugin via
  // PinArbiter): delegate to the test driver's exported getter; absent (the
  // other wasm tests) return 2 = HiZ so the proxy falls back to the latch.
  js_pal_gpio_read_state: function (pin) {
    if (typeof Module !== 'undefined' &&
        typeof Module['_mcs51_wasm_ext_pin_state'] === 'function') {
      return Module['_mcs51_wasm_ext_pin_state'](pin);
    }
    return 2; /* HiZ default — fall back to latch */
  },
});
