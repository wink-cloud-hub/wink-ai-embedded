# SPDX-License-Identifier: Apache-2.0
"""
mutation_catalog.py - Domain-Specific Mutation Operator Catalog (L2-T1 / L2-T4)
================================================================================
Provides calibrated firmware mutation operators, semantic witnesses, and
classification semantics (KILLED, MUTATION_SURVIVED, EQUIVALENT, UNKNOWN).
"""
from __future__ import annotations

import difflib
import re
from typing import Any, Dict, List, Optional, Tuple


CATALOG_OPERATORS: Dict[str, Dict[str, Any]] = {
    "MUT-GPIO-INV": {
        "domain": "GPIO",
        "name": "GPIO Output Level Inversion",
        "description": "Inverts gpio_set_level calls from level to !level",
        "prerequisites": ["SOC_GPIO_SUPPORTED"],
        "target_pattern": r"gpio_set_level\s*\(\s*([^,]+),\s*([^)]+)\)",
        "replacement": r"gpio_set_level(\1, !(\2))",
        "semantic_witness": "Inverts output electrical level at target pin"
    },
    "MUT-GPIO-PIN": {
        "domain": "GPIO",
        "name": "GPIO Pin Mapping Offset",
        "description": "Corrupts target GPIO pin constant with +1 offset",
        "prerequisites": ["SOC_GPIO_SUPPORTED"],
        "target_pattern": r"(#define\s+BLINK_GPIO\s+)(\d+)",
        "replacement": r"\g<1>(\2 + 1)",
        "semantic_witness": "Directs signal output to wrong physical pin"
    },
    "MUT-TMR-PERIOD": {
        "domain": "Timer",
        "name": "Timer Alarm Period Doubling",
        "description": "Doubles timer alarm period or interval count",
        "prerequisites": ["SOC_GPTIMER_SUPPORTED"],
        "target_pattern": r"(\.alarm_count\s*=\s*)(\d+)",
        "replacement": r"\g<1>(\2 * 2)",
        "semantic_witness": "Destroys exact period frequency observation"
    },
    "MUT-TMR-RELOAD": {
        "domain": "Timer",
        "name": "Timer Auto-Reload Disabled",
        "description": "Sets auto_reload_on_alarm flag to false",
        "prerequisites": ["SOC_GPTIMER_SUPPORTED"],
        "target_pattern": r"(\.auto_reload_on_alarm\s*=\s*)true",
        "replacement": r"\g<1>false",
        "semantic_witness": "Prevents cyclic timer alarm rearming"
    },
    "MUT-UART-PAYLOAD": {
        "domain": "UART",
        "name": "UART Transmit Data Corruption",
        "description": "Corrupts output buffer or tx payload in write call",
        "prerequisites": ["SOC_UART_SUPPORTED"],
        "target_pattern": r"uart_write_bytes\s*\(\s*([^,]+),\s*\([^)]+\)\s*data,\s*len\s*\)",
        "replacement": r"uart_write_bytes(\1, \"__MUTATED_UART_DATA__\", 20)",
        "semantic_witness": "Corrupts loopback payload returned to host"
    },
    "MUT-ADC-CHAN": {
        "domain": "ADC",
        "name": "ADC Channel Index Offset",
        "description": "Alters sampling ADC channel index",
        "prerequisites": ["SOC_ADC_SUPPORTED"],
        "target_pattern": r"(ADC_CHANNEL_)(\d+)",
        "replacement": r"\g<1>(\2 + 1)",
        "semantic_witness": "Reads voltage from unassigned ADC channel"
    },
    "MUT-DAC-SCALE": {
        "domain": "DAC",
        "name": "DAC Step Amplitude Halved",
        "description": "Halves DAC cosine or step output amplitude",
        "prerequisites": ["SOC_DAC_SUPPORTED"],
        "target_pattern": r"(dac_oneshot_output_voltage\s*\([^,]+,\s*)([^)]+)\)",
        "replacement": r"\1(\2 / 2))",
        "semantic_witness": "Reduces analog output voltage out of tolerance"
    },
    "MUT-I2C-ADDR": {
        "domain": "I2C",
        "name": "I2C Device Address Offset",
        "description": "Offsets target I2C 7-bit device address",
        "prerequisites": ["SOC_I2C_SUPPORTED"],
        "target_pattern": r"(\.device_address\s*=\s*)(0x[0-9a-fA-F]+|\d+)",
        "replacement": r"\g<1>(\2 ^ 0x01)",
        "semantic_witness": "Communicates with nonexistent slave device"
    },
    "MUT-SPI-MODE": {
        "domain": "SPI",
        "name": "SPI Clock Mode Inversion",
        "description": "Toggles SPI clock mode CPOL/CPHA",
        "prerequisites": ["SOC_SPI_SUPPORTED"],
        "target_pattern": r"(\.mode\s*=\s*)(\d+)",
        "replacement": r"\g<1>((\2 + 1) % 4)",
        "semantic_witness": "Desynchronizes bus clock sampling phase"
    },
    "MUT-PWM-DUTY": {
        "domain": "PWM",
        "name": "PWM Duty Cycle Mutation",
        "description": "Alters PWM duty cycle calculation",
        "prerequisites": ["SOC_LEDC_SUPPORTED"],
        "target_pattern": r"(ledc_set_duty\s*\([^,]+,\s*[^,]+,\s*)([^)]+)\)",
        "replacement": r"\1(\2 / 2))",
        "semantic_witness": "Halves duty cycle output voltage on PWM pin"
    },
    "MUT-SLEEP-TIME": {
        "domain": "Power",
        "name": "Sleep Timer Scale Offset",
        "description": "Multiplies sleep wakeup time by 10",
        "prerequisites": ["SOC_PM_SUPPORTED"],
        "target_pattern": r"(esp_sleep_enable_timer_wakeup\s*\()([^)]+)\)",
        "replacement": r"\1(\2 * 10))",
        "semantic_witness": "Delays scheduled wakeup event beyond tolerance"
    },
    "MUT-WIFI-SSID": {
        "domain": "WiFi",
        "name": "WiFi SSID Corruption",
        "description": "Corrupts WiFi target SSID literal",
        "prerequisites": ["SOC_WIFI_SUPPORTED"],
        "target_pattern": r"(\.ssid\s*=\s*\")([^\"]+)(\")",
        "replacement": r"\g<1>__MUTATED_\2\3",
        "semantic_witness": "Prevents association with target AP"
    },
    "MUT-FS-PATH": {
        "domain": "Storage",
        "name": "VFS File Path Mismatch",
        "description": "Corrupts fopen path in VFS storage tests",
        "prerequisites": ["SOC_SPIFFS_SUPPORTED"],
        "target_pattern": r"(fopen\s*\(\s*\")([^\"]+)(\")",
        "replacement": r"\g<1>\2_corrupt\3",
        "semantic_witness": "Forces ENOENT error on target file access"
    },
    "MUT-LEDC-FREQ": {
        "domain": "LEDC",
        "name": "LEDC Timer Frequency Offset",
        "description": "Halves LEDC timer frequency configuration",
        "prerequisites": ["SOC_LEDC_SUPPORTED"],
        "target_pattern": r"(\.freq_hz\s*=\s*)(\d+)",
        "replacement": r"\g<1>(\2 / 2)",
        "semantic_witness": "Alters PWM carrier frequency period"
    },
    "MUT-UART-BAUD": {
        "domain": "UART",
        "name": "UART Baud Rate Inversion",
        "description": "Corrupts UART baud rate divisor",
        "prerequisites": ["SOC_UART_SUPPORTED"],
        "target_pattern": r"(\.baud_rate\s*=\s*)(\d+)",
        "replacement": r"\g<1>(\2 * 2)",
        "semantic_witness": "Generates baud mismatch framing errors"
    }
}


def apply_catalog_mutation(
    source_code: str,
    operator_id: str
) -> Tuple[Optional[str], Optional[str], Optional[str]]:
    """Apply catalog mutation to C source.

    Returns:
        (mutated_code, diff_patch, semantic_witness) or (None, None, None) if pattern unmatched.
    """
    op = CATALOG_OPERATORS.get(operator_id)
    if not op:
        return None, None, None

    pattern = op["target_pattern"]
    replacement = op["replacement"]

    if not re.search(pattern, source_code):
        return None, None, None

    mutated_code = re.sub(pattern, replacement, source_code, count=1)
    if mutated_code == source_code:
        return None, None, None

    diff_lines = list(difflib.unified_diff(
        source_code.splitlines(keepends=True),
        mutated_code.splitlines(keepends=True),
        fromfile="original.c",
        tofile=f"mutant_{operator_id}.c",
        n=3
    ))
    diff_patch = "".join(diff_lines)
    return mutated_code, diff_patch, op["semantic_witness"]


def classify_mutation_verdict(
    killed: bool,
    exit_code: int,
    build_failed: bool = False,
    diagnostic: str = ""
) -> Dict[str, Any]:
    """Classify mutation result per L2-T4 contract semantics."""
    if build_failed:
        return {
            "verdict": "BUILD_FAILED",
            "accepted": False,
            "rationale": "Compilation/linking failed; not counted as valid semantic mutation kill"
        }
    if exit_code in (137, 139, 124) or "signal" in diagnostic.lower() or "infrastructure" in diagnostic.lower():
        return {
            "verdict": "INFRA_FAILURE",
            "accepted": False,
            "rationale": f"Infrastructure crash or timeout (exit {exit_code}); not a semantic kill"
        }
    if killed:
        return {
            "verdict": "KILLED",
            "accepted": True,
            "rationale": "Target assertion successfully rejected the injected defect within window"
        }
    return {
        "verdict": "MUTATION_SURVIVED",
        "accepted": False,
        "rationale": "Injected non-equivalent defect survived assertions; indicates weak assertion coverage"
    }
