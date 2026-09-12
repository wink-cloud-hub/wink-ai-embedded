/* SPDX-License-Identifier: Apache-2.0
 * CMS8S78xx Commercial Health Pot (养生壶) Thermostat Application
 *
 * Fully leverages CMS8S78xx hardware peripherals:
 *   - On-chip 12-bit SAR ADC with internal 3.0V LDO reference on AN0 (P0.0),
 *     median-of-3 filter and 12-bit piecewise linear interpolation LUT.
 *   - Dedicated on-chip hardware buzzer frequency generator (BUZCON/BUZDIV)
 *     on P0.3 (pin 3) driving passive buzzer polyphonic sound profiles:
 *     key click (4 kHz), mode transition (2->3 kHz), boil-done melody
 *     (Do-Mi-Sol-Do 4-note chord), and dual-frequency warble alarm.
 *   - 4-digit 8-segment LED digital tube (4COM-8SEG) dynamic multiplexing
 *     using CMS8S78xx 150mA high-sink COM (P3.0..P3.3) and 32.7mA SEG (P1.0..P1.7):
 *     OFF: " -- ", HEAT: "XXbO" with 1Hz blinking decimal point,
 *     WARM: "XXYY" (current temp + target setpoint), FAULT: "E-01".."E-04".
 *   - Push buttons on P0.4 (ON/OFF) and P0.5 (FUNC) with 20 ms debounce.
 *   - Heater relay on P2.0 (active high).
 *   - Timer0 mode 1 (16-bit) 10 ms tick ISR: 4COM dynamic display scanning.
 *   - UART mode 1 polled TX telemetry once per second for headless assertions.
 *   - Comprehensive safety: NTC open/short, two-stage dry-fire watchdog
 *     (25s < 45C, or 60s without reaching boil in HEAT), over-temp watchdog
 *     (12-bit raw <= 16 / >105 C for 10s; beyond-scale codes also block a
 *     false boil-complete and force the contact open), relay minimum-off
 *     dwell on EVERY re-energize path, and a manual ON/OFF fault acknowledge
 *     (muted standby: the power key is always safe to press; heating cannot
 *     restart until the probe reads healthy).
 */
#include <wink_mcu.h>
#include <absacc.h>

/* ---- Pins ---------------------------------------------------------------- */
sbit HEATER    = P2^0;   /* Heater relay, active high  (linear pin 16) */
sbit LED_HEAT  = P0^1;   /* Heat indicator LED, active low (pin 1)     */
sbit LED_WARM  = P0^2;   /* Keep-warm LED, active low      (pin 2)     */
sbit LED_ERR   = P0^6;   /* Fault indicator LED, active low(pin 6)     */
sbit BTN_ONOFF = P0^4;   /* ON/OFF button, active low  (linear pin 4)  */
sbit BTN_FUNC  = P0^5;   /* FUNC button, active low    (linear pin 5)  */
/* Note:
 * - AN0 on P0.0 (pin 0) is on-chip 12-bit SAR ADC
 * - BUZZER on P0.3 (pin 3) is hardware BUZDIV/BUZCON output
 * - COM0..3 on P3.0..P3.3 (pins 24..27)
 * - SEG0..7 on P1.0..P1.7 (pins 8..15)
 */

/* ---- Appliance states ---------------------------------------------------- */
#define ST_OFF    0u
#define ST_HEAT   1u
#define ST_WARM   2u
#define ST_FAULT  3u

/* ---- Heating target modes ------------------------------------------------ */
#define MODE_BOIL_100    0u  /* Boil to 98 C with 3 s hold, then keep-warm */
#define MODE_DIRECT_55   1u  /* Direct heat to 55 C, then 55 C keep-warm   */
#define MODE_DIRECT_80   2u  /* Direct heat to 80 C, then 80 C keep-warm   */
#define DIRECT_55_TEMP_C 55u /* 55 C direct heating target                 */
#define DIRECT_80_TEMP_C 80u /* 80 C direct heating target                 */
#define BOIL_TEMP_C      98u /* Boiling reached target (98 C)              */

/* ---- NTC LUT: 12-bit ADC raw code, HIGH when cold, LOW when hot --------- *
 * Matches the device-tree NTC plugin physics: R25 = 10 kOhm, B = 3950,
 * divider Vout/Vcc = Rntc / (Rntc + 160 kOhm pull-up), raw = ratio * 4096.
 * Recompute anchors with: R(T)=R25*exp(B*(1/(T+273.15)-1/298.15)). */
#define NTC_OPEN_RAW         3900u  /* raw >= this: probe open / unplugged (E-01) */
#define NTC_SHORT_RAW        8u     /* raw <= this: probe short / failure   (E-02) */
#define OVERTEMP_RAW         16u    /* raw <= this: runaway > 105 C         (E-04) */
#define OVERTEMP_SECONDS     10u    /* 10 s continuous overtemp trigger */
#define BOIL_TEMP_C          98u    /* boiling reached */
#define BOIL_HOLD_TICKS      30u    /* 30 x 100 ms = 3 s boil hold */
#define WARM_HYST_C          1u     /* keep-warm hysteresis +/-1 C (high precision) */
#define RELAY_DWELL_SECONDS  3u     /* min relay OFF time before re-energizing */
#define FAULT_RECOVER_TICKS  3u     /* 3 x 100 ms valid samples to auto-clear sensor fault */
#define DRYFIRE_SECONDS      25u    /* heater on this long below 45 C => dry-fire (E-03) */
#define DRYFIRE_TEMP_C       45u
#define BOIL_TIMEOUT_SECONDS 60u    /* heater on this long without reaching 98 C => dry-fire (E-03);
                                     * accelerated sim value, real product calibrate 600~900 s */
#define FAULT_BEEP_TIMEOUT   60u    /* silence periodic buzzer alarm after 60 s */

static unsigned int code ntc_lut_raw[11]  = {571, 458, 298, 241, 131,  89,  63,  32,  24,  19,  16};
static unsigned char code ntc_lut_temp[11] = {  5,  10,  20,  25,  40,  50,  60,  80,  90,  98, 105};

/* ---- 4COM-8SEG Display Font Table ---------------------------------------- */
/* Bit: dp(7) g(6) f(5) e(4) d(3) c(2) b(1) a(0) — common cathode */
static unsigned char code font_table[15] = {
    0x3Fu, /* 0 */
    0x06u, /* 1 */
    0x5Bu, /* 2 */
    0x4Fu, /* 3 */
    0x66u, /* 4 */
    0x6Du, /* 5 */
    0x7Du, /* 6 */
    0x07u, /* 7 */
    0x7Fu, /* 8 */
    0x6Fu, /* 9 */
    0x7Cu, /* 10: 'b' */
    0x3Fu, /* 11: 'O' */
    0x79u, /* 12: 'E' */
    0x40u, /* 13: '-' */
    0x00u  /* 14: blank */
};

static unsigned char disp_digits[4]; /* Active segment patterns for 4 digits */
static unsigned char scan_idx;       /* Current multiplexed digit (0..3) */

/* ---- Hardware Buzzer Tone Sequencer -------------------------------------- */
typedef struct {
    unsigned char div;       /* BUZDIV value (0 = silence) */
    unsigned char dur_10ms;  /* duration in 10 ms ticks (0 = end) */
} tone_step_t;

/* Prescaler = 64 @ 24 MHz: Fbuz = 187500 / BUZDIV */
static tone_step_t code TONE_KEY[] = {
    {47u, 3u},  /* ~4000 Hz, 30 ms key click */
    {0u, 0u}
};

static tone_step_t code TONE_STEP[] = {
    {94u, 4u},  /* ~2000 Hz, 40 ms */
    {63u, 4u},  /* ~3000 Hz, 40 ms step up */
    {0u, 0u}
};

static tone_step_t code TONE_BOIL_DONE[] = {
    {180u, 10u}, /* C6: 1042 Hz, 100 ms */
    {142u, 10u}, /* E6: 1320 Hz, 100 ms */
    {120u, 10u}, /* G6: 1562 Hz, 100 ms */
    {89u,  20u}, /* C7: 2106 Hz, 200 ms */
    {0u, 0u}
};

static tone_step_t code TONE_RECOVER[] = {
    {120u, 10u}, /* G6: 1562 Hz, 100 ms */
    {0u, 0u}
};

/* Low short blip: key recognised but action unavailable in this state */
static tone_step_t code TONE_BUSY[] = {
    {235u, 3u}, /* ~800 Hz, 30 ms denial/acknowledge blip */
    {0u, 0u}
};

static tone_step_t code *cur_melody;
static unsigned char melody_idx;
static unsigned char melody_ticks;

static void play_melody(tone_step_t code *mel) {
    cur_melody = mel;
    melody_idx = 0;
    melody_ticks = 0;
}

/* ---- Telemetry Slots (XDATA) --------------------------------------------- */
#define TLM_TEMP    0x0010u
#define TLM_STATE   0x0011u
#define TLM_HEATER  0x0012u
#define TLM_FAULT   0x0013u
#define TLM_ADC_H   0x0014u
#define TLM_ADC_L   0x0015u

/* ---- Global State Variables ---------------------------------------------- */
static unsigned char state;              /* ST_OFF / ST_HEAT / ST_WARM / ST_FAULT */
static unsigned char temp_c;             /* Measured water temp in deg C */
static unsigned int  adc_code;           /* 12-bit ADC raw code (0..4095) */
static unsigned char fault_code;         /* 0=ok 1=open 2=short 3=dryfire 4=overtemp */
static unsigned char warm_set;           /* Keep-warm target: 55/60/80/90 */
static unsigned char heat_mode;          /* Heating target mode: MODE_BOIL_100 / MODE_DIRECT_55 */
static unsigned char heater_on;          /* Heater drive latch */
static unsigned char relay_off_sec;      /* Seconds heater has been OFF (saturates 255) */
static unsigned int  heat_seconds;       /* Continuous seconds heating in HEAT */
static unsigned char boil_hold;          /* 100 ms ticks in boil confirmation */
static unsigned char boil_confirm;       /* 1 = confirming boil */
static unsigned char overtemp_seconds;   /* Consecutive seconds in overtemp zone */
static unsigned char recover_ticks;      /* Valid sensor streak counter */
static unsigned char fault_beep_seconds; /* Seconds in FAULT with audible alarm */
static unsigned char blink_toggle;       /* 1 Hz toggle for UI blinking */
static unsigned char sensor_muted;       /* 1 = sensor fault manually acknowledged:
                                          * stay in silent safe standby while probe bad */
static unsigned char div_100ms;          /* Super-loop prescaler: 10 ms -> 100 ms */
static unsigned char div_500ms;          /* Super-loop prescaler: 10 ms -> 500 ms */
static unsigned char div_1000ms;         /* Super-loop prescaler: 10 ms -> 1 s */
static unsigned char warble_phase;       /* 0..99 x 10 ms phase for the fault warble */
static volatile unsigned char tick_flag;

/* Button debounce states */
static unsigned char db_onoff;
static unsigned char db_func;
static volatile unsigned char evt_onoff;
static volatile unsigned char evt_func;

/* ---- ADC0 on-chip 12-bit SAR ADC ---------------------------------------- */
static void adc_init(void) {
    ADC_ConfigRunMode(ADC_CLK_DIV_256, ADC_RESULT_RIGHT);
    ADC_EnableChannel(ADC_CH_0);
    GPIO_SET_MUX_MODE(P00CFG, GPIO_P00_MUX_AN0);
    ADC_EnableLDO();
    ADC_ConfigADCVref(ADC_VREF_3V);
    ADC_Start();
}

static unsigned int adc_read_raw(void) {
    ADC_GO();
    /* Vendor mandatory poll (ADC_IS_BUSY = ADGO bit): conversion takes
     * ~150 us at ADC_CLK_DIV_256. In the wasm model the SFR trap completes
     * synchronously, so the loop body never executes. */
    while (ADC_IS_BUSY) {
        _nop_();
    }
    return ADC_GetADCResult();
}

static unsigned int adc_read_filtered(void) {
    unsigned int a = adc_read_raw();
    unsigned int b = adc_read_raw();
    unsigned int c = adc_read_raw();
    unsigned int t;
    if (a > b) { t = a; a = b; b = t; }
    if (b > c) { t = b; b = c; c = t; }
    if (a > b) { t = a; a = b; b = t; }
    return b;
}

static unsigned char ntc_code_to_temp(unsigned int code_val) {
    unsigned char i;
    if (code_val >= ntc_lut_raw[0]) {
        return ntc_lut_temp[0];
    }
    for (i = 1; i < 11u; i++) {
        if (code_val >= ntc_lut_raw[i]) {
            return ntc_lut_temp[i - 1] +
                (unsigned char)(((unsigned int)(ntc_lut_temp[i] - ntc_lut_temp[i - 1]) *
                (ntc_lut_raw[i - 1] - code_val)) /
                (ntc_lut_raw[i - 1] - ntc_lut_raw[i]));
        }
    }
    return 105u;
}

/* ---- 4COM-8SEG Display Update -------------------------------------------- */
static void display_update(void) {
    /* Font table has no glyph above digit 9; clamp water temperature display
     * at 99 C until the over-temperature fault takes over (>105 C, 10 s). */
    unsigned char disp_temp = (temp_c > 99u) ? 99u : temp_c;
    if (state == ST_OFF) {
        if (heat_mode == MODE_DIRECT_55) {
            /* "-55-" */
            disp_digits[0] = font_table[13]; /* '-' */
            disp_digits[1] = font_table[5];  /* '5' */
            disp_digits[2] = font_table[5];  /* '5' */
            disp_digits[3] = font_table[13]; /* '-' */
        } else if (heat_mode == MODE_DIRECT_80) {
            /* "-80-" */
            disp_digits[0] = font_table[13]; /* '-' */
            disp_digits[1] = font_table[8];  /* '8' */
            disp_digits[2] = font_table[0];  /* '0' */
            disp_digits[3] = font_table[13]; /* '-' */
        } else {
            /* " -- " */
            disp_digits[0] = font_table[14]; /* blank */
            disp_digits[1] = font_table[13]; /* '-' */
            disp_digits[2] = font_table[13]; /* '-' */
            disp_digits[3] = font_table[14]; /* blank */
        }
    } else if (state == ST_HEAT) {
        if (heat_mode == MODE_DIRECT_55) {
            /* "XX55": current temp + target 55; dp breathes while heating */
            disp_digits[0] = font_table[disp_temp / 10u];
            disp_digits[1] = font_table[disp_temp % 10u] |
                             ((heater_on && blink_toggle) ? 0x80u : 0u);
            disp_digits[2] = font_table[5];  /* '5' */
            disp_digits[3] = font_table[5];  /* '5' */
        } else if (heat_mode == MODE_DIRECT_80) {
            /* "XX80": current temp + target 80; dp breathes while heating */
            disp_digits[0] = font_table[disp_temp / 10u];
            disp_digits[1] = font_table[disp_temp % 10u] |
                             ((heater_on && blink_toggle) ? 0x80u : 0u);
            disp_digits[2] = font_table[8];  /* '8' */
            disp_digits[3] = font_table[0];  /* '0' */
        } else {
            /* "XXbO"; dp breathes only while the heater is actually energised
             * and stays OFF during the 3 s boil-confirmation window */
            disp_digits[0] = font_table[disp_temp / 10u];
            disp_digits[1] = font_table[disp_temp % 10u] |
                             ((heater_on && blink_toggle) ? 0x80u : 0u);
            disp_digits[2] = font_table[10]; /* 'b' */
            disp_digits[3] = font_table[11]; /* 'O' */
        }
    } else if (state == ST_WARM) {
        /* "XXYY": current temp + target setpoint */
        disp_digits[0] = font_table[disp_temp / 10u];
        disp_digits[1] = font_table[disp_temp % 10u] | (heater_on ? (blink_toggle ? 0x80u : 0u) : 0u);
        disp_digits[2] = font_table[warm_set / 10u];
        disp_digits[3] = font_table[warm_set % 10u];
    } else if (state == ST_FAULT) {
        /* "E-01".."E-04" */
        disp_digits[0] = font_table[12]; /* 'E' */
        disp_digits[1] = font_table[13]; /* '-' */
        disp_digits[2] = font_table[0];  /* '0' */
        disp_digits[3] = font_table[fault_code % 10u];
    }
}

/* ---- Buzzer Sequencer Task (10 ms) --------------------------------------- */
static void buzzer_task(void) {
    if (cur_melody != 0) {
        if (melody_ticks > 0) {
            melody_ticks--;
        } else {
            unsigned char div = cur_melody[melody_idx].div;
            unsigned char dur = cur_melody[melody_idx].dur_10ms;
            if (dur == 0) {
                cur_melody = 0;
                BUZ_DisableBuzzer();
            } else {
                melody_idx++;
                melody_ticks = dur;
                if (div > 0) {
                    BUZDIV = div;
                    BUZ_EnableBuzzer();
                } else {
                    BUZ_DisableBuzzer();
                }
            }
        }
    } else if (state == ST_FAULT && fault_beep_seconds < FAULT_BEEP_TIMEOUT) {
        /* Dual-frequency urgent warble: 100 ms @ 3 kHz, 100 ms @ 2 kHz.
         * warble_phase is a 0..99 x 10 ms sawtooth with a clean 1 s period. */
        unsigned int sub = (unsigned int)warble_phase * 10u;
        if (sub < 100u) {
            BUZDIV = 63u;
            BUZ_EnableBuzzer();
        } else if (sub < 200u) {
            BUZDIV = 94u;
            BUZ_EnableBuzzer();
        } else {
            BUZ_DisableBuzzer();
        }
    } else {
        BUZ_DisableBuzzer();
    }
}

/* ---- UART Polled Telemetry ----------------------------------------------- */
static void uart_send(char c) {
    SBUF = c;
    while (!TI) {
        _nop_();
    }
    TI = 0;
}

static void uart_send_dec(unsigned char v) {
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

/* ---- Button Scanning (20 ms debounce) ------------------------------------ */
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

/* ---- Fault Handler ------------------------------------------------------- */
static void enter_fault(unsigned char code_val) {
    if (state == ST_FAULT) {
        heater_on = 0;
        /* A latched thermal fault (E-03/E-04) has top priority and is never
         * downgraded to a self-clearing sensor fault. Within the sensor class
         * the code still refreshes (open <-> short) so the display and the
         * telemetry frame match the actual probe reading. */
        if (fault_code != 3u && fault_code != 4u) {
            fault_code = code_val;
        }
        return;
    }
    state = ST_FAULT;
    fault_code = code_val;
    heater_on = 0;
    recover_ticks = 0;
    fault_beep_seconds = 0;
    /* Cut any key/melody tone so the urgent warble starts at the next 10 ms
     * buzzer tick instead of being masked for the melody's remaining time. */
    cur_melody = 0;
}

/* ---- Instant Button Processing Task (10 ms tick) ------------------------ */
static void handle_buttons(void) {
    if (evt_func) {
        evt_func = 0;
        if (state == ST_OFF) {
            /* 3-mode cycle in standby: 100 -> 55 -> 80 -> 100 */
            if (heat_mode == MODE_BOIL_100) {
                heat_mode = MODE_DIRECT_55;
            } else if (heat_mode == MODE_DIRECT_55) {
                heat_mode = MODE_DIRECT_80;
            } else {
                heat_mode = MODE_BOIL_100;
            }
            play_melody(TONE_STEP);
            display_update();
        } else if (state == ST_HEAT) {
            /* 3-mode cycle during heating: 100 -> 55 -> 80 -> 100 */
            if (heat_mode == MODE_BOIL_100) {
                heat_mode = MODE_DIRECT_55;
                warm_set = 55u;
                play_melody(TONE_STEP);
                if (temp_c >= DIRECT_55_TEMP_C) {
                    heater_on = 0;
                    state = ST_WARM;
                    boil_confirm = 0;
                    boil_hold = 0;
                    overtemp_seconds = 0;
                    play_melody(TONE_BOIL_DONE);
                }
            } else if (heat_mode == MODE_DIRECT_55) {
                heat_mode = MODE_DIRECT_80;
                warm_set = 80u;
                play_melody(TONE_STEP);
                if (temp_c >= DIRECT_80_TEMP_C) {
                    heater_on = 0;
                    state = ST_WARM;
                    boil_confirm = 0;
                    boil_hold = 0;
                    overtemp_seconds = 0;
                    play_melody(TONE_BOIL_DONE);
                }
            } else {
                heat_mode = MODE_BOIL_100;
                play_melody(TONE_STEP);
            }
            /* Reset heat_seconds when switching modes so test clicking won't trip dry-fire */
            heat_seconds = 0;
            display_update();
        } else if (state == ST_WARM) {
            /* Cycle keep-warm setpoint: 55 -> 60 -> 80 -> 90 -> 55 */
            if (warm_set == 55u) {
                warm_set = 60u;
            } else if (warm_set == 60u) {
                warm_set = 80u;
            } else if (warm_set == 80u) {
                warm_set = 90u;
            } else {
                warm_set = 55u;
            }
            play_melody(TONE_STEP);
            display_update();
        }
    }

    if (evt_onoff) {
        evt_onoff = 0;
        if (state == ST_OFF) {
            if (adc_code >= NTC_OPEN_RAW || adc_code <= NTC_SHORT_RAW) {
                sensor_muted = 0u;
                enter_fault((adc_code >= NTC_OPEN_RAW) ? 1u : 2u);
            } else {
                state = ST_HEAT;
                fault_code = 0u;
                sensor_muted = 0u;
                if (heat_mode == MODE_DIRECT_55) {
                    warm_set = 55u;
                } else if (heat_mode == MODE_DIRECT_80) {
                    warm_set = 80u;
                } else {
                    warm_set = 60u;
                }
                boil_hold = 0u;
                boil_confirm = 0u;
                heat_seconds = 0u;
                overtemp_seconds = 0u;
                play_melody(TONE_KEY);
            }
            display_update();
        } else if (state == ST_HEAT || state == ST_WARM) {
            state = ST_OFF;
            heater_on = 0;
            boil_confirm = 0;
            boil_hold = 0;
            heat_seconds = 0;
            overtemp_seconds = 0;
            play_melody(TONE_KEY);
            display_update();
        } else if (state == ST_FAULT) {
            if (fault_code == 3u || fault_code == 4u) {
                state = ST_OFF;
                fault_code = 0;
                sensor_muted = 0u;
                heat_seconds = 0;
                overtemp_seconds = 0;
                recover_ticks = 0;
                play_melody(TONE_KEY);
            } else {
                state = ST_OFF;
                sensor_muted = 1u;
                heater_on = 0;
                play_melody(TONE_KEY);
            }
            display_update();
        }
    }
}

/* ---- 100 ms Control Task: ADC + State Machine ---------------------------- */
static void control_task(void) {
    unsigned int raw = adc_read_filtered();
    adc_code = raw;
    temp_c = ntc_code_to_temp(adc_code);

    XBYTE[TLM_TEMP]  = temp_c;
    XBYTE[TLM_ADC_H] = (unsigned char)((raw >> 8) & 0x0Fu);
    XBYTE[TLM_ADC_L] = (unsigned char)(raw & 0xFFu);

    /* Implicit POST & continuous sensor health monitoring, evaluated BEFORE
     * the state machine so a bad probe can never be acted upon. A sensor
     * fault that the user has manually acknowledged stays in a silent "muted
     * standby" instead of re-alarming every tick: the contact stays open and
     * the fault is remembered (telemetry F) until the probe reads healthy. */
    if (adc_code >= NTC_OPEN_RAW) {
        if (state == ST_OFF && sensor_muted) {
            fault_code = 1u;  /* NTC open, acknowledged standby */
        } else {
            enter_fault(1u);  /* NTC Open -> E-01 */
        }
    } else if (adc_code <= NTC_SHORT_RAW) {
        if (state == ST_OFF && sensor_muted) {
            fault_code = 2u;  /* NTC short, acknowledged standby */
        } else {
            enter_fault(2u);  /* NTC Short -> E-02 */
        }
    }

    /* Valid-reading streak for sensor faults (E-01, E-02): covers both the
     * audible FAULT state and the acknowledged muted-standby state */
    if (adc_code > NTC_SHORT_RAW && adc_code < NTC_OPEN_RAW &&
        ((state == ST_FAULT && (fault_code == 1u || fault_code == 2u)) ||
         (state == ST_OFF && sensor_muted))) {
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
        overtemp_seconds = 0;
        if (sensor_muted && recover_ticks >= FAULT_RECOVER_TICKS) {
            /* Probe healthy again after a manual fault acknowledge */
            sensor_muted = 0u;
            fault_code = 0u;
            recover_ticks = 0u;
            play_melody(TONE_RECOVER);
        }
        break;

    case ST_HEAT:
        if (heat_mode == MODE_DIRECT_55) {
            if (temp_c >= DIRECT_55_TEMP_C) {
                /* Direct 55 C target reached: turn off heater and enter WARM */
                heater_on = 0;
                state = ST_WARM;
                warm_set = 55u;
                boil_confirm = 0;
                boil_hold = 0;
                overtemp_seconds = 0;
                play_melody(TONE_BOIL_DONE);
            } else {
                /* Dwell-gated like every re-energize path */
                if (relay_off_sec >= RELAY_DWELL_SECONDS) {
                    heater_on = 1;
                }
            }
        } else if (heat_mode == MODE_DIRECT_80) {
            if (temp_c >= DIRECT_80_TEMP_C) {
                /* Direct 80 C target reached: turn off heater and enter WARM */
                heater_on = 0;
                state = ST_WARM;
                warm_set = 80u;
                boil_confirm = 0;
                boil_hold = 0;
                overtemp_seconds = 0;
                play_melody(TONE_BOIL_DONE);
            } else {
                /* Dwell-gated like every re-energize path */
                if (relay_off_sec >= RELAY_DWELL_SECONDS) {
                    heater_on = 1;
                }
            }
        } else {
            /* Beyond-scale raw (<=16, >105 C) must not be mistaken for boil:
             * block the 3 s confirmation while the over-temp watchdog qualifies.
             * If the probe returns into a plausible boiling band the hold simply
             * resumes, so genuine 98 C operation is unaffected. */
            if ((boil_confirm || temp_c >= BOIL_TEMP_C) && adc_code > OVERTEMP_RAW) {
                /* Boil confirmation: heater off, maintain 3 seconds hold */
                heater_on = 0;
                boil_confirm = 1;
                boil_hold++;
                if (boil_hold >= BOIL_HOLD_TICKS) {
                    state = ST_WARM;
                    boil_confirm = 0;
                    overtemp_seconds = 0;
                    play_melody(TONE_BOIL_DONE);
                }
            } else {
                /* Dwell-gated like every re-energize path: a rapid OFF->ON power
                 * toggle cannot reclose the contact with zero off-time */
                if (relay_off_sec >= RELAY_DWELL_SECONDS) {
                    heater_on = 1;
                }
            }
        }
        break;

    case ST_WARM:
        /* Precision keep-warm: +/-1 C hysteresis */
        if (warm_set > WARM_HYST_C && temp_c < (warm_set - WARM_HYST_C)) {
            if (relay_off_sec >= RELAY_DWELL_SECONDS) {
                heater_on = 1;
            }
        } else if (temp_c > (unsigned char)(warm_set + WARM_HYST_C)) {
            heater_on = 0;
        }
        break;

    case ST_FAULT:
        heater_on = 0;
        if (fault_code != 3u && fault_code != 4u) {
            if (recover_ticks >= FAULT_RECOVER_TICKS) {
                /* Auto-return after the debounced valid-reading streak */
                state = ST_OFF;
                fault_code = 0;
                sensor_muted = 0u;
                recover_ticks = 0;
                play_melody(TONE_RECOVER);
            }
        }
        break;

    default:
        state = ST_OFF;
        heater_on = 0;
        break;
    }

    /* Beyond-scale codes in an active thermal cycle force the contact open
     * while the over-temp watchdog qualifies (>105 C band), regardless of
     * what the state branches above requested. */
    if ((state == ST_HEAT || state == ST_WARM) && adc_code <= OVERTEMP_RAW) {
        heater_on = 0;
    }

    XBYTE[TLM_STATE]  = state;
    XBYTE[TLM_HEATER] = heater_on;
    XBYTE[TLM_FAULT]  = fault_code;
}

/* ---- 1-Second Task ------------------------------------------------------- */
static void one_second_task(void) {
    if (heater_on) {
        relay_off_sec = 0;
    } else if (relay_off_sec < 255u) {
        relay_off_sec++;
    }

    /* Two-stage dry-fire protection, counted only while the contact is
     * actually closed in HEAT:
     *  1) no heat-up past 45 C within DRYFIRE_SECONDS (empty pot / bad coupling)
     *  2) warm-up stalls and never reaches boil within BOIL_TIMEOUT_SECONDS
     *     (lid off / cold draught / low mains voltage / small load that has
     *     already crossed 45 C so stage 1 can no longer catch it) */
    if (state == ST_HEAT && heater_on) {
        heat_seconds++;
        if ((heat_seconds > DRYFIRE_SECONDS && temp_c < DRYFIRE_TEMP_C) ||
            (heat_mode == MODE_BOIL_100 && heat_seconds > BOIL_TIMEOUT_SECONDS && temp_c < BOIL_TEMP_C)) {
            enter_fault(3u);  /* Dry-fire -> E-03 */
        }
    }

    /* Over-temp protection: raw <= 16 (~>105 C) in HEAT or WARM */
    if ((state == ST_HEAT || state == ST_WARM) &&
        adc_code > NTC_SHORT_RAW && adc_code <= OVERTEMP_RAW) {
        overtemp_seconds++;
        if (overtemp_seconds >= OVERTEMP_SECONDS) {
            enter_fault(4u);  /* Over-temp -> E-04 */
        }
    } else {
        overtemp_seconds = 0;
    }

    if (state == ST_FAULT && fault_beep_seconds < FAULT_BEEP_TIMEOUT) {
        fault_beep_seconds++;
    }

    telemetry_emit();
}

/* ---- Timer0 10 ms Tick ISR: Display Dynamic Scan ------------------------ */
void Timer0_ISR(void) interrupt 1 {
    /* Reload for 10 ms @ Fsys/12, 24 MHz (0.5 us/count, 20000 counts) */
    TH0 = 0xB1u;
    TL0 = 0xE0u;

    /* 4COM common-cathode multiplexing:
     * 1) Blank COM lines to prevent visual ghosting */
    P3 = (P3 & 0xF0u) | 0x0Fu;

    /* 2) Output active segments to P1 */
    P1 = disp_digits[scan_idx];

    /* 3) Strobe active COM low (0 = sink current) */
    P3 = (P3 & 0xF0u) | (unsigned char)(~(1u << scan_idx) & 0x0Fu);

    /* 4) Advance to next COM */
    scan_idx = (scan_idx + 1u) & 0x03u;

    tick_flag = 1;
}

/* ---- Hardware Watchdog (CMS8S78xx, TA-protected) ------------------------- */
#define WDT_WDTRE 0x02u  /* WDCON.1: watchdog reset enable */
#define WDT_WDTCLR 0x01u /* WDCON.0: watchdog clear */
#define WDT_WTS_BITS 0x06u /* CKCON WTS<2:0> = 2^24 Tsys (~0.70 s @ 24 MHz) */

static void wdt_init(void) {
    /* Overflow interval = 2^24 / Fsys = 16.78M / 24 MHz ~= 0.70 s.
     * Must exceed the longest blocking section: UART telemetry TX
     * (22 bytes @ 9600 bps ~= 23 ms). */
    CKCON = (CKCON & 0x1Fu) | (WDT_WTS_BITS << 5u);
    /* WDTRE is a TA-protected bit (ref manual 4.2) */
    {
        unsigned char ea_save = EA;
        EA = 0;
        _nop_();
        TA = 0xAAu;
        TA = 0x55u;
        WDCON |= WDT_WDTRE;
        EA = ea_save;
    }
}

static void wdt_feed(void) {
    unsigned char ea_save = EA;
    EA = 0;
    _nop_();
    TA = 0xAAu;
    TA = 0x55u;
    WDCON |= WDT_WDTCLR;
    EA = ea_save;
}

/* ---- Main Entry Point --------------------------------------------------- */
void main(void) {
    /* 1. CMS8S78xx system clock: 24 MHz internal RC */
    SYS_SET_SYSTEM_CLK(SYS_CLK_DIV_1);

    /* 2. Configure 4COM display pins: P3.0..P3.3 GPIO, push-pull, 150 mA sink */
    GPIO_SET_MUX_MODE(P30CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P31CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P32CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P33CFG, GPIO_MUX_GPIO);
    P3TRIS |= 0x0Fu;
    P3DR   |= 0x0Fu;
    P3 = (P3 & 0xF0u) | 0x0Fu; /* Blank all COMs */

    /* 3. Configure 8SEG display pins: P1.0..P1.7 GPIO, push-pull, 32.7 mA */
    GPIO_SET_MUX_MODE(P10CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P11CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P12CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P13CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P14CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P15CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P16CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P17CFG, GPIO_MUX_GPIO);
    P1TRIS = 0xFFu;
    LEDSDRP1L = 0x02u;
    LEDSDRP1H = 0x02u;
    P1 = 0x00u;

    /* 3b. Configure indicator LEDs: P0.1 (HEAT), P0.2 (WARM), P0.6 (FAULT) */
    GPIO_SET_MUX_MODE(P01CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P02CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P06CFG, GPIO_MUX_GPIO);
    P0TRIS |= 0x46u;
    LED_HEAT = 1;
    LED_WARM = 1;
    LED_ERR = 1;

    /* 4. Configure Heater relay: P2.0 push-pull output */
    GPIO_SET_MUX_MODE(P20CFG, GPIO_MUX_GPIO);
    P2TRIS |= 0x01u;
    HEATER = 0;

    /* 5. Configure Buttons: P0.4 (ON/OFF), P0.5 (FUNC) plain input
     *    with the internal ~32 kOhm pull-ups enabled (P0UP); buttons
     *    short to GND, so a released pin must not float. */
    GPIO_SET_MUX_MODE(P04CFG, GPIO_MUX_GPIO);
    GPIO_SET_MUX_MODE(P05CFG, GPIO_MUX_GPIO);
    P0TRIS &= ~0x30u;
    P0UP |= 0x30u;
    BTN_ONOFF = 1;
    BTN_FUNC = 1;

    /* 6. Configure on-chip Hardware Buzzer: P0.3 */
    GPIO_SET_MUX_MODE(P03CFG, GPIO_P03_MUX_BUZZ);
    BUZ_ConfigBuzzer(BUZ_CKS_64, 0);
    BUZ_DisableBuzzer();

    /* 7. Configure on-chip 12-bit SAR ADC: AN0 on P0.0 */
    adc_init();

    /* 8. UART0 mode 1 (8-bit async, 9600 bps @ 24 MHz) polled TX telemetry.
     *    Baud generator: Timer1 mode 2 (8-bit auto-reload), T1M = 1 selects
     *    Fosc/4 timer clock (CKCON.4), SMOD0 = 1 double baud (PCON.7):
     *      reload = 256 - Fosc * 2 / 32 / 4 / 9600 = 217 (0xD9)
     *      actual baud = 24 MHz / 4 / (256-217) / 16 = 9615 bps (0.16 % err).
     *    UART pins are remapped off the 150 mA LED COM port: TXD -> P2.2,
     *    RXD -> P2.1 (CFG mux 0x03 + PS_RXD port select), leaving P3.0/P3.1
     *    dedicated to 4COM digit drive. */
    GPIO_SET_MUX_MODE(P22CFG, 0x03u); /* P2.2 = UART0 TXD */
    GPIO_SET_MUX_MODE(P21CFG, 0x03u); /* P2.1 = UART0 RXD */
    XBYTE[0xF69Fu] = 0x21u;           /* PS_RXD select: GPIO port P2.1 */
    FUNCCR &= 0xF8u;                  /* UART0 baud clock source = Timer1 */
    SCON = 0x40u;                     /* mode 1: 8-bit async, TX only */
    PCON |= 0x80u;                    /* SMOD0 = 1 (double baud rate) */
    CKCON |= 0x10u;                   /* T1M = 1: Timer1 clock = Fosc/4 */
    TMOD = (TMOD & 0x0Fu) | 0x20u;    /* Timer1 mode 2 (8-bit auto-reload) */
    TH1 = 217u;                       /* 0xD9 -> 9600 bps */
    TL1 = 217u;
    TR1 = 1;                          /* Start baud generator */

    /* 9. Initialize state machine & display buffers */
    state = ST_OFF;
    fault_code = 0;
    warm_set = 60u;
    heat_mode = MODE_BOIL_100;
    heater_on = 0;
    relay_off_sec = 255u;
    heat_seconds = 0;
    boil_hold = 0;
    boil_confirm = 0;
    overtemp_seconds = 0;
    recover_ticks = 0;
    fault_beep_seconds = 0;
    blink_toggle = 0;
    sensor_muted = 0;
    div_100ms = 0;
    div_500ms = 0;
    div_1000ms = 0;
    warble_phase = 0;
    scan_idx = 0;
    db_onoff = 0;
    db_func = 0;
    evt_onoff = 0;
    evt_func = 0;
    temp_c = 25u;
    adc_code = 241u;
    cur_melody = 0;
    melody_idx = 0;
    melody_ticks = 0;
    display_update();

    /* 10. Timer0: 10 ms periodic tick at Fsys/12 (24 MHz / 12 = 2 MHz,
     *     0.5 us/count -> 20000 counts = 10 ms, reload 65536-20000=0xB1E0).
     *     CKCON.T0M is cleared explicitly so timing does not depend on the
     *     silicon reset default (T0M=1, Fsys/4). */
    CKCON &= ~0x08u;
    TMOD &= 0xF0u;
    TMOD |= 0x01u;
    TH0 = 0xB1u;
    TL0 = 0xE0u;
    ET0 = 1;
    EA  = 1;
    TR0 = 1;

    wdt_init();

    while (1) {
        _nop_();
        if (!tick_flag) {
            continue;
        }
        tick_flag = 0;

        /* 10 ms periodic tasks: button debouncing, instant handling & buzzer sequencer */
        button_scan();
        handle_buttons();
        buzzer_task();
        if (warble_phase < 99u) {
            warble_phase++;
        } else {
            warble_phase = 0u;
        }

        /* Independent byte prescalers: fixed 100 ms / 500 ms / 1 s phases
         * with no 16-bit tick counter that can wrap and shorten a second */
        if (++div_100ms >= 10u) {
            div_100ms = 0u;
            /* 100 ms periodic tasks: temperature sampling & control state machine */
            control_task();
        }

        if (++div_500ms >= 50u) {
            div_500ms = 0u;
            /* 1 Hz blink toggle (dp & fault LED) */
            blink_toggle ^= 1u;
        }

        if (++div_1000ms >= 100u) {
            div_1000ms = 0u;
            /* 1 s periodic tasks: watchdogs & telemetry */
            one_second_task();
        }

        /* Refresh actuators, indicator LEDs & display buffer */
        HEATER = heater_on ? 1 : 0;
        LED_HEAT = (state == ST_HEAT) ? 0 : 1;
        LED_WARM = (state == ST_WARM) ? 0 : 1;
        if (state == ST_FAULT) {
            LED_ERR = blink_toggle ? 0 : 1;
        } else {
            LED_ERR = 1;
        }
        display_update();

        wdt_feed();
    }
}
