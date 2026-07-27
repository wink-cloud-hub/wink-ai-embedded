"""drivers pack: registry ↔ DAL source consistency (ADR-0046)."""
from __future__ import annotations

from pathlib import Path

from tools.lint.engine.models import Finding


def check_drivers(root: Path) -> list[Finding]:
    """Run list_drivers.check_consistency against ``root`` (wink-micro-os)."""
    from tools.codegen.list_drivers import check_consistency

    findings: list[Finding] = []
    for msg in check_consistency(root):
        findings.append(
            Finding(
                rule_id="drivers.registry_consistency",
                severity="error",
                path="tools/codegen/drivers/",
                line=1,
                column=None,
                message=msg,
                snippet=None,
                help="Add matching dal_*.c/.h or a codegen drivers/<type>.py plugin (ADR-0046).",
                refs=("ADR-0046",),
            )
        )
    return findings
