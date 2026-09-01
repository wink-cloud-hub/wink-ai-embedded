/* SPDX-License-Identifier: Apache-2.0
 * MCS-51 health-pot (养生壶) thermostat — UNMODIFIED-STYLE Keil C51 user
 * source for the Wink MCS-51 zero-intrusion simulation sandbox.
 *
 * Appliance profile:
 *   - NTC temperature probe through an external ADC0832 (3-wire bit-bang,
 *     CS=P2.0 CLK=P2.1 DIO=P2.2; NTC pulled up to VREF => ADC code is HIGH
 *     when cold, LOW when hot, same convention as the iron_ntc sample).
 *   - Heater relay on P1.0 (active high).
 *   - Active buzzer on P1.1 (active high).
 *   - Three indicator LEDs on P1.2/P1.3/P1.4 (active low, 0 = lit):
 *     heat / keep-warm / fault.
 *   - Two push buttons on P3.2 (ON/OFF, also INT0) and P3.3 (FUNC, also
 *     INT1), active low, 20 ms software debounce in the super-loop.
 *   - Timer0 mode-1 10 ms tick ISR (12 MHz teaching crystal, 1 count = 1 us,
 *     reload 65536-10000 = 0xD8F0).
 *   - UART mode-1 polled TX telemetry once per second:
 *       "T=<temp>C,S=<state>,H=<heater>,F=<fault>\n"
 *     state: 0=OFF 1=HEAT(boil) 2=WARM 3=FAULT; H: 0/1 heater drive.
 *
 * State machine:
 *   OFF  --press ON/OFF-->  HEAT  (boil to >=98 C, hold 3 s)  -->  WARM
 *   HEAT/WARM --press ON/OFF--> OFF
 *   WARM: FUNC cycles the keep-warm setpoint 60 -> 80 -> 90 -> 60 C with
 *         +/-3 C hysteresis on the heater.
 *   Any state -> FAULT (latched) on: NTC open (code >= 250), NTC short
 *   (code <= 8), or dry-fire (heater on for >25 s while temp stays < 45 C;
 *   a real pot uses a water-level probe, this is the functional stand-in).
 *   FAULT: heater forced off, fault LED blinks, buzzer beeps periodically;
 *   pressing ON/OFF after the sensor fault clears returns to OFF.
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
#define BOIL_TEMP_C     98u    /* boiling reached           */
#define BOIL_HOLD_TICKS 30u    /* 30 x 100 ms = 3 s boil hold */
#define WARM_HYST_C     3u     /* keep-warm hysteresis +/-3 C */
#define DRYFIRE_SECONDS 25u    /* heater on this long below 45 C => dry-fire */
#define DRYFIRE_TEMP_C  45u

/* ---- XDATA telemetry slots (host e2e can assert on these too) ----------- */
#define TLM_TEMP    0x0010u
#define TLM_STATE   0x0011u
#define TLM_HEATER  0x0012u
#define TLM_FAULT   0x0013u

static unsigned char state;        /* ST_* */
static unsigned char temp_c;       /* last filtered-ish temperature, deg C */
static unsigned char adc_code;     /* last raw 8-bit ADC code */
static unsigned char fault_code;   /* 0=ok 1=open 2=short 3=dry-fire */
static unsigned char warm_set;     /* keep-warm setpoint: 60/80/90 */
static unsigned char heater_on;    /* heater drive latch */
static unsigned int  tick10ms;     /* free-running 10 ms counter */
static unsigned int  heat_seconds; /* seconds with heater on in HEAT state */
static unsigned char boil_hold;    /* 100 ms ticks at/above boil temp */
static unsigned int  beep_ticks;   /* remaining 10 ms ticks of buzzer on */
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
    ADC_DIO = channel & 1;                   /* rise 3: channel select */
    ADC_CLK = 1; ADC_CLK = 0;
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

static unsigned char ntc_code_to_temp(unsigned char code_val) {
    unsigned char i;
    for (i = 0; i < 6u; i++) {
        if (code_val >= ntc_lut_code[i]) {
            return ntc_lut_temp[i];
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
    state = ST_FAULT;
    fault_code = code_val;
    heater_on = 0;
    beep(100u);                 /* 1 s alarm on entry */
}

/* ---- 100 ms control task: sample + state machine ------------------------ */
static void control_task(void) {
    adc_code = adc0832_read(0);
    XBYTE[TLM_TEMP] = adc_code;
    temp_c = ntc_code_to_temp(adc_code);

    /* Sensor faults dominate every state and are re-derived each pass:
     * a recovered probe clears the latched sensor fault immediately. */
    if (adc_code >= NTC_OPEN_CODE) {
        if (fault_code != 1u) {
            enter_fault(1u);
        } else {
            state = ST_FAULT;
            heater_on = 0;
        }
    } else if (adc_code <= NTC_SHORT_CODE) {
        if (fault_code != 2u) {
            enter_fault(2u);
        } else {
            state = ST_FAULT;
            heater_on = 0;
        }
    } else if (state == ST_FAULT && fault_code != 3u) {
        /* sensor fault cleared; wait for ON/OFF to leave FAULT (below) */
    }

    switch (state) {
    case ST_OFF:
        heater_on = 0;
        boil_hold = 0;
        heat_seconds = 0;
        if (evt_onoff) {
            state = ST_HEAT;
            fault_code = 0;
            boil_hold = 0;
            heat_seconds = 0;
            beep(5u);           /* 50 ms key beep */
        }
        break;

    case ST_HEAT:
        if (evt_onoff) {
            state = ST_OFF;
            heater_on = 0;
            beep(5u);
            break;
        }
        if (temp_c >= BOIL_TEMP_C) {
            heater_on = 0;      /* reached boil: stop while confirming */
            boil_hold++;
            if (boil_hold >= BOIL_HOLD_TICKS) {
                state = ST_WARM;
                beep(20u);      /* 200 ms boil-done beep */
            }
        } else {
            heater_on = 1;
            boil_hold = 0;
        }
        break;

    case ST_WARM:
        if (evt_onoff) {
            state = ST_OFF;
            heater_on = 0;
            beep(5u);
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
            beep(5u);
        }
        /* bang-bang with +/-3 C hysteresis around the setpoint */
        if (temp_c < (warm_set - WARM_HYST_C)) {
            heater_on = 1;
        } else if (temp_c > (warm_set + WARM_HYST_C)) {
            heater_on = 0;
        }
        break;

    case ST_FAULT:
        heater_on = 0;
        if (fault_code == 3u) {
            /* dry-fire is latched until the user presses ON/OFF */
            if (evt_onoff) {
                state = ST_OFF;
                fault_code = 0;
                heat_seconds = 0;
                beep(5u);
            }
        } else {
            /* sensor fault: auto-return to OFF once the probe recovers */
            if (adc_code > NTC_SHORT_CODE && adc_code < NTC_OPEN_CODE) {
                state = ST_OFF;
                fault_code = 0;
                beep(10u);
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
    if (state == ST_HEAT && heater_on) {
        heat_seconds++;
        if (heat_seconds > DRYFIRE_SECONDS && temp_c < DRYFIRE_TEMP_C) {
            enter_fault(3u);    /* dry-fire: no water / no temperature rise */
        }
    }
    telemetry_emit();
}

/* ---- Output refresh: actuators + indicators ----------------------------- */
static void outputs_refresh(void) {
    HEATER = heater_on ? 1 : 0;

    /* LEDs active low: 0 = lit */
    LED_HEAT = (state == ST_HEAT && heater_on) ? 0 : 1;
    LED_WARM = (state == ST_WARM) ? 0 : 1;
    if (state == ST_FAULT) {
        /* blink at ~1 Hz: (tick10ms/100) odd => lit */
        LED_ERR = ((tick10ms / 100u) & 1u) ? 0 : 1;
    } else {
        LED_ERR = 1;
    }

    /* buzzer: explicit beep timer, plus periodic FAULT alarm */
    if (beep_ticks > 0u) {
        beep_ticks--;
        BUZZER = 1;
    } else if (state == ST_FAULT) {
        /* 100 ms chirp every second */
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

void main(void) {
    /* Output ports are driven to their idle-high level. P1 actuators/LEDs:
     * high = relay/buzzer off, LEDs off (active low). P2: ADC CS idle high.
     * P3 (buttons on P3.2/P3.3, UART TX on P3.1) is deliberately NOT written:
     * 8051 quasi-bidirectional ports reset to input mode (latch=1), and driving
     * P3 high would register the MCU as a strong high driver that arbitrates
     * against the button plugin (the input plugins supply the external level). */
    P1 = 0xFF;
    P2 = 0xFF;

    SCON = 0x40;    /* UART mode 1 (8-bit), REN=0, TX only */

    state = ST_OFF;
    fault_code = 0;
    warm_set = 60u;
    heater_on = 0;
    tick10ms = 0;
    heat_seconds = 0;
    boil_hold = 0;
    beep_ticks = 0;
    db_onoff = 0;
    db_func = 0;
    evt_onoff = 0;
    evt_func = 0;
    temp_c = 25;
    adc_code = 240;

    timer0_init();

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
            one_second_task();
        }

        outputs_refresh();
    }
}
