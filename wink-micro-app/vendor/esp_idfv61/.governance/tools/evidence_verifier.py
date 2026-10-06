# SPDX-License-Identifier: Apache-2.0
"""
evidence_verifier.py (tools wrapper)
====================================
Re-exports evidence_verifier from .governance/gates/evidence_verifier.py.
"""
from pathlib import Path
import importlib.util
import sys

GATES_DIR = Path(__file__).resolve().parent.parent / "gates"
if str(GATES_DIR) not in sys.path:
    sys.path.insert(0, str(GATES_DIR))

# Loading this wrapper as "evidence_verifier" must not import itself again.
_spec = importlib.util.spec_from_file_location(
    "_esp_governance_evidence_verifier", GATES_DIR / "evidence_verifier.py"
)
_implementation = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_implementation)
for _name in (
    "compute_file_sha256", "compute_assets_composite_sha256",
    "compute_scenario_sha256", "resolve_execution_report_path",
    "verify_execution_report", "verify_evidence", "write_evidence_for_app", "main",
):
    globals()[_name] = getattr(_implementation, _name)

if __name__ == "__main__":
    main()
