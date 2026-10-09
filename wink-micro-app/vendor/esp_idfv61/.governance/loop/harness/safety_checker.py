# SPDX-License-Identifier: Apache-2.0
"""
loop.harness.safety_checker - Heuristic Safety Checker & Tiered C Parser
========================================================================
Independent module enforcing H-1 to H-8 rules on candidate patches and
preventing architecture corruption during autonomous self-healing.
Supports zero-dependency pure-Python Tier 1 C parsing with optional Tier 2
tree-sitter acceleration.
"""
from __future__ import annotations

import re
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

# Tier 2 optional import
try:
    import tree_sitter  # type: ignore
    import tree_sitter_c  # type: ignore
    HAS_TREE_SITTER = True
except ImportError:
    HAS_TREE_SITTER = False


SAFE_WRITE_WHITELIST = [
    "wink-micro-app/vendor/esp_idfv61/",
    "wink-micro-os/frameworks/esp_idf/src/",
    "wink-micro-os/frameworks/esp_idf/include/",
    "wink-micro-os/pal/include/hal/",
    "wink-micro-os/targets/wasm/",
    "wink-micro-os/targets/esp32/",
    "wink-plugin-peripherals/builtin/",
    "docs/implementation-plans/wokwi-dal-type-coverage-type/",
]

STRICT_BLOCKED_PATHS = [
    "wink-micro-os/pal/include/osal/",
    "wink-micro-os/pal/include/internal/",
    "wink-micro-os/pal/src/osal/",
    "packages/unisim/",
    ".governance/gates/",
    ".github/",
    "CMakeLists.txt",
]


class TieredCParser:
    """Zero-dependency pure-Python C Tokenizer and bracket scanner with optional AST upgrade."""

    def __init__(self, use_ast_if_available: bool = True):
        self.has_ast = HAS_TREE_SITTER and use_ast_if_available

    @staticmethod
    def strip_comments_and_strings(code: str) -> str:
        """Strip C line comments, block comments, and literal strings/chars safely."""
        def replacer(match):
            s = match.group(0)
            if s.startswith("/"):
                return " "
            elif s.startswith('"'):
                return '""'
            elif s.startswith("'"):
                return "''"
            return s

        pattern = re.compile(
            r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^\\"\n])*"|\'(?:\\.|[^\\\'\n])*\'',
            re.DOTALL,
        )
        return pattern.sub(replacer, code)

    def extract_function_bodies(self, c_code: str) -> List[Tuple[str, str]]:
        """Extract function signature and body pairs using bracket depth scanning.

        Returns list of (signature, body_content).
        """
        clean_code = self.strip_comments_and_strings(c_code)
        functions = []
        i = 0
        n = len(clean_code)

        while i < n:
            ch = clean_code[i]
            if ch == "{":
                # Find start of function header before '{'
                header_start = max(0, clean_code.rfind(";", 0, i) + 1)
                header_start = max(header_start, clean_code.rfind("}", 0, i) + 1)
                sig = clean_code[header_start:i].strip()

                # Match closing brace
                depth = 1
                body_start = i + 1
                j = body_start
                while j < n and depth > 0:
                    if clean_code[j] == "{":
                        depth += 1
                    elif clean_code[j] == "}":
                        depth -= 1
                    j += 1

                if depth == 0:
                    body = clean_code[body_start : j - 1]
                    # Filter out struct / enum definitions
                    if not re.search(r"\b(struct|enum|union)\b", sig) and "(" in sig:
                        functions.append((sig, body))
                    i = j
                    continue
            i += 1

        return functions

    def is_empty_stub(self, func_body: str) -> bool:
        """Check if function body is a dummy empty stub or trivial constant return.

        A function is flagged as an empty stub (H-1) if:
        1. It has 0 functional statements; OR
        2. Its only non-logging statement is returning a constant (ESP_OK, 0, true, NULL, void).
        """
        # Remove log macros, assertions, and variable declarations
        lines = func_body.split(";")
        substantive_stmts = []

        for stmt in lines:
            s = stmt.strip()
            if not s:
                continue
            # Ignore log / debug macros
            if re.match(r"^(ESP_LOG[EWIUDV]|WINK_LOG|printf|assert)\b", s):
                continue
            # Ignore pure variable declarations like 'int x = 0' or 'esp_err_t ret'
            if re.match(r"^(int|uint\w+_t|bool|size_t|esp_err_t|wink_status_t|\w+_handle_t)\s+\w+(\s*=\s*[^;]+)?$", s):
                continue
            substantive_stmts.append(s)

        if len(substantive_stmts) == 0:
            return True

        if len(substantive_stmts) == 1:
            only = substantive_stmts[0]
            # Flag if the only substantive statement is returning success/constant
            if re.match(r"^return\s+(ESP_OK|0|true|WINK_OK|NULL|\w+_SUCCESS)\s*$", only) or re.match(r"^return\s*$", only):
                return True

        return False


class HeuristicSafetyChecker:
    """Enforces H-1 to H-8 rules on candidate patch diffs and source code."""

    def __init__(self, workspace_root: Optional[Path] = None):
        self.ws_root = workspace_root
        self.c_parser = TieredCParser()

    @staticmethod
    def extract_touched_files(patch_content: str) -> List[str]:
        """Extract all relative target paths modified, added, or deleted in unified diff."""
        touched: Set[str] = set()
        for line in patch_content.splitlines():
            if line.startswith("+++ b/") or line.startswith("+++ "):
                path_part = line[6:].strip() if line.startswith("+++ b/") else line[4:].strip()
                if path_part != "/dev/null":
                    touched.add(path_part.replace("\\", "/"))
            elif line.startswith("--- a/") or line.startswith("--- "):
                path_part = line[6:].strip() if line.startswith("--- a/") else line[4:].strip()
                if path_part != "/dev/null":
                    touched.add(path_part.replace("\\", "/"))
        return sorted(list(touched))

    def validate_patch_scope(self, patch_content: str) -> Tuple[bool, List[str]]:
        """Validate touched files against whitelist and blocked paths (H-8)."""
        errors = []
        files = self.extract_touched_files(patch_content)
        if not files:
            return False, ["Patch modifies no files or has invalid unified diff header."]

        for f in files:
            # Check strict blocked paths
            for blocked in STRICT_BLOCKED_PATHS:
                if f == blocked or f.startswith(blocked):
                    errors.append(f"H-8 Violation: Modification of strictly blocked path: {f}")

            # Check safe write whitelist
            is_whitelisted = any(f.startswith(w) for w in SAFE_WRITE_WHITELIST)
            if not is_whitelisted:
                errors.append(f"H-8 Violation: File not in safe write whitelist: {f}")

        return (len(errors) == 0, errors)

    def validate_pal_additive_increment(self, patch_content: str) -> Tuple[bool, List[str]]:
        """Validate that any PAL HAL modifications are strictly additive and vendor-neutral (H-5).

        Rules:
        1. Only allow modifications inside wink-micro-os/pal/include/hal/ and targets/.
        2. Prohibit removing or altering existing lines in pal/ headers (- lines with substantive code).
        3. Prohibit any vendor header includes (e.g. #include "esp_*.h" or "freertos/").
        """
        errors = []
        in_pal_header = False

        for line in patch_content.splitlines():
            if line.startswith("+++ b/") or line.startswith("--- a/"):
                filepath = line[6:].strip().replace("\\", "/")
                in_pal_header = filepath.startswith("wink-micro-os/pal/include/")

            if in_pal_header:
                # Disallow deleting substantive lines in PAL headers
                if line.startswith("-") and not line.startswith("---"):
                    deleted_code = line[1:].strip()
                    if deleted_code and not deleted_code.startswith("//") and not deleted_code.startswith("/*"):
                        errors.append(f"H-5 ABI Breaking Violation: Deleting existing line in PAL header: {deleted_code}")

                # Disallow vendor headers in PAL headers
                if line.startswith("+") and not line.startswith("+++"):
                    added_code = line[1:].strip()
                    if re.search(r'#\s*include\s*[<"](esp_|freertos/|driver/|soc/|hal/esp_)', added_code):
                        errors.append(f"H-5 Vendor Neutrality Violation: PAL header includes vendor header: {added_code}")

        return (len(errors) == 0, errors)

    def validate_patch_content(self, patch_content: str) -> Tuple[bool, List[str]]:
        """Validate patch additions for H-1, H-2, H-3, H-4, H-6, H-7."""
        errors = []
        added_lines: List[str] = []
        current_file = ""
        file_added_c_code: Dict[str, List[str]] = {}

        for line in patch_content.splitlines():
            if line.startswith("+++ b/"):
                current_file = line[6:].strip().replace("\\", "/")
                if current_file.endswith(".c") or current_file.endswith(".h"):
                    file_added_c_code[current_file] = []
                continue

            if line.startswith("+") and not line.startswith("+++"):
                added_text = line[1:]
                added_lines.append(added_text)
                if current_file in file_added_c_code:
                    file_added_c_code[current_file].append(added_text)

        # H-2: App name branching / special casing
        for line in added_lines:
            if re.search(r'strstr\s*\(\s*\w+\s*,\s*["\'][\w\-]+["\']\s*\)', line):
                if "app" in line.lower() or "example" in line.lower():
                    errors.append(f"H-2 Violation: Hardcoded application name branching: {line.strip()}")

        # H-3: Canary mutation bypass
        for line in added_lines:
            if re.search(r'\b(BYPASS_CANARY|SKIP_MUTATION|canary_bypass)\b', line, re.IGNORECASE):
                errors.append(f"H-3 Violation: Attempt to bypass Canary kill testing: {line.strip()}")

        # H-4: Dropping compiler warnings
        for line in added_lines:
            if re.search(r'#.*(-Wno-error|-w\b|Wno-all)', line):
                errors.append(f"H-4 Violation: Suppressing compiler warnings: {line.strip()}")

        # H-6: Float PWM anti-pattern (ADR-0066)
        for line in added_lines:
            if re.search(r'\bpal_pwm_set_duty\s*\([^)]*\)', line):
                if not re.search(r'\bpal_pwm_set_duty_bp\b', line):
                    errors.append(f"H-6 Violation: Usage of deprecated floating-point PWM duty API: {line.strip()}")

        # H-7: License check on newly created files in patch
        for f, lines in file_added_c_code.items():
            combined_code = "\n".join(lines)
            if "SPDX-License-Identifier" not in combined_code and len(lines) > 20:
                # If it's a completely new file with substantive lines
                errors.append(f"H-7 Violation: Missing SPDX-License-Identifier in {f}")

        # H-1: Empty stub checking on added C functions
        for f, lines in file_added_c_code.items():
            if f.endswith(".c"):
                code_blob = "\n".join(lines)
                funcs = self.c_parser.extract_function_bodies(code_blob)
                for sig, body in funcs:
                    if self.c_parser.is_empty_stub(body):
                        errors.append(f"H-1 Violation: Empty stub or trivial constant return in {f}: '{sig.strip()[:60]}'")

        return (len(errors) == 0, errors)

    def validate_peripheral_plugin_rules(self, patch_content: str, ws_root: Optional[Path] = None) -> Tuple[bool, List[str]]:
        """Validate peripheral plugin additions for P-1, P-2, P-3, P-4 rules."""
        errors = []
        touched = self.extract_touched_files(patch_content)
        plugin_files = [f for f in touched if f.startswith("wink-plugin-peripherals/builtin/")]

        if not plugin_files:
            return (True, [])

        # Extract peripheral types touched
        types_touched = set()
        for pf in plugin_files:
            parts = pf.replace("\\", "/").split("/")
            # wink-plugin-peripherals/builtin/<type>/...
            if len(parts) >= 3 and parts[2]:
                types_touched.add(parts[2])

        # Forbidden chip-specific model names that should be unified (Rule P-2)
        FORBIDDEN_BARE_TYPES = {
            "mpu6050", "mpu9250", "dht11", "dht22", "ds1307", "hx711",
            "ili9341", "ws2812", "tm1637", "ch340", "cp2102",
        }

        # Valid pin types for manifest (Rule P-3)
        VALID_PIN_TYPES = {
            "gpio", "i2c_scl", "i2c_sda", "spi_sck", "spi_mosi", "spi_miso",
            "spi_cs", "power_vcc", "power_gnd", "analog", "uart_tx", "uart_rx", "pwm",
        }

        # Check P-2: Type consolidation
        for ptype in types_touched:
            if ptype.lower() in FORBIDDEN_BARE_TYPES:
                errors.append(
                    f"P-2 Type Unification Violation: Forbidden chip-specific model name '{ptype}' used as peripheral type. "
                    f"Must unify to canonical type (e.g. 'imu', 'rtc', 'tft', etc.) conforming to 00-master-execution-plan.md."
                )

        # Check P-1: Plan document must exist for any new peripheral type
        plan_files_in_patch = [
            f for f in touched if f.startswith("docs/implementation-plans/wokwi-dal-type-coverage-type/")
        ]
        for ptype in types_touched:
            has_plan_in_patch = any(ptype in pf for pf in plan_files_in_patch)
            has_plan_on_disk = False
            if ws_root:
                plan_dir = ws_root / "docs" / "implementation-plans" / "wokwi-dal-type-coverage-type"
                if plan_dir.is_dir():
                    has_plan_on_disk = any(ptype in p.name for p in plan_dir.glob("*.md"))

            if not has_plan_in_patch and not has_plan_on_disk:
                errors.append(
                    f"P-1 Plan-First Violation: Peripheral plugin '{ptype}' lacks mandatory sub-plan document. "
                    f"Expected docs/implementation-plans/wokwi-dal-type-coverage-type/*-{ptype}-plan.md."
                )

        # Parse added file contents for P-3 and P-4
        current_file = ""
        file_lines: Dict[str, List[str]] = {}
        for line in patch_content.splitlines():
            if line.startswith("+++ b/"):
                current_file = line[6:].strip().replace("\\", "/")
                file_lines[current_file] = []
                continue
            if line.startswith("+") and not line.startswith("+++"):
                if current_file in file_lines:
                    file_lines[current_file].append(line[1:])

        # Check P-3 & P-4 on content
        for fpath, lines in file_lines.items():
            content = "\n".join(lines)
            if fpath.endswith("manifest.json"):
                # Rule P-3: Check pinTypes
                pin_type_matches = re.findall(r'"pinType"\s*:\s*"([^"]+)"', content)
                for pt in pin_type_matches:
                    if pt not in VALID_PIN_TYPES:
                        errors.append(
                            f"P-3 Manifest Integrity Violation: Invalid pinType '{pt}' in {fpath}. "
                            f"Valid pinTypes are: {sorted(VALID_PIN_TYPES)}"
                        )

            if fpath.endswith("simulation.ts"):
                # Rule P-4: Lifecycle reset check
                has_reset_method = bool(re.search(r'\breset\s*\(', content))
                has_attach_reset = bool(re.search(r'onAttach[\s\S]*?(this\.\w+\s*=\s*(0|\[\]|\{\}|null|false))', content))
                if not has_reset_method and not has_attach_reset:
                    errors.append(
                        f"P-4 Lifecycle Reset Violation: Peripheral plugin {fpath} must implement state reset "
                        f"(e.g. reset() method or explicit state clearing in onAttach)."
                    )

        return (len(errors) == 0, errors)

    def validate_all(self, patch_content: str, ws_root: Optional[Path] = None) -> Tuple[bool, List[str]]:
        """Run complete H-1 to H-8 and P-1 to P-4 validation on unified diff patch."""
        all_errors = []

        # Scope & Whitelist (H-8)
        ok_scope, scope_errs = self.validate_patch_scope(patch_content)
        all_errors.extend(scope_errs)

        # PAL Additive & Vendor Neutrality (H-5)
        ok_pal, pal_errs = self.validate_pal_additive_increment(patch_content)
        all_errors.extend(pal_errs)

        # Content Rules (H-1, H-2, H-3, H-4, H-6, H-7)
        ok_content, content_errs = self.validate_patch_content(patch_content)
        all_errors.extend(content_errs)

        # Peripheral Plugin Rules (P-1, P-2, P-3, P-4)
        ok_plugin, plugin_errs = self.validate_peripheral_plugin_rules(patch_content, ws_root=ws_root)
        all_errors.extend(plugin_errs)

        return (len(all_errors) == 0, all_errors)
