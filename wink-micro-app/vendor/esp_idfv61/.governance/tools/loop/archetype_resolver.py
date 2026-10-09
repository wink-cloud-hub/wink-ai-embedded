# SPDX-License-Identifier: Apache-2.0
"""
archetype_resolver.py - Archetype Inheritance & ProofPlan Contract Resolver
=============================================================================
Implements Task 4.1 Contract Archetype inheritance resolution:
- Loads base archetype from G/archetypes/<archetype_id>.yaml
- Enforces archetype_claim_diff declaration on inheriting proofplans
- Validates non-empty intersection between archetype standard claims and child claims (ERR_ARCHETYPE_CLAIM_EMPTY_DIFF)
- Merges standard claims, L1 injectors, L2 operators, and applicability protocols
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, List, Optional, Set
import yaml

ARCHETYPES_DIR = Path(__file__).resolve().parents[2] / "archetypes"


class ArchetypeResolutionError(Exception):
    """Raised when archetype inheritance or validation fails."""
    pass


class ArchetypeResolver:
    """Resolves child ProofPlan against parent archetype definitions."""

    def __init__(self, archetypes_dir: Optional[Path] = None):
        self.archetypes_dir = (archetypes_dir or ARCHETYPES_DIR).resolve()

    def load_archetype(self, archetype_id: str) -> Dict[str, Any]:
        """Load archetype definition from YAML file."""
        if not archetype_id.endswith(".yaml"):
            archetype_file = self.archetypes_dir / f"{archetype_id}.yaml"
        else:
            archetype_file = self.archetypes_dir / archetype_id

        if not archetype_file.is_file():
            raise ArchetypeResolutionError(f"Archetype file not found: {archetype_file}")

        try:
            with open(archetype_file, "r", encoding="utf-8") as f:
                data = yaml.safe_load(f)
            if not isinstance(data, dict):
                raise ArchetypeResolutionError(f"Archetype YAML must parse to a dict: {archetype_file}")
            return data
        except Exception as exc:
            if isinstance(exc, ArchetypeResolutionError):
                raise
            raise ArchetypeResolutionError(f"Failed to parse archetype YAML {archetype_file}: {exc}") from exc

    def resolve(self, proofplan: Dict[str, Any]) -> Dict[str, Any]:
        """Resolve a ProofPlan dict, expanding archetype inheritance if specified.
        
        Rules:
        1. If 'inherits' is specified:
           - 'archetype_claim_diff' must be explicitly declared (list or dict).
           - Child claims and Archetype standard claims must have non-empty intersection.
             If intersection is empty, raises ArchetypeResolutionError with ERR_ARCHETYPE_CLAIM_EMPTY_DIFF.
        2. Merges default L1 injectors, L2 operators, and applicability protocols.
        """
        inherits = proofplan.get("inherits")
        if not inherits:
            # Standalone proofplan without inheritance
            claims = proofplan.get("claims", [])
            return {
                "archetype_id": None,
                "claims": claims,
                "l1_injectors": proofplan.get("l1_injectors", []),
                "l2_operators": proofplan.get("l2_operators", []),
                "applicability_protocol": proofplan.get("applicability_protocol", {}),
                "resolved_source": "standalone",
                "custom_recipes": proofplan.get("custom_recipes", {}),
            }

        archetype = self.load_archetype(inherits)
        archetype_id = archetype.get("archetype_id", inherits)

        # Enforce archetype_claim_diff declaration
        if "archetype_claim_diff" not in proofplan:
            raise ArchetypeResolutionError(
                f"[ERR_ARCHETYPE_CLAIM_DIFF_MISSING] Inheriting proofplan must declare 'archetype_claim_diff'"
            )

        claim_diff = proofplan.get("archetype_claim_diff")
        if not isinstance(claim_diff, (list, dict)):
            raise ArchetypeResolutionError(
                f"[ERR_ARCHETYPE_CLAIM_DIFF_INVALID] 'archetype_claim_diff' must be a list or dict, got {type(claim_diff)}"
            )

        # Base archetype claims
        base_claims: List[Dict[str, Any]] = archetype.get("standard_claims", [])
        base_claim_ids: Set[str] = {c.get("id") for c in base_claims if isinstance(c, dict) and "id" in c}

        # Child extra or explicit claims
        child_claims: List[Dict[str, Any]] = proofplan.get("claims", [])
        diff_claim_ids: Set[str] = set()

        if isinstance(claim_diff, list):
            for item in claim_diff:
                if isinstance(item, str):
                    diff_claim_ids.add(item)
                elif isinstance(item, dict) and "id" in item:
                    diff_claim_ids.add(item["id"])
        elif isinstance(claim_diff, dict):
            # Dict mapping claim_id -> overrides or additions
            diff_claim_ids.update(claim_diff.keys())

        # All claims claimed by child
        declared_claim_ids = {c.get("id") for c in child_claims if isinstance(c, dict) and "id" in c} | diff_claim_ids

        # Enforce non-empty intersection between child claims and archetype claims
        effective_intersection = base_claim_ids & declared_claim_ids
        if not effective_intersection and base_claim_ids:
            raise ArchetypeResolutionError(
                f"[ERR_ARCHETYPE_CLAIM_EMPTY_DIFF] Child claims {declared_claim_ids} have empty intersection with archetype claims {base_claim_ids}"
            )

        # Merge claims: start with base claims, apply diff overrides, add child-only claims
        merged_claims_map: Dict[str, Dict[str, Any]] = {}
        for c in base_claims:
            cid = c.get("id")
            if cid:
                merged_claims_map[cid] = dict(c)

        # Apply diff overrides
        if isinstance(claim_diff, dict):
            for cid, override in claim_diff.items():
                if cid in merged_claims_map and isinstance(override, dict):
                    merged_claims_map[cid].update(override)
                elif isinstance(override, dict):
                    merged_claims_map[cid] = override
        elif isinstance(claim_diff, list):
            for item in claim_diff:
                if isinstance(item, dict) and "id" in item:
                    cid = item["id"]
                    if cid in merged_claims_map:
                        merged_claims_map[cid].update(item)
                    else:
                        merged_claims_map[cid] = item

        # Add child claims
        for c in child_claims:
            cid = c.get("id")
            if cid:
                if cid in merged_claims_map:
                    merged_claims_map[cid].update(c)
                else:
                    merged_claims_map[cid] = c

        # Merge L1 injectors
        l1_injectors = list(archetype.get("default_l1_injectors", []))
        override_injectors = proofplan.get("override_injectors", [])
        if override_injectors:
            # Child can override or filter injectors
            l1_injectors = override_injectors
        elif "extra_l1_injectors" in proofplan:
            l1_injectors.extend(proofplan.get("extra_l1_injectors", []))

        # Merge L2 operators
        l2_operators = list(archetype.get("default_l2_operators", []))
        if "override_l2_operators" in proofplan:
            l2_operators = proofplan.get("override_l2_operators", [])
        elif "extra_l2_operators" in proofplan:
            l2_operators.extend(proofplan.get("extra_l2_operators", []))

        # Merge applicability protocol
        applicability = dict(archetype.get("applicability_protocol", {}))
        applicability.update(proofplan.get("applicability_protocol", {}))

        return {
            "archetype_id": archetype_id,
            "claims": list(merged_claims_map.values()),
            "l1_injectors": l1_injectors,
            "l2_operators": l2_operators,
            "applicability_protocol": applicability,
            "resolved_source": "inherited",
            "effective_claim_count": len(merged_claims_map),
            "custom_recipes": proofplan.get("custom_recipes", {}),
        }
