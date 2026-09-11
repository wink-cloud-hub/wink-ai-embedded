#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Comprehensive test suite for Task R4 & R5 mcs51_cleanup.py enhancements:
  - Preprocessor conditional branch tracking (#if 0 masking)
  - Target gating: Native vs SDCC
  - Local definition guard for delay functions
  - Delay call-site rewriting
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools"))
import mcs51_cleanup


class TestMcs51Cleanup(unittest.TestCase):
    def test_if0_masks_dead_isr(self):
        source = """
#include <reg52.h>

#if 0
void dead_isr(void) interrupt 1 {
    P1 = 0;
}
#endif

void active_isr(void) interrupt 0 {
    P1 = 1;
}

void main(void) {
    while(1);
}
"""
        cleaned, counts = mcs51_cleanup.cleanup(source, target="native")
        self.assertIn("void dead_isr(void) interrupt 1", cleaned)
        self.assertNotIn("WINK_ISR(1)", cleaned)
        self.assertIn("WINK_ISR(0)", cleaned)
        self.assertEqual(counts["isr"], 1)

    def test_if0_else_active_branch(self):
        source = """
#if 0
void isr0(void) interrupt 0 {}
#else
void isr1(void) interrupt 1 {}
#endif
"""
        cleaned, counts = mcs51_cleanup.cleanup(source, target="native")
        self.assertIn("void isr0(void) interrupt 0", cleaned)
        self.assertIn("WINK_ISR(1)", cleaned)
        self.assertEqual(counts["isr"], 1)

    def test_nested_if_conditionals(self):
        source = """
#if 0
  #if 1
    void isr2(void) interrupt 2 {}
  #endif
#endif
void isr3(void) interrupt 3 {}
"""
        cleaned, counts = mcs51_cleanup.cleanup(source, target="native")
        self.assertIn("void isr2(void) interrupt 2", cleaned)
        self.assertNotIn("WINK_ISR(2)", cleaned)
        self.assertIn("WINK_ISR(3)", cleaned)
        self.assertEqual(counts["isr"], 1)

    def test_local_definition_guard_skips_delay_rewrite(self):
        source = """
#include <reg52.h>

void delay_ms(unsigned int ms) {
    while(ms--);
}

void main(void) {
    delay_ms(100);
}
"""
        cleaned, counts = mcs51_cleanup.cleanup(source, target="native")
        self.assertIn("delay_ms(100)", cleaned)
        self.assertNotIn("wink_mcs51_delay_ms", cleaned)
        self.assertEqual(counts["delay"], 0)

    def test_external_delay_call_site_rewriting(self):
        source = """
#include <reg52.h>

void main(void) {
    delay_ms(50);
    delay_us(10);
}
"""
        cleaned, counts = mcs51_cleanup.cleanup(source, target="native")
        self.assertIn("wink_mcs51_delay_ms(50)", cleaned)
        self.assertIn("wink_delay_us(10)", cleaned)
        self.assertEqual(counts["delay"], 2)

    def test_sdcc_target_mode(self):
        source = """
#include <reg52.h>

unsigned char code lookup_table[] = { 0x01, 0x02 };
volatile unsigned char flag _at_ 0x20;

void timer_isr(void) interrupt 1 using 2 {
    P1 = lookup_table[0];
}

void main(void) {
    delay_ms(10);
    while(1);
}
"""
        cleaned, counts = mcs51_cleanup.cleanup(source, target="sdcc")
        self.assertIn("__code", cleaned)
        self.assertIn("__at(0x20)", cleaned)
        self.assertIn("void timer_isr(void) __interrupt(1) __using(2)", cleaned)
        self.assertNotIn("WINK_ISR", cleaned)
        self.assertNotIn("<wink_mcu.h>", cleaned)
        self.assertNotIn("wink_mcs51_delay_ms", cleaned)
        self.assertNotIn("_nop_()", cleaned)
        self.assertEqual(counts["isr"], 1)
        self.assertEqual(counts["sdcc"], 2)

    def test_sdcc_sbit_rewrite(self):
        # GAP-03: user sbit declarations must become absolute __sbit at().
        source = """
sbit HEATER = P2^0;
sbit TF1    = TCON^7;
sbit LED    = 0x90;
void main(void) { HEATER = 0; }
"""
        cleaned, counts = mcs51_cleanup.cleanup(source, target="sdcc")
        self.assertIn("__sbit __at(0xA0) HEATER;", cleaned)   # P2 base 0xA0 + 0
        self.assertIn("__sbit __at(0x8F) TF1;", cleaned)      # TCON 0x88 + 7
        self.assertIn("__sbit __at(0x90) LED;", cleaned)      # absolute kept
        self.assertEqual(counts["sbit_unresolved"], 0)

    def test_sdcc_sbit_unresolved_reg(self):
        # Unknown SFR in a relative sbit is left untouched and counted.
        source = "sbit X = NO_SUCH_REG^3;\n"
        cleaned, counts = mcs51_cleanup.cleanup(source, target="sdcc")
        self.assertIn("sbit X = NO_SUCH_REG^3;", cleaned)
        self.assertEqual(counts["sbit_unresolved"], 1)


if __name__ == "__main__":
    unittest.main()
