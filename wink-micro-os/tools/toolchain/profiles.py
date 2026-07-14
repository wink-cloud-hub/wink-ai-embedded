"""Toolchain capability profiles and DAG expansion.

A *profile* is a named bundle of tool capabilities the user wants ready for a
particular workflow (host testing, wasm build, ESP32 flashing, ...). Profiles
can reference other profiles by name; :func:`expand_profile` recursively
resolves those references to a flat, deduplicated list of leaf capability ids
in dependency order (codegen caps before host caps before target-specific caps).

Capability ids used here match the ``id`` field on :class:`Provider`
subclasses in :mod:`tools.toolchain.providers`.
"""
from __future__ import annotations

# Named bundles of capabilities. Values are lists of either:
#   - another profile name (recursively expanded), or
#   - a leaf capability id (provider.id).
#
# Ordering within each list matters: it becomes the ordering in the expanded
# output (with dedup keeping the first occurrence).
PROFILES: dict[str, list[str]] = {
    "codegen": ["python", "jinja2"],
    "host":    ["codegen", "gcc", "cmake", "make"],
    "wasm":    ["host", "emsdk"],                # node optional (see OPTIONAL_CAPS)
    "test":    ["host"],                          # emsdk, node optional
    "esp32":   ["python", "idf"],  # powershell used internally by activate() on Win only; not a declared cap
    "web":     ["node"],
}

# Extra workspace-relative directory anchors a profile needs. These are
# distinct from tool capabilities: they are validated by the CLI (e.g.
# `wink doctor esp32` checks that `esp32_dir` exists) but never resolved by
# providers. Values are informal keys the CLI knows how to map to filesystem
# paths under the workspace root.
WORKSPACE_DEPS: dict[str, list[str]] = {
    "esp32": ["esp32_dir"],
    "web":   ["frontend_dir"],
}

# Optional capabilities per profile: reported but not required for the profile
# to be considered "ready". A missing optional cap should not fail
# `wink doctor`; it should only surface as a warning.
OPTIONAL_CAPS: dict[str, list[str]] = {
    "test": ["emsdk", "node"],
    "wasm": ["node"],
}


def expand_profile(name: str) -> list[str]:
    """Return the flat, deduplicated list of leaf capability ids for a profile.

    Profile references inside :data:`PROFILES` are expanded recursively.
    Ordering: the *first* time a cap id is seen (in DFS order following the
    lists as written) determines its position. This means transitive
    codegen/host caps naturally end up before target-specific caps.

    Raises:
        KeyError:   if ``name`` is not a known profile.
        ValueError: if the profile graph contains a cycle.
    """
    if name not in PROFILES:
        raise KeyError(name)

    result: list[str] = []
    seen: set[str] = set()
    stack: set[str] = set()

    def visit(profile: str) -> None:
        if profile in stack:
            raise ValueError(f"cycle detected in profiles: {profile}")
        stack.add(profile)
        for item in PROFILES[profile]:
            if item in PROFILES:
                # Nested profile: recurse.
                visit(item)
            else:
                # Leaf capability id.
                if item not in seen:
                    seen.add(item)
                    result.append(item)
        stack.discard(profile)

    visit(name)
    return result
