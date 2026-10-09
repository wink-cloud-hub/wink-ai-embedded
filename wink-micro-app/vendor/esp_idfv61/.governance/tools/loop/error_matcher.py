# SPDX-License-Identifier: Apache-2.0
"""
error_matcher.py - Multi-Domain Error Matcher & Boundary Verifier (WS-1 Task 1.3)
===================================================================================
Enforces strong typing and domain separation across:
  - domain: esp_err (positive integers, e.g. ESP_ERR_TIMEOUT = 0x107, ESP_FAIL = -1)
  - domain: wink_status (negative integers, e.g. WINK_ERR_TIMEOUT = -2, ADR-0001)
  - domain: posix_errno (positive errno integers, e.g. ETIMEDOUT = 110)
  - domain: nimble_hs (NimBLE host status positive integers)

Rejects vague matchers (< 0, != 0, boolean coercion) and prevents cross-domain sign confusion.
"""
from __future__ import annotations

from typing import Any, Dict, Optional, Tuple, Union

# Standard taxonomy symbol tables
ESP_ERR_SYMBOLS: Dict[str, int] = {
    "ESP_OK": 0,
    "ESP_FAIL": -1,
    "ESP_ERR_NO_MEM": 0x101,            # 257
    "ESP_ERR_INVALID_ARG": 0x102,       # 258
    "ESP_ERR_INVALID_STATE": 0x103,     # 259
    "ESP_ERR_INVALID_SIZE": 0x104,      # 260
    "ESP_ERR_NOT_FOUND": 0x105,         # 261
    "ESP_ERR_NOT_SUPPORTED": 0x106,     # 262
    "ESP_ERR_TIMEOUT": 0x107,           # 263
    "ESP_ERR_INVALID_RESPONSE": 0x108,  # 264
    "ESP_ERR_INVALID_CRC": 0x109,       # 265
    "ESP_ERR_INVALID_VERSION": 0x10A,   # 266
    "ESP_ERR_INVALID_MAC": 0x10B,       # 267
    "ESP_ERR_NOT_FINISHED": 0x10C,      # 268
    "ESP_ERR_NOT_ALLOWED": 0x10D,       # 269
}

WINK_STATUS_SYMBOLS: Dict[str, int] = {
    "WINK_OK": 0,
    "WINK_ERR_INVALID_ARG": -1,
    "WINK_ERR_TIMEOUT": -2,
    "WINK_ERR_DISCONNECTED": -3,
    "WINK_ERR_OUT_OF_RANGE": -4,
    "WINK_ERR_IO": -5,
    "WINK_ERR_BUSY": -6,
    "WINK_ERR_UNSUPPORTED": -7,
    "WINK_ERR_CHECKSUM": -8,
    "WINK_ERR_PERMISSION": -9,
    "WINK_ERR_RESOURCE_EXHAUSTED": -10,
    "WINK_ERR_NOT_INITIALIZED": -11,
    "WINK_ERR_HARDWARE": -12,
    "WINK_ERR_NO_MEM": -13,
    "WINK_ERR_EMPTY": -14,
    "WINK_ERR_FULL": -15,
    "WINK_ERR_INVALID_STATE": -16,
    "WINK_ERR_LOCKED": -17,
    "WINK_ERR_NOT_FOUND": -18,
    "WINK_ERR_CANCELED": -19,
    "WINK_ERR_OVERCURRENT": -20,
    "WINK_ERR_OVERTEMPERATURE": -21,
    "WINK_ERR_ALREADY_INITIALIZED": -22,
    "WINK_ERR_WATCHDOG": -30,
    "WINK_ERR_OVERFLOW": -40,
    "WINK_ERR_PANIC": -99,
}

POSIX_ERRNO_SYMBOLS: Dict[str, int] = {
    "EPERM": 1,
    "ENOENT": 2,
    "ESRCH": 3,
    "EINTR": 4,
    "EIO": 5,
    "ENXIO": 6,
    "EBADF": 9,
    "EAGAIN": 11,
    "ENOMEM": 12,
    "EACCES": 13,
    "EBUSY": 16,
    "EEXIST": 17,
    "ENODEV": 19,
    "EINVAL": 22,
    "ETIMEDOUT": 110,
    "ECONNREFUSED": 111,
    "ECONNRESET": 104,
}

NIMBLE_HS_SYMBOLS: Dict[str, int] = {
    "BLE_HS_EAGAIN": 1,
    "BLE_HS_EALREADY": 2,
    "BLE_HS_EINVAL": 3,
    "BLE_HS_ENOTCONN": 4,
    "BLE_HS_ETIMEOUT": 5,
    "BLE_HS_EDONE": 6,
    "BLE_HS_EBUSY": 7,
    "BLE_HS_EREJECT": 8,
}

VALID_DOMAINS = {"esp_err", "wink_status", "posix_errno", "nimble_hs"}


def is_error_matcher(matcher: Any) -> bool:
    """Check if a matcher specification represents a domain-typed error assertion."""
    if not isinstance(matcher, dict):
        return False
    if "assert_error" in matcher:
        return True
    if "domain" in matcher and matcher.get("domain") in VALID_DOMAINS:
        return True
    return False


def validate_no_vague_matcher(matcher: Any) -> Tuple[bool, str]:
    """Detect and reject vague error matchers such as untyped != 0 or < 0 comparisons."""
    if isinstance(matcher, dict):
        # Look for vague comparison operators
        op = matcher.get("op") or matcher.get("operator")
        if op in ("!=", "<", "<=", ">", ">="):
            val = matcher.get("value", matcher.get("expected"))
            if val == 0 and "domain" not in matcher:
                return False, f"[VAGUE_MATCHER_REJECTED] Vague comparison '{op} {val}' without explicit domain is forbidden"
        if matcher.get("matcher_type") in ("not_zero", "is_error", "negative") and "domain" not in matcher:
            return False, f"[VAGUE_MATCHER_REJECTED] Untyped generic error check '{matcher.get('matcher_type')}' is forbidden"
    return True, "OK"


def resolve_domain_expected_value(domain: str, symbol: Optional[str], raw_value: Optional[int]) -> int:
    """Resolve expected raw integer value for a given domain and symbol/raw_value."""
    if raw_value is not None:
        return int(raw_value)
    if not symbol:
        raise ValueError(f"Neither symbol nor raw_value specified for domain '{domain}'")
    
    if domain == "esp_err":
        if symbol not in ESP_ERR_SYMBOLS:
            raise KeyError(f"Unknown esp_err symbol: {symbol}")
        return ESP_ERR_SYMBOLS[symbol]
    elif domain == "wink_status":
        if symbol not in WINK_STATUS_SYMBOLS:
            raise KeyError(f"Unknown wink_status symbol: {symbol}")
        return WINK_STATUS_SYMBOLS[symbol]
    elif domain == "posix_errno":
        if symbol not in POSIX_ERRNO_SYMBOLS:
            raise KeyError(f"Unknown posix_errno symbol: {symbol}")
        return POSIX_ERRNO_SYMBOLS[symbol]
    elif domain == "nimble_hs":
        if symbol not in NIMBLE_HS_SYMBOLS:
            raise KeyError(f"Unknown nimble_hs symbol: {symbol}")
        return NIMBLE_HS_SYMBOLS[symbol]
    else:
        raise ValueError(f"Unsupported error domain: {domain}")


def match_error_assertion(expected: Any, actual: Any) -> Tuple[bool, str]:
    """Match actual observation against a domain-typed error assertion.
    
    Returns:
        (True, "OK") if matching succeeds.
        (False, error_reason) if validation or domain constraints fail.
    """
    if isinstance(expected, dict) and "assert_error" in expected:
        expected = expected["assert_error"]

    if not isinstance(expected, dict):
        return False, "[SCHEMA_ERROR] Expected matcher must be a dictionary specification"

    domain = expected.get("domain")
    if not domain or domain not in VALID_DOMAINS:
        return False, f"[INVALID_DOMAIN] Domain '{domain}' must be one of {sorted(VALID_DOMAINS)}"

    symbol = expected.get("symbol")
    raw_val = expected.get("raw_value")
    if symbol is None and raw_val is None:
        return False, "[SPEC_ERROR] Error matcher must specify at least 'symbol' or 'raw_value'"

    try:
        expected_raw = resolve_domain_expected_value(domain, symbol, raw_val)
    except Exception as exc:
        return False, f"[SPEC_ERROR] Failed to resolve expected value: {exc}"

    # Extract actual integer value
    actual_int: int
    if isinstance(actual, (int, float)):
        actual_int = int(actual)
    elif isinstance(actual, dict) and "raw_value" in actual:
        actual_int = int(actual["raw_value"])
    elif isinstance(actual, dict) and "code" in actual:
        actual_int = int(actual["code"])
    elif isinstance(actual, str) and actual.lstrip("-").isdigit():
        actual_int = int(actual)
    elif isinstance(actual, str) and actual.startswith("0x"):
        try:
            actual_int = int(actual, 16)
        except ValueError:
            return False, f"[TYPE_ERROR] Cannot parse hex string actual observation: {actual}"
    elif isinstance(actual, dict) and "symbol" in actual and actual["symbol"] == symbol:
        actual_int = expected_raw
    else:
        return False, f"[TYPE_ERROR] Actual observation '{actual}' cannot be parsed as an error code"

    # Domain-specific sign and range polarity checks
    if domain == "esp_err":
        # ESP-IDF errors are positive (0x101..0x10D, 0x3000..), except ESP_FAIL (-1) and ESP_OK (0)
        if actual_int < -1:
            return False, (
                f"[ERROR_DOMAIN_MISMATCH] esp_err domain received negative code {actual_int}; "
                f"ESP-IDF error codes are positive (e.g. 0x{expected_raw:x} / {expected_raw})"
            )
        if expected_raw > 0 and actual_int <= 0:
            return False, (
                f"[VALUE_MISMATCH] Expected positive esp_err {symbol or hex(expected_raw)} "
                f"({expected_raw}), got {actual_int}"
            )

    elif domain == "wink_status":
        # Wink status errors are strictly negative (ADR-0001), 0 is success
        if expected_raw < 0 and actual_int > 0:
            return False, (
                f"[ERROR_DOMAIN_MISMATCH] wink_status domain received positive code {actual_int}; "
                f"Wink error codes must be negative (ADR-0001, expected {expected_raw})"
            )
        if expected_raw < 0 and actual_int == 0:
            return False, f"[VALUE_MISMATCH] Expected error wink_status {expected_raw}, got success 0"

    elif domain == "posix_errno":
        # POSIX errno values are strictly positive integers
        if actual_int < 0:
            return False, (
                f"[ERROR_DOMAIN_MISMATCH] posix_errno received negative value {actual_int}; "
                f"POSIX errno must be positive"
            )

    elif domain == "nimble_hs":
        # NimBLE host status codes are strictly positive integers
        if actual_int < 0:
            return False, (
                f"[ERROR_DOMAIN_MISMATCH] nimble_hs received negative status {actual_int}; "
                f"NimBLE status must be positive"
            )

    # Check exact value equality
    if actual_int != expected_raw:
        return False, (
            f"[VALUE_MISMATCH] Domain '{domain}' expected {symbol or ''} ({expected_raw}), "
            f"observed {actual_int}"
        )

    return True, "OK"
