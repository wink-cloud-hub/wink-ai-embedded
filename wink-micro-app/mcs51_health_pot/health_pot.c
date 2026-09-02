/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 health-pot (养生壶) thermostat — UNMODIFIED-STYLE Keil C51 user
 * source for the Wink MCS-51 zero-intrusion simulation sandbox.
 *
 * Appliance profile:
 *   - NTC temperature probe through an external ADC0832 (3-wire bit-bang,
 *     CS=P2.0 CLK=P2.1 DIO=P2.2; NTC pulled up to VREF => ADC code is HIGH
 *     when cold, LOW when hot, same convention as the iron_ntc sample).
 *     All readings use a median-of-3 filter (adc_read_filtered) to reject
 *     single-sample noise from supply ripple and relay transients.
 *   - Heater relay on P1.0 (active high).
 *   - Active buzzer on P1.1 (active high).
 *   - Three indicator LEDs on P1.2/P1.3/P1.4 (active low, 0 = lit):
 *     heat / keep-warm / fault.
 *   - Two push buttons on P3.2 (ON/OFF, also INT0) and P3.3 (FUNC, also
 *     INT1), active low, 20 ms software debounce in the super-loop.
 *   - Timer0 mode-1 10 ms tick ISR (12 MHz teaching crystal, 1 count = 1 us,
 *     reload 65536-10000 = 0xD8F0).
 *   - Timer1 mode-2 (auto-reload) baud-rate generator for UART mode-1 TX:
 *     9600 bps @ 11.0592 MHz crystal (TH1 = 0xFD).  The simulation model
 *     sets TI synchronously without baud-rate validation, so Timer1 is
 *     functionally inert under sim but mandatory for real 8051 silicon.
 *   - UART mode-1 polled TX telemetry once per second:
 *       "T=<temp>C,S=<state>,H=<heater>,F=<fault>\n"
 *     state: 0=OFF 1=HEAT(boil) 2=WARM 3=FAULT; H: 0/1 heater drive.
 *
 * State machine:
 *   OFF  --press ON/OFF-->  HEAT  (boil to >=98 C, hold 3 s)  -->  WARM
 *   HEAT/WARM --press ON/OFF--> OFF
 *   WARM: FUNC cycles the keep-warm setpoint 60 -> 80 -> 90 -> 60 C with
 *         +/-3 C hysteresis on the heater.  A relay dwell (min-off-time,
 *         RELAY_DWELL_SECONDS) gates only the WARM re-energize edge: the
 *         heater must have been OFF >= 3 s (sim; 30-60 s on a real product)
 *         before it may pull in again, extending mechanical-relay contact
 *         life.  Every OFF edge (fault / ON-OFF / boil cut-off / too-hot) is
 *         immediate and is never dwell-gated.
 *   FAULT (heater forced off, fault LED blinks 1 Hz, buzzer chirps every
 *          second for up to 60 s then silences — LED continues blinking):
 *     1 = NTC open   (code >= 250)            sensor fault, auto-returns to
 *     2 = NTC short  (code <= 8)              OFF after 3 consecutive valid
 *                                             samples (300 ms debounce)
 *     3 = dry-fire   (heater on > 25 s while  THERMAL fault, latched, manual
 *                     temp stays < 45 C)      ON/OFF reset only; never
 *     4 = over-temp  (code <= 20 for 10 s in  downgraded by a sensor code.
 *                     HEAT/WARM: plate has    Dry-fire covers HEAT with no
 *                     run away beyond scale)  water; over-temp covers WARM
 *                                             (long keep-warm boiled dry).
 *   Boil confirmation is time-only: once >= 98 C is first seen the heater
 *   stays off for 3 s regardless of temperature dips (thermal lag / NTC
 *   position would otherwise reset the hold and livelock the relay).
 *
 * Watchdog: wdt_init()/wdt_feed() drive the STC12 WDT_CONTR SFR (0xE1) under
 *   __C51__ only; the simulation models no WDT peripheral so the calls compile
 *   to empty stubs (same inert-under-sim / mandatory-on-silicon status as
 *   Timer1).  The dog is kicked once per 10 ms tick in the MAIN LOOP, never in
 *   an ISR, with a ~1 s timeout far beyond the worst-case ~24 ms UART stall.
 *   Classic AT89C52 has no on-chip WDT and needs an external WDT IC.
 *
 * Boot safety (implicit POST):
 *   control_task() checks sensor validity (open / short) BEFORE processing
 *   any state-machine transition in the same 100 ms pass.  If the NTC probe
 *   is bad at power-on, the very first control tick enters FAULT before any
 *   ON/OFF press can arm the heater — the button debounce alone needs 20 ms
 *   (2 ticks), and evt_onoff is consumed in the same control_task() pass
 *   that detects the fault.  A separate synchronous POST is therefore
 *   architecturally unnecessary; the invariant is: sensor-check always
 *   precedes state-transition within a single atomic control period.
 *
 * Every actuator/sensor crosses a LIVE UniSim channel:
 *   CH1 GPIO: relay/buzzer/LED outputs (js_pal_gpio_write) and button inputs
 *             (js_pal_gpio_read_state, INT0/1 polled by mcs51_extint).
 *   CH2 UART: telemetry (js_pal_uart_write -> UARTBus TX timeline).
 *   CH3 ADC : NTC via ADC0832 Level-2 pin-trap FSM pulling the analog rail
 *             (js_pal_adc_read_norm, headless INPUT_ANALOG on rail pin 32).
 * The cleanup pass emits a .cpp copy; this original is never edited in place.
 */
#include <REGX52.H>
#include <absacc.h>

/* ---- Pins ---------------------------------------------------------------- */
sbit ADC_CS   = P2^0;   /* ADC0832 chip select  (linear pin 16) */
sbit ADC_CLK  = P2^1;   /* ADC0832 clock       (linear pin 17) */
sbit ADC_DIO  = P2^2;   /* ADC0832 DI/DO shared (linear pin 18) */
sbit HEATER   = P1^0;   /* heater relay, active high  (pin 8)  */
sbit BUZZER   = P1^1;   /* active buzzer, active high (pin 9)  */
sbit LED_HEAT = P1^2;   /* heating LED, active low    (pin 10) */
sbit LED_WARM = P1^3;   /* keep-warm LED, active low  (pin 11) */
sbit LED_ERR  = P1^4;   /* fault LED, active low      (pin 12) */
sbit BTN_ONOFF = P3^2;  /* ON/OFF button, active low  (pin 26) */
sbit BTN_FUNC  = P3^3;  /* FUNC button, active low    (pin 27) */

/* Watchdog: STC12/STC15 clones integrate a WDT; classic AT89C52 has none
 * (needs an external WDT IC such as MAX813L/IMP706).  The sim models no WDT
 * peripheral, so the SFR + kick sequence compile to nothing under simulation
 * (__C51__ undefined -> the shim toolchain) and are mandatory on real silicon.
 * WDT_CONTR: bit5 EN_WDT(0x20), bit4 CLR_WDT/kick(0x10), PS2..0 prescale.
 * STC12 maps WDT_CONTR at 0xE1; STC15 at 0xC1 (adjust the address per MCU). */
#ifdef __C51__
sfr WDT_CONTR = 0xE1;
#endif

/* ---- Timer0 reload: 10 ms @ 12 MHz (1 count = 1 us) ---------------------- */
#define TICK_RELOAD_H   0xD8u
#define TICK_RELOAD_L   0xF0u

/* ---- Appliance states ---------------------------------------------------- */
#define ST_OFF    0u
#define ST_HEAT   1u
#define ST_WARM   2u
#define ST_FAULT  3u

/* ---- NTC LUT: ADC code HIGH when cold. code -> deg C breakpoints --------- */
static unsigned char code ntc_lut_code[6] = {240, 200, 150, 100,  60,  30};
static unsigned char code ntc_lut_temp[6] = { 25,  40,  60,  80,  95, 105};

#define NTC_OPEN_CODE   250u   /* code >= this: sensor open  */
#define NTC_SHORT_CODE  8u     /* code <= this: sensor short */
#define OVERTEMP_CODE   20u    /* code <= this: beyond-scale hot (plate has
                                * run away past the 105 C LUT clamp; normal
                                * boiling reads ~50, so margin is large) */
#define OVERTEMP_SECONDS 10u   /* code <= OVERTEMP_CODE this long in
                                * HEAT/WARM => over-temp fault (10 s sim
                                * acceleration; real product ~10-30 s) */
#define BOIL_TEMP_C     98u    /* boiling reached           */
#define BOIL_HOLD_TICKS 30u    /* 30 x 100 ms = 3 s boil hold */
#define WARM_HYST_C     3u     /* keep-warm hysteresis +/-3 C */
#define RELAY_DWELL_SECONDS 3u /* min relay OFF time before a re-energize is
                                * allowed (WARM bang-bang ON edge only).
                                * Accelerated sim value; a real product uses
                                * 30-60 s to extend mechanical-relay contact
                                * life (rated 1e5-2e5 ops; hot-switching arc
                                * erosion is the wear driver).  OFF edges are
                                * NEVER dwell-gated: fault / ON-OFF button /
                                * boil cut-off drop the heater immediately. */
#define FAULT_RECOVER_TICKS 3u /* 3 x 100 ms valid samples required before
                                * a sensor fault auto-clears (debounce) */
#define DRYFIRE_SECONDS 25u    /* heater on this long below 45 C => dry-fire
                                * NOTE: this is an accelerated sim value;
                                * real-product typical range is 120-180 s for
                                * a 1.0-1.8 L kettle with 1000 W heater.
                                * A real pot uses a water-level probe; this
                                * temperature/time watchdog is the stand-in. */
#define DRYFIRE_TEMP_C  45u

/* ---- Buzzer duration constants (in 10 ms ticks) -------------------------- */
#define BEEP_KEY_TICKS      5u    /* 50 ms key press acknowledgement */
#define BEEP_RECOVER_TICKS  10u   /* 100 ms sensor-fault recovery chirp */
#define BEEP_BOILDONE_TICKS 20u   /* 200 ms boil-done notification */
#define BEEP_ALARM_TICKS    100u  /* 1 s entry alarm on FAULT */
#define FAULT_BEEP_TIMEOUT  60u   /* silence periodic buzzer after 60 s in
                                   * FAULT; LED continues blinking.  Avoids
                                   * indefinite nuisance alarm (IEC 60335-1
                                   * Annex R recommends bounded audible
                                   * alarms for non-critical faults). */

/* ---- XDATA telemetry slots (host e2e can assert on these too) ----------- */
#define TLM_TEMP    0x0010u
#define TLM_STATE   0x0011u
#define TLM_HEATER  0x0012u
#define TLM_FAULT   0x0013u

static unsigned char state;        /* ST_* */
static unsigned char temp_c;       /* last temperature, deg C (LUT-mapped) */
static unsigned char adc_code;     /* last raw 8-bit ADC code */
static unsigned char fault_code;   /* 0=ok 1=open 2=short 3=dry-fire 4=over-temp */
static unsigned char warm_set;     /* keep-warm setpoint: 60/80/90 */
static unsigned char heater_on;    /* heater drive latch */
static unsigned char relay_off_sec;/* whole seconds the heater drive has been
                                    * OFF (saturates at 255); gates the WARM
                                    * bang-bang re-energize (relay dwell). */
static unsigned int  tick10ms;     /* free-running 10 ms counter */
static unsigned int  heat_seconds; /* seconds with heater on in HEAT state */
static unsigned char boil_hold;    /* 100 ms ticks of boil confirmation */
static unsigned char boil_confirm; /* 1 = confirming boil: hold runs to
                                    * completion without temperature resets */
static unsigned char overtemp_seconds; /* consecutive s with code <= OVERTEMP */
static unsigned char recover_ticks;    /* valid-sample streak for sensor-fault
                                        * auto-recovery (100 ms ticks) */
static unsigned int  beep_ticks;   /* remaining 10 ms ticks of buzzer on */
static unsigned char fault_beep_seconds;   /* seconds in FAULT with audible
                                            * alarm; once >= FAULT_BEEP_TIMEOUT,
                                            * periodic buzzer chirp silences
                                            * but fault LED keeps blinking */
static unsigned char blink_toggle;         /* toggles each second for 1 Hz
                                            * fault-LED blink; replaces
                                            * tick10ms/100 division which has
                                            * a phase glitch at 16-bit
                                            * unsigned wraparound (~11 min) */
static volatile unsigned char tick_flag;   /* set by Timer0 ISR each 10 ms */

/* button debounce: 2 consecutive low samples (20 ms) => press event */
static unsigned char db_onoff;
static unsigned char db_func;
static volatile unsigned char evt_onoff;
static volatile unsigned char evt_func;

/* ---- ADC0832 bit-bang read (3-wire, MSB first, same shape as iron_ntc) --- */
static unsigned char adc0832_read(unsigned char channel) {
    unsigned char i;
    unsigned char dat = 0;

    ADC_CS  = 1;
    ADC_CLK = 0;
    ADC_CS  = 0;               /* CS fall: begin conversion */

    ADC_DIO = 1; ADC_CLK = 1; ADC_CLK = 0;   /* rise 1: Start bit = 1 */
    ADC_DIO = 1; ADC_CLK = 1; ADC_CLK = 0;   /* rise 2: SGL/DIF = 1 */
    ADC_DIO = channel & 1;                   /* rise 3: ODD/SIGN channel select */
    ADC_CLK = 1;                             /* rise 3 latches channel; CLK is
                                              * held HIGH (no trailing fall) so
                                              * the read loop's first CLK=0 is
                                              * the falling edge that presents
                                              * the MSB — identical shape to the
                                              * proven iron_ntc / adc0832_read
                                              * samples.  An extra CLK pulse here
                                              * (a trailing CLK=0 or a "park"
                                              * CLK=1) adds a clock edge that
                                              * shifts the ADC0832 pin-trap FSM
                                              * and mis-reads every code. */
    ADC_DIO = 1;                             /* release DIO (input enable) */

    for (i = 0; i < 8; i++) {
        ADC_CLK = 0;                        /* falling: DO presents next bit */
        dat <<= 1;
        if (ADC_DIO) {
            dat |= 1;
        }
        ADC_CLK = 1;
    }

    ADC_CS = 1;               /* CS rise: abort to idle */
    return dat;
}

/* Median-of-3 ADC filter: rejects single-sample noise spikes from supply
 * ripple and relay switching transients (real-machine concern; transparent
 * under simulation with ideal analog rail).  Three 13-clock ADC0832 reads
 * cost < 100 us at 12 MHz — well within the 100 ms control period. */
static unsigned char adc_read_filtered(unsigned char channel) {
    unsigned char a, b, c, t;
    a = adc0832_read(channel);
    b = adc0832_read(channel);
    c = adc0832_read(channel);
    /* 3-element sort network: exactly 3 compare-swap ops => a <= b <= c */
    if (a > b) { t = a; a = b; b = t; }
    if (b > c) { t = b; b = c; c = t; }
    if (a > b) { t = a; a = b; b = t; }
    return b;                               /* median */
}

static unsigned char ntc_code_to_temp(unsigned char code_val) {
    unsigned char i;
    /* clamp at the cold end */
    if (code_val >= ntc_lut_code[0]) {
        return ntc_lut_temp[0];
    }
    /* linear interpolation between breakpoints for adequate keep-warm
     * resolution (a step-function LUT collapses +/-3 C hysteresis). */
    for (i = 1; i < 6u; i++) {
        if (code_val >= ntc_lut_code[i]) {
            return ntc_lut_temp[i - 1] +
                (unsigned char)(((unsigned int)(ntc_lut_temp[i] - ntc_lut_temp[i - 1]) *
                (ntc_lut_code[i - 1] - code_val)) /
                (ntc_lut_code[i - 1] - ntc_lut_code[i]));
        }
    }
    return 105u;   /* below the lowest bracket: hotter than the table */
}

/* ---- UART polled TX ------------------------------------------------------ */
static void uart_send(char c) {
    SBUF = c;       /* emits on channel 2; TI set synchronously by the model */
    while (!TI) {
        _nop_();
    }
    TI = 0;
}

static void uart_send_dec(unsigned char v) {
    /* 0..255, always 3 digits for stable ASSERT_BUS_PAYLOAD matching. */
    uart_send((char)('0' + (v / 100u)));
    uart_send((char)('0' + ((v / 10u) % 10u)));
    uart_send((char)('0' + (v % 10u)));
}

static void uart_send_str(const char *s) {
    while (*s) {
        uart_send(*s);
        s++;
    }
}

static void telemetry_emit(void) {
    uart_send_str("T=");
    uart_send_dec(temp_c);
    uart_send_str("C,S=");
    uart_send((char)('0' + state));
    uart_send_str(",H=");
    uart_send((char)('0' + heater_on));
    uart_send_str(",F=");
    uart_send((char)('0' + fault_code));
    uart_send_str("\n");
}

static void beep(unsigned int ticks_10ms) {
    beep_ticks = ticks_10ms;
}

/* ---- Button scan: 20 ms debounce, press event on falling edge ----------- */
static void button_scan(void) {
    if (BTN_ONOFF == 0) {
        if (db_onoff < 2u) {
            db_onoff++;
            if (db_onoff == 2u) {
                evt_onoff = 1;
            }
        }
    } else {
        db_onoff = 0;
    }

    if (BTN_FUNC == 0) {
        if (db_func < 2u) {
            db_func++;
            if (db_func == 2u) {
                evt_func = 1;
            }
        }
    } else {
        db_func = 0;
    }
}

static void enter_fault(unsigned char code_val) {
    if (state == ST_FAULT) {
        /* Already faulted: hold the safe state and keep the FIRST latched
         * code. Thermal faults (3/4) only fire from HEAT/WARM, so the first
         * latch is always the highest reachable severity; a later off-scale
         * sensor reading must not reclassify a manual-reset thermal fault
         * as an auto-clearing sensor fault, nor re-arm the entry alarm. */
        heater_on = 0;
        return;
    }
    state = ST_FAULT;
    fault_code = code_val;
    heater_on = 0;
    recover_ticks = 0;
    fault_beep_seconds = 0;        /* reset alarm-silence countdown */
    beep(BEEP_ALARM_TICKS);        /* 1 s alarm on entry only */
}

/* ---- 100 ms control task: sample + state machine ------------------------ */
static void control_task(void) {
    adc_code = adc_read_filtered(0);
    XBYTE[TLM_TEMP] = adc_code;
    temp_c = ntc_code_to_temp(adc_code);

    /* Sensor faults dominate normal states and are re-derived each pass.
     * enter_fault() keeps latched thermal faults (3/4) at top priority:
     * an off-scale reading after dry-fire/over-temp must NOT reclassify a
     * manual-reset fault as an auto-clearing sensor fault.
     *
     * NOTE (implicit POST): this sensor check runs BEFORE the switch()
     * state-machine below, so if the probe is bad at the very first 100 ms
     * tick after boot, FAULT fires before any ON/OFF press can arm the
     * heater — the button debounce needs 20 ms (2 ticks) and evt_onoff
     * is consumed in the same atomic pass that detects the fault. */
    if (adc_code >= NTC_OPEN_CODE) {
        enter_fault(1u);
    } else if (adc_code <= NTC_SHORT_CODE) {
        enter_fault(2u);
    }
    if (state == ST_FAULT && (fault_code == 1u || fault_code == 2u) &&
        adc_code > NTC_SHORT_CODE && adc_code < NTC_OPEN_CODE) {
        /* Sensor reading valid again: require FAULT_RECOVER_TICKS consecutive
         * good samples before auto-returning to OFF (debounce a flaky probe). */
        if (recover_ticks < 255u) {
            recover_ticks++;
        }
    } else {
        recover_ticks = 0;
    }

    switch (state) {
    case ST_OFF:
        heater_on = 0;
        boil_hold = 0;
        boil_confirm = 0;
        heat_seconds = 0;
        if (evt_onoff) {
            state = ST_HEAT;
            fault_code = 0;
            warm_set = 60u;     /* reset to default on each power-on cycle */
            boil_hold = 0;
            boil_confirm = 0;
            heat_seconds = 0;
            overtemp_seconds = 0;
            beep(BEEP_KEY_TICKS);
        }
        break;

    case ST_HEAT:
        if (evt_onoff) {
            state = ST_OFF;
            heater_on = 0;
            boil_confirm = 0;
            beep(BEEP_KEY_TICKS);
            break;
        }
        if (boil_confirm || temp_c >= BOIL_TEMP_C) {
            /* Boil confirmation is time-only: after the first >= 98 C
             * sample the heater stays off for 3 s even if the (lagging,
             * plate-mounted) NTC dips below threshold — resetting on the
             * dip livelocked the relay on/off around 98 C. */
            heater_on = 0;
            boil_confirm = 1;
            boil_hold++;
            if (boil_hold >= BOIL_HOLD_TICKS) {
                state = ST_WARM;
                boil_confirm = 0;
                overtemp_seconds = 0;
                beep(BEEP_BOILDONE_TICKS);
            }
        } else {
            heater_on = 1;
        }
        break;

    case ST_WARM:
        if (evt_onoff) {
            state = ST_OFF;
            heater_on = 0;
            beep(BEEP_KEY_TICKS);
            break;
        }
        if (evt_func) {
            /* cycle keep-warm setpoint 60 -> 80 -> 90 -> 60 */
            if (warm_set == 60u) {
                warm_set = 80u;
            } else if (warm_set == 80u) {
                warm_set = 90u;
            } else {
                warm_set = 60u;
            }
            beep(BEEP_KEY_TICKS);
        }
        /* bang-bang with +/-3 C hysteresis around the setpoint.
         * Guard against unsigned underflow if warm_set < WARM_HYST_C
         * (defensive — current setpoints 60/80/90 are always safe, but
         * future-proofing per C51 best practice). */
        if (warm_set > WARM_HYST_C && temp_c < (warm_set - WARM_HYST_C)) {
            /* Re-energize only after the relay has been OFF for at least
             * RELAY_DWELL_SECONDS (min-off-time, contact-life protection);
             * otherwise hold off and re-evaluate next tick.  The OFF branch
             * below is unconditional — a too-hot cut-off is never delayed. */
            if (relay_off_sec >= RELAY_DWELL_SECONDS) {
                heater_on = 1;
            }
        } else if (temp_c > (unsigned char)(warm_set + WARM_HYST_C)) {
            heater_on = 0;
        }
        break;

    case ST_FAULT:
        heater_on = 0;
        if (fault_code == 3u || fault_code == 4u) {
            /* Thermal faults are latched until the user presses ON/OFF;
             * a recovered probe does NOT clear them. */
            if (evt_onoff) {
                state = ST_OFF;
                fault_code = 0;
                heat_seconds = 0;
                overtemp_seconds = 0;
                recover_ticks = 0;
                beep(BEEP_KEY_TICKS);
            }
        } else {
            /* Sensor fault: auto-return to OFF only after the debounced
             * valid-sample streak (recover_ticks, counted above). */
            if (recover_ticks >= FAULT_RECOVER_TICKS) {
                state = ST_OFF;
                fault_code = 0;
                recover_ticks = 0;
                beep(BEEP_RECOVER_TICKS);
            }
        }
        break;

    default:
        state = ST_OFF;
        heater_on = 0;
        break;
    }

    evt_onoff = 0;
    evt_func  = 0;

    XBYTE[TLM_STATE]  = state;
    XBYTE[TLM_HEATER] = heater_on;
    XBYTE[TLM_FAULT]  = fault_code;
}

/* ---- 1 s task: dry-fire watchdog + UART telemetry ----------------------- */
static void one_second_task(void) {
    /* Relay dwell timer: count consecutive seconds the heater drive is OFF.
     * Runs at the 1 s boundary, AFTER control_task() in this same tick, so the
     * count increments one second after the drive actually drops.  Saturates
     * at 255 (boot seeds 255 so the very first heat-up is never gated). */
    if (heater_on) {
        relay_off_sec = 0;
    } else if (relay_off_sec < 255u) {
        relay_off_sec++;
    }

    if (state == ST_HEAT && heater_on) {
        heat_seconds++;
        if (heat_seconds > DRYFIRE_SECONDS && temp_c < DRYFIRE_TEMP_C) {
            enter_fault(3u);    /* dry-fire: no water / no temperature rise */
        }
    }
    /* Over-temp watchdog: raw code beyond-scale hot while heating. Runs in
     * WARM too — a long keep-warm session can boil the pot dry, and the
     * dry-fire watchdog above only covers HEAT. Raw code is used on purpose:
     * temp_c is clamped to 105 C by the LUT and cannot see run-away. */
    if ((state == ST_HEAT || state == ST_WARM) &&
        adc_code > NTC_SHORT_CODE && adc_code <= OVERTEMP_CODE) {
        overtemp_seconds++;
        if (overtemp_seconds >= OVERTEMP_SECONDS) {
            enter_fault(4u);
        }
    } else {
        overtemp_seconds = 0;
    }
    /* Fault alarm silence countdown: after FAULT_BEEP_TIMEOUT seconds the
     * periodic buzzer chirp is suppressed while the fault LED keeps
     * blinking — avoids indefinite nuisance alarm. */
    if (state == ST_FAULT && fault_beep_seconds < FAULT_BEEP_TIMEOUT) {
        fault_beep_seconds++;
    }
    telemetry_emit();
}

/* ---- Output refresh: actuators + indicators ----------------------------- */
static void outputs_refresh(void) {
    HEATER = heater_on ? 1 : 0;

    /* LEDs active low: 0 = lit */
    LED_HEAT = (state == ST_HEAT) ? 0 : 1;
    LED_WARM = (state == ST_WARM) ? 0 : 1;
    if (state == ST_FAULT) {
        /* 1 Hz blink driven by blink_toggle (toggled in main loop each
         * second); replaces tick10ms/100 division which has a visible
         * phase glitch at 16-bit unsigned wraparound (~655.36 s). */
        LED_ERR = blink_toggle ? 0 : 1;
    } else {
        LED_ERR = 1;
    }

    /* buzzer: explicit beep timer, plus periodic FAULT alarm with timeout */
    if (beep_ticks > 0u) {
        beep_ticks--;
        BUZZER = 1;
    } else if (state == ST_FAULT && fault_beep_seconds < FAULT_BEEP_TIMEOUT) {
        /* 100 ms chirp every second; silenced after FAULT_BEEP_TIMEOUT to
         * avoid indefinite nuisance alarm (LED continues blinking). */
        BUZZER = ((tick10ms % 100u) < 10u) ? 1 : 0;
    } else {
        BUZZER = 0;
    }
}

/* ---- Timer0 10 ms tick ISR ---------------------------------------------- */
void Timer0_ISR(void) interrupt 1 {
    TH0 = TICK_RELOAD_H;
    TL0 = TICK_RELOAD_L;
    tick_flag = 1;
}

static void timer0_init(void) {
    TMOD &= 0xF0;   /* clear Timer0 control nibble */
    TMOD |= 0x01;   /* Timer0 mode 1 (16-bit), internal clock */
    TH0 = TICK_RELOAD_H;
    TL0 = TICK_RELOAD_L;
    ET0 = 1;
    EA  = 1;
    TR0 = 1;
}

/* Watchdog kick.  Fed from the MAIN LOOP only — never from an ISR: an ISR-fed
 * dog stays silent if the foreground code runs away while interrupts keep
 * firing, defeating the protection.  The WDT timeout (prescale PS=100 -> ~1 s
 * at 12 MHz) far exceeds the worst-case loop stall (one full UART telemetry
 * frame blocks ~24 ms @9600, plus <100 us for the median-of-3 ADC read). */
static void wdt_init(void) {
#ifdef __C51__
    WDT_CONTR = 0x24;   /* EN_WDT=1, IDLE_WDT=0, PS=100 (start, ~1 s timeout) */
#endif
}

static void wdt_feed(void) {
#ifdef __C51__
    WDT_CONTR = 0x34;   /* EN_WDT=1, CLR_WDT=1 (kick), PS=100 preserved */
#endif
}

void main(void) {
    /* Standard 8051 power-on idiom: latch every port high so pins are
     * quasi-bidirectional inputs (weak internal pull-up) / idle-high outputs.
     * P1 actuators/LEDs: high = relay/buzzer off, LEDs off (active low);
     * P2: ADC CS idle high; P3: buttons (P3.2/P3.3) released, UART TX idle.
     * Under simulation (ADR-0077) the framework already seeds P0..P3 latch=0xFF
     * with a WEAK-HIGH driver at reset, so these writes compute diff==0 and
     * emit no edge — exactly mirroring silicon, which never edges either. */
    P1 = 0xFF;
    P2 = 0xFF;
    P3 = 0xFF;

    SCON = 0x40;    /* UART mode 1 (8-bit), REN=0, TX only */

    state = ST_OFF;
    fault_code = 0;
    warm_set = 60u;
    heater_on = 0;
    relay_off_sec = 255u;   /* dwell already satisfied at boot: first heat-up
                             * is never gated; counts down/up from real edges */
    tick10ms = 0;
    heat_seconds = 0;
    boil_hold = 0;
    boil_confirm = 0;
    overtemp_seconds = 0;
    recover_ticks = 0;
    beep_ticks = 0;
    fault_beep_seconds = 0;
    blink_toggle = 0;
    db_onoff = 0;
    db_func = 0;
    evt_onoff = 0;
    evt_func = 0;
    temp_c = 25;
    adc_code = 240;

    timer0_init();

    /* Timer1 mode-2 (8-bit auto-reload) baud-rate generator for UART mode-1.
     * Initialized AFTER timer0_init() so the TMOD mask preserves Timer0's
     * nibble (standard Keil C51 dual-timer initialization pattern).
     *
     * 9600 bps @ 11.0592 MHz, SMOD=0:
     *   TH1 = 256 - Fosc / (384 * baud) = 256 - 11059200 / (384 * 9600)
     *        = 256 - 3 = 253 = 0xFD.
     *
     * NOTE: the simulation model sets TI synchronously and does not validate
     * baud rate, so Timer1 is functionally inert under sim but mandatory for
     * real 8051 silicon.  The 12 MHz teaching crystal does NOT yield a
     * standard baud rate (9600 error ~+7%, beyond the ±5% UART tolerance);
     * real hardware must use an 11.0592 MHz crystal or drop to 4800 bps. */
    TMOD &= 0x0F;     /* clear Timer1 control nibble, preserve Timer0 */
    TMOD |= 0x20;     /* Timer1 mode 2: 8-bit auto-reload */
    TH1 = 0xFD;       /* 9600 bps @ 11.0592 MHz */
    TL1 = 0xFD;
    TR1 = 1;           /* start Timer1 */

    wdt_init();        /* hardware watchdog: inert under sim, armed on silicon */

    while (1) {
        _nop_();                /* cooperative microstep / event rendezvous */
        if (!tick_flag) {
            continue;
        }
        tick_flag = 0;
        tick10ms++;

        button_scan();

        if ((tick10ms % 10u) == 0u) {    /* every 100 ms */
            control_task();
        }
        if ((tick10ms % 100u) == 0u) {   /* every 1 s */
            blink_toggle ^= 1u;          /* 1 Hz toggle for fault LED blink */
            one_second_task();
        }

        outputs_refresh();
        wdt_feed();          /* kick in the main loop only, once per tick */
    }
}
