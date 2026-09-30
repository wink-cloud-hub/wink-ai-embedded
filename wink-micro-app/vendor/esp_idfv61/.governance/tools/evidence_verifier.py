# SPDX-License-Identifier: Apache-2.0
"""
evidence_verifier.py (tools wrapper)
====================================
Re-exports evidence_verifier from .governance/gates/evidence_verifier.py.
"""
from pathlib import Path
import sys

GATES_DIR = Path(__file__).resolve().parent.parent / "gates"
if str(GATES_DIR) not in sys.path:
    sys.path.insert(0, str(GATES_DIR))

from evidence_verifier import (  # noqa: F401
    compute_file_sha256,
    compute_assets_composite_sha256,
    compute_scenario_sha256,
    resolve_execution_report_path,
    verify_execution_report,
    verify_evidence,
    write_evidence_for_app,
    main,
)

if __name__ == "__main__":
    main()
