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
    ("app_options.cmake.j2", "app_options.cmake"),
    ("device_tree_api.md.j2", "device_tree_api.md"),
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
    ):
        if k in cfg and not isinstance(cfg[k], expected):
            _die(f"{source}: '{k}' must be a {expected.__name__}")
    for forbidden in ("services", "callbacks", "state_variables"):
        if forbidden in cfg:
            _die(f"{source}: field '{forbidden}' is deprecated and no longer supported")


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
# ── Context assembly ─────────────────────────────────────────────────
def _load_board_config(board_name: str, config_source: str) -> dict:
    """Locate and load a board configuration JSON file using a 2-level search path."""
    app_path = Path(config_source).resolve()
    app_dir = app_path.parent

    paths = []
    if board_name.endswith(".json") or "/" in board_name or "\\" in board_name:
        paths.append(app_dir / board_name)
        paths.append(Path(board_name))
    else:
        # Standard search paths: local first, then global codegen boards
        paths.append(app_dir / "boards" / f"{board_name}.json")
        paths.append(app_dir / f"{board_name}.json")
        repo_root = Path(__file__).resolve().parent.parent.parent
        paths.append(repo_root / "tools" / "codegen" / "boards" / f"{board_name}.json")

    for path in paths:
        if path.exists():
            try:
                with path.open("r", encoding="utf-8") as fp:
                    return json.load(fp)
            except json.JSONDecodeError as e:
                _die(f"board config '{path}' is invalid JSON: {e}")
            except Exception as e:
                _die(f"failed to read board config '{path}': {e}")

    checked_str = "\n  - ".join(str(p) for p in paths)
    _die(f"board config '{board_name}' not found. Checked:\n  - {checked_str}")
    raise RuntimeError("unreachable")


ROLE_VERBS = {
    "binary_sensor": ["is_active", "is_active_status", "was_active", "was_active_status", "start_auto_poll", "stop_auto_poll"],
    "binary_indicator": ["activate", "deactivate", "toggle"],
    "distance_sensor": ["request_measurement", "read_distance", "read_distance_status"],
    "text_display": ["clear", "draw_text", "flush"],
}


def build_context(cfg: dict, config_source: str) -> dict:
    validate_top_level(cfg, config_source)

    board_name = cfg.get("board")
    board_cfg = {}
    if board_name:
        board_cfg = _load_board_config(board_name, config_source)

    devices_raw: Dict[str, dict] = cfg["devices"]
    resolved_devices: Dict[str, dict] = {}

    # 1. Resolve use_onboard inheritance
    for name, spec in devices_raw.items():
        if not isinstance(spec, dict):
            _die(f"{config_source}: device '{name}' must be an object")

        use_onboard = spec.get("use_onboard")
        if use_onboard:
            if not board_cfg:
                _die(f"{config_source}: device '{name}' uses 'use_onboard' but no 'board' is defined")
            onboard_devices = board_cfg.get("onboard_devices", {})
            if use_onboard not in onboard_devices:
                _die(f"{config_source}: device '{name}' uses unknown onboard device '{use_onboard}' on board '{board_name}'")
            
            # Deep copy to avoid mutating original board_cfg
            merged = json.loads(json.dumps(onboard_devices[use_onboard]))
            decl_type = spec.get("type")
            if decl_type is not None:
                if decl_type != merged.get("type"):
                    _die(f"{config_source}: device '{name}' type '{decl_type}' conflicts with onboard device '{use_onboard}' type '{merged.get('type')}'")
            for k, v in spec.items():
                if k != "use_onboard":
                    merged[k] = v
            resolved_devices[name] = merged
        else:
            resolved_devices[name] = json.loads(json.dumps(spec))

    # 2. Resolve "$board." variables recursively, with support for "$$board." escaping
    def resolve_val(val, path_context: str):
        if isinstance(val, str):
            if val.startswith("$$board."):
                return val[1:]  # Escaped: strip first '$' and return literal "$board.xxxx"
            elif val.startswith("$board."):
                parts = val[1:].split(".")  # Strip '$' and split: ["board", "headers", "D18"]
                curr = board_cfg
                for part in parts[1:]:
                    if not isinstance(curr, dict) or part not in curr:
                        _die(f"{config_source}: unable to resolve reference '{val}' in '{path_context}'")
                    curr = curr[part]
                return curr
        elif isinstance(val, list):
            return [resolve_val(item, path_context) for item in val]
        elif isinstance(val, dict):
            return {k: resolve_val(v, f"{path_context}.{k}") for k, v in val.items()}
        return val

    for name, spec in resolved_devices.items():
        resolved_devices[name] = resolve_val(spec, f"devices.{name}")

    # 3. Pin conflict validation
    pin_occupancy: Dict[int, List[str]] = {}
    for name, spec in resolved_devices.items():
        for k, v in spec.items():
            if (k == "pin" or k.endswith("_pin")) and isinstance(v, int):
                pin_occupancy.setdefault(v, []).append(f"{name}.{k}")

    conflicts = []
    for pin, users in pin_occupancy.items():
        if len(users) > 1:
            conflicts.append(f"GPIO {pin} is shared by: {', '.join(users)}")

    if conflicts:
        _die(f"Pin conflict(s) detected:\n  - " + "\n  - ".join(conflicts))

    # 4. Resolve drivers and validate fields/roles
    resolved: List[Tuple[str, DriverBase, dict]] = []
    for name, spec in resolved_devices.items():
        driver = _resolve_driver(name, spec, config_source)
        _validate_required_fields(name, spec, driver, config_source)
        role = spec.get("role")
        if role is not None:
            if not isinstance(role, str) or not role:
                _die(f"{config_source}: device '{name}' role must be a non-empty string")
            if role not in getattr(driver, "role_verbs", {}):
                supported = sorted(list(getattr(driver, "role_verbs", {}).keys()))
                _die(f"{config_source}: device '{name}' has unsupported role '{role}' for driver '{driver.type}' (supported: {supported})")
        resolved.append((name, driver, spec))

    # Count instances of each helper-managed type, defaulting to 0 for known helper types
    instance_counts = {"led": 0, "button": 0, "ultrasonic": 0, "servo": 0}
    # Collect all config macros
    config_macros = []
    for name, driver, spec in resolved:
        t = driver.type
        if t in instance_counts:
            instance_counts[t] += 1
        if hasattr(driver, "render_config_macros"):
            config_macros.extend(driver.render_config_macros(name, spec))

    # 5. I2C shared bus detection
    i2c_buses = {}
    for name, driver, spec in resolved:
        if "i2c_port" in spec:
            port = int(spec["i2c_port"])
            sda = spec.get("sda_pin")
            scl = spec.get("scl_pin")
            hz = spec.get("hz") or spec.get("i2c_hz") or 100000  # Default 100kHz
            if sda is None or scl is None:
                # Fallback to board definition
                board_buses = board_cfg.get("buses", {})
                bus_key = f"i2c{port}"
                if bus_key in board_buses:
                    if sda is None:
                        sda = board_buses[bus_key].get("sda")
                    if scl is None:
                        scl = board_buses[bus_key].get("scl")
            if sda is None or scl is None:
                _die(f"device '{name}' is on I2C port {port} but SDA/SCL pins cannot be resolved")
            
            # Resolve to pins (ensure they are integers/resolved variables)
            if port in i2c_buses:
                existing = i2c_buses[port]
                if existing["sda"] != sda or existing["scl"] != scl:
                    _die(f"I2C port {port} configuration conflict: device '{name}' uses SDA={sda}, SCL={scl} while another device uses SDA={existing['sda']}, SCL={existing['scl']}")
                existing["devices"].append(name)
            else:
                i2c_buses[port] = {
                    "port": port,
                    "sda": sda,
                    "scl": scl,
                    "hz": hz,
                    "devices": [name]
                }
    buses_ctx = list(i2c_buses.values())

    ordered = topo_sort(resolved_devices, config_source)
    lookup = {n: (d, s) for n, d, s in resolved}
    devices_ctx = []
    role_headers_all = []
    role_wrappers_all = []

    for name in ordered:
        driver, spec = lookup[name]
        role = spec.get("role") or getattr(driver, "default_role", "")
        if role:
            if role in getattr(driver, "role_verbs", {}):
                role_headers_all.extend(driver.get_role_headers(role))
                for verb in driver.role_verbs[role]:
                    code = driver.render_role_wrapper(name, role, verb, spec)
                    if code:
                        role_wrappers_all.append(code)

        devices_ctx.append({
            "name": name,
            "type": driver.type,
            "role": role,
            "is_actuator": driver.is_actuator,
            "headers": driver.get_headers(),
            "device_type": driver.get_device_type(),
            "safe_off_fn": driver.get_safe_off_fn(),
            "config_init": driver.render_config_init(name, spec),
            "post_init_calls": driver.render_post_init_calls(name, spec),
            "deinit": driver.render_deinit(name),
        })

    # Deduplicate headers preserving order.
    def _dedup(seq: List[str]) -> List[str]:
        seen_: Dict[str, None] = {}
        for x in seq:
            seen_.setdefault(x, None)
        return list(seen_)

    driver_headers = _dedup(h for d in devices_ctx for h in d["headers"])
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
        "role_headers": _dedup(role_headers_all),
        "role_wrappers": role_wrappers_all,
        "config_macros": config_macros,
        "instance_counts": instance_counts,
        "buses": buses_ctx,
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
        try:
            md_gen_path = args.out_dir / "device_tree_api.md"
            if md_gen_path.exists():
                src_doc_dir = args.config.resolve().parent / "docs"
                src_doc_dir.mkdir(parents=True, exist_ok=True)
                src_doc_file = src_doc_dir / "device_tree_api.md"
                src_doc_file.write_text(md_gen_path.read_text(encoding="utf-8"), encoding="utf-8", newline="\n")
        except Exception as e:
            print(f"[codegen] warning: failed to write source tree documentation: {e}", file=sys.stderr)
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
