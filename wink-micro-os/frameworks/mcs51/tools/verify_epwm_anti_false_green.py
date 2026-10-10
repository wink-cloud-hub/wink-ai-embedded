#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Automated Mutation Defense Verification Suite for CMS8S78xx EPWM
(PLAN-20261010-CMS8S78XX-ANTI-FALSE-GREEN-PHASE2-EPWM, S4 / Anti-False-Green Gate).

Validates that all 5 EPWM hardware simulation defense boundaries fail LOUD and CLEAR
when faults are injected, and that none degrade into silent false greens, crashes, or timeouts:

  1. MUT-PG-DISABLE   : Disable physical PG pin driving in cms8s_epwm.cpp
                        -> Waveform evaluator detects no transitions / frequency 0.
  2. MUT-COMP-INVERT  : Force PG0/PG1 in-phase instead of complementary
                        -> Point assertion detects complementary rule violation (PG1 != 0).
  3. MUT-FB-IGNORE    : Ignore FB0 external brake pin input
                        -> Steady-state hold assertion fails (signal keeps oscillating).
  4. MUT-CLOCK-FIXED  : Prescaler/clock calculation distortion (halved frequency)
                        -> Waveform evaluator detects frequency mismatch (~2500Hz vs 4998.96Hz).
  5. MUT-STOP-NO-CNTE : Stop mode hardware PWMCNTE clearing skipped
                        -> Post-release frozen hold assertion fails (resumes oscillating).

Strict Acceptance Criteria:
  - Exit code must be non-zero (1).
  - Failure reason MUST match expected evaluator regex (AssertionFailed).
  - Zero crashes (SIGSEGV, abort, Wasm panic, out-of-bounds memory).
  - Zero timeouts.
  - Reversion to 100% byte-exact original code guaranteed via try...finally.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
EMBEDDED_ROOT = HERE.parents[3]
CMS8S_EPWM_CPP = HERE.parent / "chips" / "cms8s78xx" / "src" / "cms8s_epwm.cpp"

CRASH_PATTERNS = [
    re.compile(r"Segmentation fault", re.IGNORECASE),
    re.compile(r"SIGSEGV", re.IGNORECASE),
    re.compile(r"RuntimeError", re.IGNORECASE),
    re.compile(r"memory access out of bounds", re.IGNORECASE),
    re.compile(r"abort\(\) at", re.IGNORECASE),
    re.compile(r"panic", re.IGNORECASE),
]

MUTATIONS = [
    {
        "id": "MUT-PG-DISABLE",
        "description": "Disable physical PG pin driving in cms8s_epwm.cpp",
        "app": "wink-micro-app/vendor/cms8s78xx/epwm_down_count",
        "scenario": "wink-micro-app/vendor/cms8s78xx/epwm_down_count/unisim-scenarios/epwm_down_count.scenario.json",
        "target_original": """        // If channel output is enabled in PWMOE, drive physical pin
        if (pwmoe & (1u << ch)) {
            const uint16_t phys_pin = resolve_pg_physical_pin(ctx, ch);
            if (phys_pin != 0xFFFFu) {
                js_pal_gpio_write(phys_pin, out_level != 0, MCS51_DRIVE_SUPPLY);
            }
        }""",
        "target_mutated": """        // If channel output is enabled in PWMOE, drive physical pin
        if (pwmoe & (1u << ch)) {
            const uint16_t phys_pin = resolve_pg_physical_pin(ctx, ch);
            if (phys_pin != 0xFFFFu) {
                (void)phys_pin; (void)out_level; // MUT-PG-DISABLE
            }
        }""",
        "expected_regex": r"(?:0 rising edges|differs from 4998\.96|No observation data|Step #1|Calculated frequency 0\.00Hz)",
        "expected_explanation": "ASSERT_WAVEFORM Pin 16 observes 0 edges / frequency mismatch",
    },
    {
        "id": "MUT-COMP-INVERT",
        "description": "Force PG0/PG1 in-phase instead of complementary",
        "app": "wink-micro-app/vendor/cms8s78xx/epwm_down_count",
        "scenario": "wink-micro-app/vendor/cms8s78xx/epwm_down_count/unisim-scenarios/epwm_down_count.scenario.json",
        "target_original": """    if (pwm_mode == 0x01u) {
        // Complementary: PG1 = !PG0, PG3 = !PG2
        mode_levels[0] = raw_levels[0];
        mode_levels[1] = raw_levels[0] ? 0u : 1u;
        mode_levels[2] = raw_levels[2];
        mode_levels[3] = raw_levels[2] ? 0u : 1u;
    }""",
        "target_mutated": """    if (pwm_mode == 0x01u) {
        // Complementary: PG1 = !PG0, PG3 = !PG2
        mode_levels[0] = raw_levels[0];
        mode_levels[1] = raw_levels[0]; // MUT-COMP-INVERT: in-phase
        mode_levels[2] = raw_levels[2];
        mode_levels[3] = raw_levels[2]; // MUT-COMP-INVERT: in-phase
    }""",
        "expected_regex": r"(?:Expected 0, got 1|Step #6|ASSERT_POINT)",
        "expected_explanation": "ASSERT_POINT at 10050us intercepts PG1=1 in active window",
    },
    {
        "id": "MUT-FB-IGNORE",
        "description": "Ignore FB0 external brake pin input",
        "app": "wink-micro-app/vendor/cms8s78xx/epwm_brake_fb",
        "scenario": "wink-micro-app/vendor/cms8s78xx/epwm_brake_fb/unisim-scenarios/epwm_brake_fb.scenario.json",
        "target_original": """    bool fb0_brake = false;
    if (pwmfbkc & 0x01u) { // PWMFB0EN
        const uint8_t ps_fb0 = ctx->xdata_shadow[CMS8S_XSFR_PS_FB0];
        const uint8_t pin_val = resolve_ps_pin_val(ctx, ps_fb0, 1, 4);
        const uint8_t fb0_es = (pwmfbkc >> 2u) & 0x01u;
        fb0_brake = (pin_val == fb0_es);
    }""",
        "target_mutated": """    bool fb0_brake = false;
    if (pwmfbkc & 0x01u) { // PWMFB0EN
        const uint8_t ps_fb0 = ctx->xdata_shadow[CMS8S_XSFR_PS_FB0];
        const uint8_t pin_val = resolve_ps_pin_val(ctx, ps_fb0, 1, 4);
        const uint8_t fb0_es = (pwmfbkc >> 2u) & 0x01u;
        fb0_brake = false; // MUT-FB-IGNORE: FB0 input ignored
    }""",
        "expected_regex": r"(?:max continuous hold|Step #4|stableLevel)",
        "expected_explanation": "ASSERT_WAVEFORM stableLevel intercepts continuing oscillations",
    },
    {
        "id": "MUT-CLOCK-FIXED",
        "description": "Distort clock prescaler calculation (halved frequency)",
        "app": "wink-micro-app/vendor/cms8s78xx/epwm_down_count",
        "scenario": "wink-micro-app/vendor/cms8s78xx/epwm_down_count/unisim-scenarios/epwm_down_count.scenario.json",
        "target_original": """    const uint8_t div_reg = ctx->xdata_shadow[XSFR_PWM0DIV + ch];
    if (div_reg == 0xFFu) {
        // EPWM_CLK_DIV_1 (0xFF): 直通系统主频 Fsys，旁路第一级预分频！
        return fsys;
    }""",
        "target_mutated": """    const uint8_t div_reg = ctx->xdata_shadow[XSFR_PWM0DIV + ch];
    if (div_reg == 0xFFu) {
        // EPWM_CLK_DIV_1 (0xFF): 直通系统主频 Fsys，旁路第一级预分频！
        return fsys / 2u; // MUT-CLOCK-FIXED
    }""",
        "expected_regex": r"(?:differs from 4998\.96|Calculated frequency 2499|Step #1)",
        "expected_explanation": "ASSERT_WAVEFORM frequency intercepts carrier frequency shift",
    },
    {
        "id": "MUT-STOP-NO-CNTE",
        "description": "Skip Stop mode hardware PWMCNTE clearing",
        "app": "wink-micro-app/vendor/cms8s78xx/epwm_brake_stop",
        "scenario": "wink-micro-app/vendor/cms8s78xx/epwm_brake_stop/unisim-scenarios/epwm_brake_stop.scenario.json",
        "target_original": """            const uint8_t brake_mode = pwmbrkc & 0x03u;
            if (brake_mode == 0x00u) {
                // Stop Mode: 故障发生时硬件清零 PWMCNTE 停止运行！(手册第 126 页)
                ctx->xdata_shadow[XSFR_PWMCNTE] = 0;
            }""",
        "target_mutated": """            const uint8_t brake_mode = pwmbrkc & 0x03u;
            if (brake_mode == 0x00u) {
                // Stop Mode: 故障发生时硬件清零 PWMCNTE 停止运行！(手册第 126 页)
                // MUT-STOP-NO-CNTE: hardware clearing skipped
                (void)brake_mode;
            }""",
        "expected_regex": r"(?:differs from 0|Calculated frequency 2500|Step #10|max continuous hold)",
        "expected_explanation": "ASSERT_WAVEFORM pin 26 intercepts continuing zero interrupts (2500 Hz vs 0 Hz)",
    },
]


def resolve_winkcli() -> str:
    cli = shutil.which("winkcli")
    if not cli:
        raise RuntimeError("Unable to find 'winkcli' executable on PATH. Please install winkcli globally (e.g. 'pip install wink-tools') or ensure it is added to PATH.")
    return cli


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def run_scenario(winkcli: str, app_rel: str, scenario_rel: str, timeout: int = 120) -> tuple[int, str, str]:
    app_path = EMBEDDED_ROOT / app_rel
    scenario_path = EMBEDDED_ROOT / scenario_rel
    cmd = [
        winkcli,
        "sim",
        "run",
        "--app",
        str(app_path),
        "--scenarios",
        str(scenario_path),
    ]
    env = dict(os.environ)
    env["PYTHONIOENCODING"] = "utf-8"
    env["PYTHONUTF8"] = "1"

    proc = subprocess.run(
        cmd,
        cwd=str(EMBEDDED_ROOT),
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
    )
    return proc.returncode, proc.stdout or "", proc.stderr or ""


def main() -> int:
    parser = argparse.ArgumentParser(description="CMS8S78xx EPWM Anti-False-Green Mutation Verification")
    parser.add_argument("--mutation", type=str, help="Run only a specific mutation by ID")
    parser.add_argument("--skip-baseline", action="store_true", help="Skip the initial baseline sanity check")
    args = parser.parse_args()

    winkcli = resolve_winkcli()
    if not CMS8S_EPWM_CPP.is_file():
        print(f"[FATAL] Target source file not found: {CMS8S_EPWM_CPP}", file=sys.stderr)
        return 2

    original_code = CMS8S_EPWM_CPP.read_text(encoding="utf-8")
    original_sha = sha256_text(original_code)

    print("================================================================================")
    print(" CMS8S78xx EPWM Anti-False-Green Mutation Verification Suite (Phase 2 / S4)")
    print("================================================================================")
    print(f" Source Under Test : {CMS8S_EPWM_CPP.relative_to(EMBEDDED_ROOT)}")
    print(f" Source SHA256     : {original_sha}")
    print(f" Launcher Path     : {winkcli}")
    print("================================================================================\n")

    # Step 0: Initial Baseline Sanity Check (must PASS 100%)
    if not args.skip_baseline:
        print("[0/5] Running Baseline Sanity Check (unmutated epwm_down_count)...")
        t0 = time.time()
        ret, stdout, stderr = run_scenario(
            winkcli,
            "wink-micro-app/vendor/cms8s78xx/epwm_down_count",
            "wink-micro-app/vendor/cms8s78xx/epwm_down_count/unisim-scenarios/epwm_down_count.scenario.json",
        )
        elapsed = time.time() - t0
        if ret != 0:
            print(f"  [ERROR] Baseline failed with code {ret}!\nStdout:\n{stdout}\nStderr:\n{stderr}")
            return 1
        print(f"  -> Baseline PASS ({elapsed:.2f}s)\n")

    selected = MUTATIONS
    if args.mutation:
        selected = [m for m in MUTATIONS if m["id"] == args.mutation]
        if not selected:
            print(f"[FATAL] Unknown mutation ID: {args.mutation}", file=sys.stderr)
            return 2

    results = []
    overall_pass = True

    try:
        for idx, mut in enumerate(selected, 1):
            mut_id = mut["id"]
            print(f"[{idx}/{len(selected)}] Testing Mutation: {mut_id}")
            print(f"    Intent    : {mut['description']}")
            print(f"    Target App: {mut['app']}")

            if mut["target_original"] not in original_code:
                print(f"    [FAIL] Original target string not found in {CMS8S_EPWM_CPP.name}!")
                results.append({
                    "id": mut_id,
                    "status": "FAIL",
                    "reason": "Target original snippet not found in source code",
                })
                overall_pass = False
                continue

            # Apply mutation
            mutated_code = original_code.replace(mut["target_original"], mut["target_mutated"], 1)
            CMS8S_EPWM_CPP.write_text(mutated_code, encoding="utf-8")

            try:
                t0 = time.time()
                ret, stdout, stderr = run_scenario(winkcli, mut["app"], mut["scenario"])
                elapsed = time.time() - t0
            finally:
                # Revert immediately to protect code integrity
                CMS8S_EPWM_CPP.write_text(original_code, encoding="utf-8")

            combined_output = stdout + "\n" + stderr

            # Check 1: Must NOT crash or abort
            crashed = False
            crash_reason = None
            for cp in CRASH_PATTERNS:
                m = cp.search(combined_output)
                if m:
                    crashed = True
                    crash_reason = f"Crashed ({m.group(0)})"
                    break

            if crashed:
                print(f"    [FAIL] Process crashed unexpectedly: {crash_reason}")
                results.append({"id": mut_id, "status": "FAIL", "reason": crash_reason})
                overall_pass = False
                continue

            # Check 2: Must fail (non-zero returncode)
            if ret == 0:
                print(f"    [FAIL] False Green! Mutation did not trigger a test failure (exit code 0)")
                results.append({
                    "id": mut_id,
                    "status": "FALSE_GREEN",
                    "reason": "Returned exit code 0 despite injected fault",
                })
                overall_pass = False
                continue

            # Check 3: Error message must match expected assertion regex
            expected_re = re.compile(mut["expected_regex"], re.IGNORECASE)
            matched = expected_re.search(combined_output)
            if not matched:
                print(f"    [FAIL] Intercepted with exit code {ret}, but error message did not match expected pattern: {mut['expected_regex']}")
                print(f"    Snippet of output:\n{combined_output[-500:]}")
                results.append({
                    "id": mut_id,
                    "status": "MISMATCH",
                    "reason": f"Exit code {ret} but message did not match /{mut['expected_regex']}/",
                })
                overall_pass = False
                continue

            print(f"    [PASS] Correctly intercepted (exit={ret}, pattern='{matched.group(0)}', {elapsed:.2f}s)")
            results.append({
                "id": mut_id,
                "status": "PASS",
                "exit_code": ret,
                "matched": matched.group(0),
                "elapsed": f"{elapsed:.2f}s",
            })

    finally:
        # Ultimate fail-safe to restore file
        CMS8S_EPWM_CPP.write_text(original_code, encoding="utf-8")
        current_sha = sha256_text(CMS8S_EPWM_CPP.read_text(encoding="utf-8"))
        if current_sha != original_sha:
            print("[CRITICAL] Failed to restore original code bytes!", file=sys.stderr)
            overall_pass = False

    print("\n================================================================================")
    print(" Mutation Defense Summary Table")
    print("================================================================================")
    print(f" {'ID':<18} | {'Status':<11} | {'Details'}")
    print("--------------------------------------------------------------------------------")
    for r in results:
        status_tag = r["status"]
        if status_tag == "PASS":
            details = f"Exit={r['exit_code']} | Match: '{r['matched']}' ({r['elapsed']})"
        else:
            details = r.get("reason", "Unknown failure")
        print(f" {r['id']:<18} | {status_tag:<11} | {details}")
    print("================================================================================\n")

    if overall_pass:
        print("[SUCCESS] All 5 mutations were strictly intercepted with expected assertion signatures!")
        return 0
    else:
        print("[FAILURE] Mutation defenses failed to meet the anti-false-green standard.", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
