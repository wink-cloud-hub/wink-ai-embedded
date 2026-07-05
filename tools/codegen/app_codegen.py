#!/usr/bin/env python
"""app_codegen — generate device_tree + app_support + CMake glue from wink-app.json.

Usage:
    python app_codegen.py --config path/to/wink-app.json --out-dir build/generated

Exit codes:
    0  success
    2  schema / validation error (message on stderr)
    1  other errors (IO, template, unexpected)

The tool loads the JSON spec, resolves driver plugins from
``tools/codegen/drivers/``, topologically sorts devices by ``depends_on`` (if
present; otherwise preserves declaration order), then renders four Jinja2
templates. See ``tools/codegen/README.md`` for design rationale.
"""
from __future__ import annotations

import argparse
import difflib
import json
import sys
from pathlib import Path
from typing import Dict, List, Tuple

# ── Package-relative import shim ─────────────────────────────────────
# Allow running either as ``python -m tools.codegen.app_codegen`` (repo root
# on sys.path) or ``python tools/codegen/app_codegen.py`` (script mode).
if __package__ in (None, ""):
    _HERE = Path(__file__).resolve().parent
    # Prepend repo root so ``tools.codegen.drivers`` resolves.
    sys.path.insert(0, str(_HERE.parent.parent))
    from tools.codegen.drivers import get_driver, all_drivers  # noqa: E402
    from tools.codegen.drivers.base import DriverBase  # noqa: E402
else:
    from .drivers import get_driver, all_drivers
    from .drivers.base import DriverBase

from jinja2 import Environment, FileSystemLoader, StrictUndefined  # noqa: E402

TEMPLATES_DIR = Path(__file__).resolve().parent / "templates"
OUTPUT_FILES = (
    ("device_tree.h.j2", "device_tree.h"),
    ("device_tree.c.j2", "device_tree.c"),
    ("app_support.c.j2", "app_support.c"),
    ("app_options.cmake.j2", "app_options.cmake"),
)


# ── Validation ───────────────────────────────────────────────────────
def _die(msg: str, code: int = 2) -> None:
    print(f"error: {msg}", file=sys.stderr)
    sys.exit(code)


def validate_top_level(cfg: dict, source: str) -> None:
    if not isinstance(cfg, dict):
        _die(f"{source}: top-level must be a JSON object")
    if "app_name" not in cfg or not isinstance(cfg["app_name"], str):
        _die(f"{source}: 'app_name' is required and must be a string")
    if "devices" not in cfg or not isinstance(cfg["devices"], dict):
        _die(f"{source}: 'devices' is required and must be an object (may be empty)")
    for k, expected in (
        ("board", str),
        ("services", dict),
        ("callbacks", dict),
        ("state_variables", list),
    ):
        if k in cfg and not isinstance(cfg[k], expected):
            _die(f"{source}: '{k}' must be a {expected.__name__}")


def _resolve_driver(dev_name: str, spec: dict, source: str) -> DriverBase:
    if not isinstance(spec, dict):
        _die(f"{source}: device '{dev_name}' must be an object")
    type_ = spec.get("type")
    if not isinstance(type_, str) or not type_:
        _die(f"{source}: device '{dev_name}' is missing required field 'type'")
    try:
        return get_driver(type_)
    except ValueError:
        known = [d.type for d in all_drivers()]
        hint = difflib.get_close_matches(type_, known, n=1)
        suggestion = f"; did you mean '{hint[0]}'?" if hint else ""
        _die(
            f"{source}: device '{dev_name}' has unknown type '{type_}'"
            f"{suggestion} (known: {sorted(known)})"
        )
    raise RuntimeError("unreachable")  # for type-checker


def _validate_required_fields(dev_name: str, spec: dict,
                              driver: DriverBase, source: str) -> None:
    missing = [f for f in driver.required_fields if f not in spec]
    if missing:
        _die(
            f"{source}: device '{dev_name}' (type '{driver.type}') is missing "
            f"required fields: {missing}"
        )


# ── Topological sort (depends_on) ────────────────────────────────────
def topo_sort(devices: Dict[str, dict], source: str) -> List[str]:
    """Return device names in init order. Devices without ``depends_on``
    keep declaration order; devices with ``depends_on`` come after their
    dependencies. Raises on missing refs or cycles.

    NOTE: full bus-type-based dependency inference (tech-design §4.5) is
    P2 scope. This impl only honours the explicit ``depends_on`` field.
    """
    order: List[str] = []
    seen: Dict[str, str] = {}  # name -> "visiting" | "done"
    names = list(devices.keys())

    def visit(name: str, stack: List[str]) -> None:
        state = seen.get(name)
        if state == "done":
            return
        if state == "visiting":
            cycle = " -> ".join(stack + [name])
            _die(f"{source}: dependency cycle detected: {cycle}")
        seen[name] = "visiting"
        deps = devices[name].get("depends_on", [])
        if not isinstance(deps, list):
            _die(
                f"{source}: device '{name}' has non-list 'depends_on' "
                f"(got {type(deps).__name__})"
            )
        for dep in deps:
            if dep not in devices:
                _die(
                    f"{source}: device '{name}' depends_on unknown device "
                    f"'{dep}'"
                )
            visit(dep, stack + [name])
        seen[name] = "done"
        order.append(name)

    for n in names:
        visit(n, [])
    return order


# ── Context assembly ─────────────────────────────────────────────────
def build_context(cfg: dict, config_source: str) -> dict:
    validate_top_level(cfg, config_source)

    devices_raw: Dict[str, dict] = cfg["devices"]
    resolved: List[Tuple[str, DriverBase, dict]] = []
    for name, spec in devices_raw.items():
        driver = _resolve_driver(name, spec, config_source)
        _validate_required_fields(name, spec, driver, config_source)
        resolved.append((name, driver, spec))

    ordered = topo_sort(devices_raw, config_source)
    lookup = {n: (d, s) for n, d, s in resolved}
    devices_ctx = []
    for name in ordered:
        driver, spec = lookup[name]
        devices_ctx.append({
            "name": name,
            "type": driver.type,
            "is_actuator": driver.is_actuator,
            "headers": driver.get_headers(),
            "device_type": driver.get_device_type(),
            "safe_off_fn": driver.get_safe_off_fn(),
            "config_init": driver.render_config_init(name, spec),
            "post_init_calls": driver.render_post_init_calls(name, spec),
            "deinit": driver.render_deinit(name),
            "service_headers": driver.get_service_headers(name, spec),
            "service_starts": driver.render_service_starts(name, spec),
        })

    # Deduplicate headers preserving order.
    def _dedup(seq: List[str]) -> List[str]:
        seen_: Dict[str, None] = {}
        for x in seq:
            seen_.setdefault(x, None)
        return list(seen_)

    driver_headers = _dedup(h for d in devices_ctx for h in d["headers"])
    service_headers = _dedup(
        h for d in devices_ctx for h in d["service_headers"]
    )
    actuators = [d for d in devices_ctx if d["is_actuator"]]

    # Deduplicate CMake options across all driver instances used.
    cmake_opts: List[str] = []
    seen_opts: Dict[str, None] = {}
    for name in ordered:
        driver, _ = lookup[name]
        for opt in driver.cmake_options():
            if opt not in seen_opts:
                seen_opts[opt] = None
                cmake_opts.append(opt)

    return {
        "config_source": config_source,
        "app_name": cfg["app_name"],
        "board": cfg.get("board", ""),
        "devices": devices_ctx,
        "actuators": actuators,
        "driver_headers": driver_headers,
        "service_headers": service_headers,
        "callbacks": cfg.get("callbacks", {}),
        "services": cfg.get("services", {}),
        "state_variables": cfg.get("state_variables", []),
        "cmake_options": cmake_opts,
    }


# ── Rendering ────────────────────────────────────────────────────────
def make_env() -> Environment:
    return Environment(
        loader=FileSystemLoader(str(TEMPLATES_DIR)),
        undefined=StrictUndefined,
        keep_trailing_newline=True,
        trim_blocks=True,
        lstrip_blocks=True,
    )


def render_all(ctx: dict, out_dir: Path) -> List[Path]:
    env = make_env()
    out_dir.mkdir(parents=True, exist_ok=True)
    written: List[Path] = []
    for tpl_name, out_name in OUTPUT_FILES:
        tpl = env.get_template(tpl_name)
        text = tpl.render(**ctx)
        # Normalise line endings to LF regardless of host OS.
        target = out_dir / out_name
        target.write_text(text, encoding="utf-8", newline="\n")
        written.append(target)
    return written


# ── CLI ──────────────────────────────────────────────────────────────
def main(argv: List[str] | None = None) -> int:
    p = argparse.ArgumentParser(
        prog="app_codegen",
        description="Generate C/CMake glue from a wink-app.json spec.",
    )
    p.add_argument("--config", required=True, type=Path,
                   help="path to wink-app.json")
    p.add_argument("--out-dir", required=True, type=Path,
                   help="output directory (created if missing)")
    p.add_argument("--regen-golden", action="store_true",
                   help=argparse.SUPPRESS)  # hidden helper for golden tests
    args = p.parse_args(argv)

    try:
        source_path = args.config.resolve()
        # Render POSIX-style relative path in banner comments to keep
        # generated files deterministic across machines (golden-test-safe).
        try:
            source_display = source_path.relative_to(Path.cwd().resolve()).as_posix()
        except ValueError:
            source_display = source_path.as_posix()
        try:
            with source_path.open("r", encoding="utf-8") as fp:
                cfg = json.load(fp)
        except FileNotFoundError:
            _die(f"config not found: {source_path}", code=1)
        except json.JSONDecodeError as e:
            _die(f"{source_path}: invalid JSON: {e}")

        ctx = build_context(cfg, source_display)
        render_all(ctx, args.out_dir)
    except SystemExit:
        raise
    except Exception as e:  # noqa: BLE001 — top-level fallback
        print(f"error: unexpected: {e}", file=sys.stderr)
        return 1

    n_dev = len(ctx["devices"])
    n_act = len(ctx["actuators"])
    print(
        f"[codegen] generated {len(OUTPUT_FILES)} files for "
        f"'{ctx['app_name']}' ({n_dev} devices, {n_act} actuators)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
