/**
 * wink_sim_js.js — Default JS-library implementation for Wasm `extern js_*` symbols (ADR-0019 wrapper pattern).
 *
 * Injected into wink_simulator.js glue layer via `--js-library=<this>` during emcc compilation.
 * Provides default implementations for all `js_pal_*` / `js_sim_*` extern symbols declared in wasm_bridge.h.
 */

addToLibrary({
    /* ---- Asyncify Yield Point ---- */
    js_pal_os_sleep_ms__deps: ['pal_wasm_advance_virtual_clock'],
    js_pal_os_sleep_ms__postset: "Module['setSimMode'] = function (mode) { var v = (mode === 'HEADLESS') ? 1 : 0; if (typeof Module['_pal_wasm_set_sim_mode'] === 'function') { Module['_pal_wasm_set_sim_mode'](v); } };",
    js_pal_os_sleep_ms: function (ms) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_os_sleep_ms'] === 'function' && Module['js_pal_os_sleep_ms'] !== _js_pal_os_sleep_ms) {
            return Asyncify.handleSleep(function(wakeUp) {
                Promise.resolve(Module['js_pal_os_sleep_ms'](ms)).then(function() {
                    wakeUp();
                });
            });
        }
        var advanceUs = BigInt(ms) * 1000n;
        return Asyncify.handleSleep(function(wakeUp) {
            setTimeout(function() {
                try {
                    _pal_wasm_advance_virtual_clock(advanceUs);
                } catch (_e1) {
                    try { _pal_wasm_advance_virtual_clock(Number(advanceUs)); } catch (_e2) {}
                }
                wakeUp();
            }, ms);
        });
    },

    js_pal_os_busy_wait_us__deps: ['pal_wasm_advance_virtual_clock'],
    js_pal_os_busy_wait_us: function (us) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_os_busy_wait_us'] === 'function' && Module['js_pal_os_busy_wait_us'] !== _js_pal_os_busy_wait_us) {
            return Asyncify.handleSleep(function(wakeUp) {
                Promise.resolve(Module['js_pal_os_busy_wait_us'](us)).then(function() {
                    wakeUp();
                });
            });
        }
        // VirtualClock advances by the exact BigInt(us) value — determinism is preserved regardless
        // of the wall-clock setTimeout delay. For sub-ms durations (us < 1000), use a 0ms timeout
        // (next event-loop tick) rather than a forced 1ms wait. This prevents I²C/SPI bit-bang
        // simulations from running 2-10x slower than the equivalent real MCU timing.
        var advanceUs = BigInt(us);
        var waitMs = us >= 1000 ? Math.floor(us / 1000) : 0;
        return Asyncify.handleSleep(function(wakeUp) {
            setTimeout(function() {
                try {
                    _pal_wasm_advance_virtual_clock(advanceUs);
                } catch (_e1) {
                    try { _pal_wasm_advance_virtual_clock(Number(advanceUs)); } catch (_e2) {}
                }
                wakeUp();
            }, waitMs);
        });
    },

    /* ---- Leveled Logging Bridge ---- */
    js_pal_log: function (level, msgPtr) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_log'] === 'function' && Module['js_pal_log'] !== _js_pal_log) {
            return Module['js_pal_log'](level, msgPtr);
        }
        var msg = UTF8ToString(msgPtr);
        switch (level) {
            case 1: (console.error || console.log).call(console, '[wink E] ' + msg); break;
            case 2: (console.warn  || console.log).call(console, '[wink W] ' + msg); break;
            case 3: (console.info  || console.log).call(console, '[wink I] ' + msg); break;
            case 4: /* debug: muted by default in fallback stub */ break;
            default: console.log('[wink ?] ' + msg); break;
        }
    },

    /* ---- PAL HAL Defaults (No-Op Stub) ---- */
    js_pal_gpio_write: function (pin, level) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_gpio_write'] === 'function' && Module['js_pal_gpio_write'] !== _js_pal_gpio_write) {
            return Module['js_pal_gpio_write'](pin, level);
        }
    },
    js_pal_gpio_read_state: function (pin) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_gpio_read_state'] === 'function' && Module['js_pal_gpio_read_state'] !== _js_pal_gpio_read_state) {
            return Module['js_pal_gpio_read_state'](pin);
        }
        return 2; /* HiZ default */
    },
    js_pal_gpio_drive_ideal: function (pin, level) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_gpio_drive_ideal'] === 'function' && Module['js_pal_gpio_drive_ideal'] !== _js_pal_gpio_drive_ideal) {
            return Module['js_pal_gpio_drive_ideal'](pin, level);
        }
    },
    js_pal_gpio_release_ideal: function (pin) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_gpio_release_ideal'] === 'function' && Module['js_pal_gpio_release_ideal'] !== _js_pal_gpio_release_ideal) {
            return Module['js_pal_gpio_release_ideal'](pin);
        }
    },
    js_pal_gpio_release_mcu: function (pin) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_gpio_release_mcu'] === 'function' && Module['js_pal_gpio_release_mcu'] !== _js_pal_gpio_release_mcu) {
            return Module['js_pal_gpio_release_mcu'](pin);
        }
    },
    js_pal_pwm_set_duty: function (channel, duty) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_pwm_set_duty'] === 'function' && Module['js_pal_pwm_set_duty'] !== _js_pal_pwm_set_duty) {
            return Module['js_pal_pwm_set_duty'](channel, duty);
        }
    },
    js_pal_i2c_transfer: function (port, addr, wbuf, wlen, rbuf, rlen) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_i2c_transfer'] === 'function' && Module['js_pal_i2c_transfer'] !== _js_pal_i2c_transfer) {
            return Module['js_pal_i2c_transfer'](port, addr, wbuf, wlen, rbuf, rlen);
        }
        return 1;
    },
    js_pal_spi_transfer: function (port, deviceId, txbuf, len, rxbuf, mode, sckHz) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_spi_transfer'] === 'function' && Module['js_pal_spi_transfer'] !== _js_pal_spi_transfer) {
            return Module['js_pal_spi_transfer'](port, deviceId, txbuf, len, rxbuf, mode, sckHz);
        }
        return 0;
    },
    js_pal_uart_write: function (port, buf, len) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_uart_write'] === 'function' && Module['js_pal_uart_write'] !== _js_pal_uart_write) {
            return Module['js_pal_uart_write'](port, buf, len);
        }
    },

    /* ---- CH3: Analog ADC (normalised [0,1] read) ---- */
    js_pal_adc_read_norm: function (pin) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_adc_read_norm'] === 'function' && Module['js_pal_adc_read_norm'] !== _js_pal_adc_read_norm) {
            return Module['js_pal_adc_read_norm'](pin);
        }
        return 0.0;
    },

    /* ---- CH4: WS2812 framebuffer (zero-copy; JS must .slice() before return) ---- */
    js_pal_ws2812_write: function (pin, buf, len) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_ws2812_write'] === 'function' && Module['js_pal_ws2812_write'] !== _js_pal_ws2812_write) {
            return Module['js_pal_ws2812_write'](pin, buf, len);
        }
    },

    /* ---- Interrupt Bridge Poll Model ---- */
    js_pal_register_interrupt: function (pin, cbIdx, argPtr) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_register_interrupt'] === 'function' && Module['js_pal_register_interrupt'] !== _js_pal_register_interrupt) {
            return Module['js_pal_register_interrupt'](pin, cbIdx, argPtr);
        }
    },
    js_pal_deregister_interrupt: function (pin) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_deregister_interrupt'] === 'function' && Module['js_pal_deregister_interrupt'] !== _js_pal_deregister_interrupt) {
            return Module['js_pal_deregister_interrupt'](pin);
        }
    },
    js_pal_poll_interrupt: function (outCbPtr, outArgPtr) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_poll_interrupt'] === 'function' && Module['js_pal_poll_interrupt'] !== _js_pal_poll_interrupt) {
            return Module['js_pal_poll_interrupt'](outCbPtr, outArgPtr);
        }
        return 0;
    },

    /* ---- GPIO Notify Bridge ---- */
    js_pal_gpio_on_write: function (pin, level) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_gpio_on_write'] === 'function' && Module['js_pal_gpio_on_write'] !== _js_pal_gpio_on_write) {
            return Module['js_pal_gpio_on_write'](pin, level);
        }
    },

    /* ---- Plugin Channel API ---- */
    js_sim_get_plugin_channel: function (instanceIdPtr, channelNamePtr) {
        if (typeof Module !== 'undefined' && typeof Module['js_sim_get_plugin_channel'] === 'function' && Module['js_sim_get_plugin_channel'] !== _js_sim_get_plugin_channel) {
            return Module['js_sim_get_plugin_channel'](instanceIdPtr, channelNamePtr);
        }
        return -1.0; /* Uninitialized sentinel */
    },

    /* ---- Sub-step Waveform & Ultrasonic Event Bridges ---- */
    js_pal_notify_pin_edge: function (pin, level, tUs) {
        if (typeof Module !== 'undefined' && typeof Module['js_pal_notify_pin_edge'] === 'function' && Module['js_pal_notify_pin_edge'] !== _js_pal_notify_pin_edge) {
            return Module['js_pal_notify_pin_edge'](pin, level, tUs);
        }
    },
    wink_ultrasonic_distance_events_trigger_now_by_trig_pin: function (pin) {
        if (typeof Module !== 'undefined' && typeof Module['wink_ultrasonic_distance_events_trigger_now_by_trig_pin'] === 'function' && Module['wink_ultrasonic_distance_events_trigger_now_by_trig_pin'] !== _wink_ultrasonic_distance_events_trigger_now_by_trig_pin) {
            return Module['wink_ultrasonic_distance_events_trigger_now_by_trig_pin'](pin);
        }
    },
});
