# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for g2_pal_naming.py.

The original test fixture used a comment containing a bare "ws2812" token to
satisfy the rule, which masked the fact that the regex-based implementation could
not detect identifiers such as `pal_ws2812_write`. These tests are written against
the real layer-inversion case from the codebase:
wink-micro-os/targets/wasm/pal_wasm_ch4_buffer.c::pal_ws2812_write.
"""

from gates.rules import g2_pal_naming


def test_pal_naming_clean_file(tmp_path):
    pal_dir = tmp_path / "pal"
    pal_dir.mkdir(parents=True, exist_ok=True)
    pal_file = pal_dir / "pal_clean.h"
    pal_file.write_text("void pal_gpio_set(int pin, int val);\n", encoding="utf-8")

    context = {
        "workspace_root": str(tmp_path),
        "changed_files": ["pal/pal_clean.h"],
    }
    findings = g2_pal_naming.run(context)
    assert len(findings) == 0


def test_pal_naming_detects_identifier_not_just_comment(tmp_path):
    """The regression this rule exists for: an API named after a device."""
    target_dir = tmp_path / "targets" / "wasm"
    target_dir.mkdir(parents=True, exist_ok=True)
    target_file = target_dir / "pal_wasm_ch4_buffer.c"
    # No comment at all -- only the offending API surface.
    target_file.write_text(
        "wink_status_t pal_ws2812_write(wink_pin_t pin, const uint8_t *rgb_buf, size_t n)\n"
        "{\n"
        "    js_pal_ws2812_write((uint16_t)pin, rgb_buf, n);\n"
        "    return WINK_OK;\n"
        "}\n",
        encoding="utf-8",
    )

    context = {
        "workspace_root": str(tmp_path),
        "changed_files": ["targets/wasm/pal_wasm_ch4_buffer.c"],
    }
    findings = g2_pal_naming.run(context)
    # One finding per offending line (first matching token reported).
    assert len(findings) == 2, findings
    assert all(f["severity"] == "error" for f in findings)
    assert all("ws2812" in f["message"] for f in findings)


def test_pal_naming_underscore_is_a_word_separator(tmp_path):
    """`\b` treats `_` as a word char, so identifiers were previously invisible."""
    assert g2_pal_naming.match_forbidden_token(
        "void pal_ws2812_send(void);", ["ws2812"]) == "ws2812"
    assert g2_pal_naming.match_forbidden_token(
        "int pal_necir_rx(void);", ["necir"]) == "necir"
    assert g2_pal_naming.match_forbidden_token(
        "void oled_display_init(void);", ["oled"]) == "oled"
    assert g2_pal_naming.match_forbidden_token(
        "int touch_pad_read(void);", ["touch_pad"]) == "touch_pad"


def test_generic_bus_address_is_not_treated_as_device_leakage():
    """`i2c_addr` was removed from the vocabulary: an I2C bus address is a
    cross-MCU generic concept, and PAL_RESOURCE_I2C_ADDR is the correct way to
    express it. Device-specific addressing remains covered by `at24`."""
    assert "i2c_addr" not in g2_pal_naming.DEFAULT_FORBIDDEN
    assert g2_pal_naming.match_forbidden_token(
        "PAL_RESOURCE_I2C_ADDR = 4,", g2_pal_naming.DEFAULT_FORBIDDEN) is None
    assert g2_pal_naming.match_forbidden_token(
        "int at24_read(void);", g2_pal_naming.DEFAULT_FORBIDDEN) == "at24"


def test_pal_naming_allowlist_suppresses_reviewed_entries(tmp_path):
    """Allowlist is path-scoped so a reviewed PAL file stays clean while the
    equivalent violation elsewhere is still reported."""
    pal_dir = tmp_path / "pal" / "include" / "hal"
    pal_dir.mkdir(parents=True, exist_ok=True)
    (pal_dir / "pal_rmt.h").write_text(
        "/* generic pulse TX; generalizes over LED strips (WS2812) */\n", encoding="utf-8")

    tgt_dir = tmp_path / "targets" / "wasm"
    tgt_dir.mkdir(parents=True, exist_ok=True)
    (tgt_dir / "pal_ws2812.c").write_text(
        "void pal_ws2812_write(void);\n", encoding="utf-8")

    allowlist = [{
        "path": "pal/include/hal/pal_rmt.h",
        "token": "ws2812",
        "reason": "generic capability documentation",
    }]
    ctx = {"workspace_root": str(tmp_path)}

    clean = g2_pal_naming.run(
        {**ctx, "changed_files": ["pal/include/hal/pal_rmt.h"]},
        {"allowlist": allowlist})
    assert clean == []

    still_caught = g2_pal_naming.run(
        {**ctx, "changed_files": ["targets/wasm/pal_ws2812.c"]},
        {"allowlist": allowlist})
    assert len(still_caught) == 1


def test_pal_naming_rejects_empty_forbidden_list(tmp_path):
    """Fail-closed: an emptied vocabulary must not silently disable the scan."""
    ctx = {"workspace_root": str(tmp_path), "changed_files": ["pal/x.h"]}
    try:
        g2_pal_naming.run(ctx, {"forbidden_patterns": []})
    except ValueError:
        return
    raise AssertionError("empty forbidden_patterns must raise, not pass")
