#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""
cli.verify_afg - Anti-False-Green Engine Verification & Pilot CLI
================================================================
Standard CLI entry point to verify AFG v1.1 pilot scenarios and legacy remediation.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

_GOV_DIR = Path(__file__).resolve().parent.parent
if str(_GOV_DIR) not in sys.path:
    sys.path.insert(0, str(_GOV_DIR))

from loop.services.afg_verification import PilotVerifier


def main() -> int:
    parser = argparse.ArgumentParser(description="AFG Engine Pilot & Legacy Verifier")
    parser.add_argument("--pilot", action="store_true", help="Run Pilot 3 Scenarios physical evidence verification")
    parser.add_argument("--algo-exercise", action="store_true", help="Run algorithmic decision exercise on synthetic mock vectors")
    parser.add_argument("--triage-legacy", action="store_true", help="Run legacy 46 items triage decision tree")
    parser.add_argument("--apply", action="store_true", help="Apply remediation decisions to checklist.data.json")
    parser.add_argument("--workspace-root", type=str, default=None, help="Workspace root (defaults to checkout anchor)")
    args = parser.parse_args()

    verifier = PilotVerifier(args.workspace_root)
    if args.pilot:
        return verifier.run_pilot(physical=True)
    elif args.algo_exercise:
        return verifier.run_pilot(physical=False)
    elif args.triage_legacy:
        return verifier.triage_legacy_items(apply=args.apply)
    else:
        print("Specify --pilot, --algo-exercise, or --triage-legacy.")
        return 0


if __name__ == "__main__":
    sys.exit(main())
