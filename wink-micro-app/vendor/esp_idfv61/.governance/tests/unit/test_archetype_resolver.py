# SPDX-License-Identifier: Apache-2.0
"""
Unit tests for ArchetypeResolver (loop.afg.archetype_resolver).
"""
import pytest
from loop.afg.archetype_resolver import ArchetypeResolver, ArchetypeResolutionError


def test_resolver_resolves_without_inherits():
    resolver = ArchetypeResolver()
    raw = {
        "claims": [{"id": "claim.custom", "evidence_class": "baseline"}],
        "applicability_protocol": {"allow_na": True}
    }
    resolved = resolver.resolve(raw)
    assert resolved["claims"] == raw["claims"]
    assert resolved["applicability_protocol"] == raw["applicability_protocol"]


def test_resolver_requires_archetype_claim_diff_on_inheritance():
    resolver = ArchetypeResolver()
    raw = {
        "inherits": "archetype_start",
    }
    with pytest.raises(ArchetypeResolutionError) as exc_info:
        resolver.resolve(raw)
    assert "ERR_ARCHETYPE_CLAIM_DIFF_MISSING" in str(exc_info.value)


def test_resolver_rejects_empty_intersection_diff():
    resolver = ArchetypeResolver()
    raw = {
        "inherits": "archetype_start",
        "archetype_claim_diff": ["claim.completely.unrelated.foo"],
    }
    with pytest.raises(ArchetypeResolutionError) as exc_info:
        resolver.resolve(raw)
    assert "ERR_ARCHETYPE_CLAIM_EMPTY_DIFF" in str(exc_info.value)


def test_resolver_resolves_valid_archetype_start():
    resolver = ArchetypeResolver()
    raw = {
        "inherits": "archetype_start",
        "archetype_claim_diff": [
            "claim.start.boot_banner",
            "claim.start.countdown_progress"
        ],
    }
    resolved = resolver.resolve(raw)
    assert "claims" in resolved
    claim_ids = {c["id"] for c in resolved["claims"]}
    assert "claim.start.boot_banner" in claim_ids
    assert "claim.start.countdown_progress" in claim_ids
