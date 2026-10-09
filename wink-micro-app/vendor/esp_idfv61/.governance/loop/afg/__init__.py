# SPDX-License-Identifier: Apache-2.0
"""
loop.afg - Anti-False-Green (AFG) Verification Engine Package
============================================================
Pure verifier kernel for dual closed-loop verification, multi-domain error matching,
archetype inheritance resolution, build sandboxing, and mutation execution.
"""
from .engine import (
    AFGEngine,
    AFGReceipt,
    ExecutionIdentity,
    EXPECTED_PROBE_ABI_VERSION,
    EXPECTED_PROBE_SIZE_BYTES,
    EXCLUSIVE_SOC_CAPABILITIES,
)
from .error_matcher import (
    is_error_matcher,
    validate_no_vague_matcher,
    resolve_domain_expected_value,
    match_error_assertion,
    ESP_ERR_SYMBOLS,
    WINK_STATUS_SYMBOLS,
    POSIX_ERRNO_SYMBOLS,
    NIMBLE_HS_SYMBOLS,
    VALID_DOMAINS,
)
from .archetype_resolver import (
    ArchetypeResolver,
    ArchetypeResolutionError,
)
from .build_sandbox import (
    BuildSandbox,
    compute_build_cache_key,
    compute_directory_digest,
)
from .mutation_runner import (
    MutationRunner,
    MutationBudgetExceededError,
    EquivalentMutantNotSignedError,
)
from .canonical_sealing import (
    canonical_json_dumps,
    canonical_json_bytes,
    compute_normalized_file_sha256,
    is_envelope_file,
    compute_payload_manifest,
    seal_candidate_payload,
    verify_payload_seal,
    validate_candidate_for_audit,
    ENVELOPE_FILENAMES,
)

__all__ = [
    "AFGEngine",
    "AFGReceipt",
    "ExecutionIdentity",
    "EXPECTED_PROBE_ABI_VERSION",
    "EXPECTED_PROBE_SIZE_BYTES",
    "EXCLUSIVE_SOC_CAPABILITIES",
    "is_error_matcher",
    "validate_no_vague_matcher",
    "resolve_domain_expected_value",
    "match_error_assertion",
    "ESP_ERR_SYMBOLS",
    "WINK_STATUS_SYMBOLS",
    "POSIX_ERRNO_SYMBOLS",
    "NIMBLE_HS_SYMBOLS",
    "VALID_DOMAINS",
    "ArchetypeResolver",
    "ArchetypeResolutionError",
    "BuildSandbox",
    "compute_build_cache_key",
    "compute_directory_digest",
    "MutationRunner",
    "MutationBudgetExceededError",
    "EquivalentMutantNotSignedError",
    "canonical_json_dumps",
    "canonical_json_bytes",
    "compute_normalized_file_sha256",
    "is_envelope_file",
    "compute_payload_manifest",
    "seal_candidate_payload",
    "verify_payload_seal",
    "validate_candidate_for_audit",
    "ENVELOPE_FILENAMES",
]

