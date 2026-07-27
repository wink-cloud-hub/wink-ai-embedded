"""tools.cli.commands.new_dal — Scaffold a new DAL driver + codegen plugin (ADR-0046)."""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Optional

from tools.cli.base import CommandBase
from tools.cli.context import AppContext

_TYPE_RE = re.compile(r"^[a-z][a-z0-9_]*$")
_CATEGORIES = (
    "input",
    "output",
    "actuator",
    "sensor",
    "display",
    "communication",
    "storage",
)


def _templates_dir() -> Path:
    return Path(__file__).resolve().parent.parent / "templates" / "dal"


def _render(template_name: str, **ctx: object) -> str:
    try:
        from jinja2 import Environment, FileSystemLoader, StrictUndefined
    except ImportError:
        print(
            "[wink] Error: jinja2 is required for new-dal "
            "(pip install jinja2).",
            file=sys.stderr,
        )
        raise SystemExit(2)
    env = Environment(
        loader=FileSystemLoader(str(_templates_dir())),
        undefined=StrictUndefined,
        keep_trailing_newline=True,
    )
    return env.get_template(template_name).render(**ctx)


class NewDalCommand(CommandBase):
    name = "new-dal"
    help = "Scaffold DAL .h/.c + codegen driver plugin (ADR-0046)"

    def register_args(self, parser: argparse.ArgumentParser) -> None:
        parser.add_argument(
            "type",
            help="Device type string (snake_case; must match wink-app.json / unisim)",
        )
        parser.add_argument(
            "--category",
            required=True,
            choices=_CATEGORIES,
            help="DAL category directory under dal/include|src/",
        )
        parser.add_argument(
            "--actuator",
            action="store_true",
            help="Mark plugin is_actuator=True and stub off/safe_off APIs",
        )
        parser.add_argument(
            "--role",
            default="",
            help="Optional default_role name for Role wrappers",
        )
        parser.add_argument(
            "--pin-field",
            action="append",
            default=[],
            dest="pin_fields",
            help="Config pin field name (repeatable; default: gpio_pin)",
        )
        parser.add_argument(
            "--force",
            action="store_true",
            help="Overwrite existing scaffold files",
        )

    def run(self, ctx: AppContext, args: argparse.Namespace) -> Optional[int]:
        type_ = args.type.strip()
        if not _TYPE_RE.match(type_):
            print(
                f"[wink] Error: type must be snake_case starting with a letter "
                f"(got {type_!r})",
                file=sys.stderr,
            )
            return 1

        category = args.category
        pin_fields = args.pin_fields or ["gpio_pin"]
        type_upper = type_.upper()
        type_title = type_.replace("_", " ").title().replace(" ", "")

        hdr = (
            ctx.sdk_root
            / "dal"
            / "include"
            / category
            / f"dal_{type_}.h"
        )
        src = ctx.sdk_root / "dal" / "src" / category / f"dal_{type_}.c"
        plugin = (
            ctx.sdk_root
            / "tools"
            / "codegen"
            / "drivers"
            / f"{type_}.py"
        )

        targets = [hdr, src, plugin]
        existing = [p for p in targets if p.exists()]
        if existing and not args.force:
            print(
                "[wink] Error: target file(s) already exist "
                "(use --force to overwrite):",
                file=sys.stderr,
            )
            for p in existing:
                print(f"  {p}", file=sys.stderr)
            return 1

        # Reject clash with registered types unless --force (regen).
        try:
            from tools.codegen.drivers import known_types

            if type_ in known_types() and not args.force:
                print(
                    f"[wink] Error: type {type_!r} already registered in "
                    f"codegen drivers (use --force to overwrite files).",
                    file=sys.stderr,
                )
                return 1
        except Exception:
            pass

        ctx_vars = {
            "type": type_,
            "type_upper": type_upper,
            "type_title": type_title,
            "category": category,
            "category_enum": category.upper(),
            "is_actuator": bool(args.actuator),
            "role": args.role or "",
            "pin_fields": pin_fields,
            "guard": f"DAL_{type_upper}_H",
        }

        for path, tmpl in (
            (hdr, "dal_peripheral.h.j2"),
            (src, "dal_peripheral.c.j2"),
            (plugin, "driver_plugin.py.j2"),
        ):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(_render(tmpl, **ctx_vars), encoding="utf-8")
            print(f"[wink] wrote {path.relative_to(ctx.sdk_root)}")

        print(
            "\n[wink] Next steps:\n"
            f"  1. Fill DAL API/impl in dal/include|src/{category}/dal_{type_}.*\n"
            f"  2. Fill tools/codegen/drivers/{type_}.py render_* hooks\n"
            "  3. python wink-micro-os/tools/wink.py lint "
            "--pack drivers --pack layering --pack api\n"
            "  4. Host build (JSON prune + once without JSON)\n"
            "  5. Align unisim Manifest type string\n"
            "Re-configure CMake so the new driver enters WINK_KNOWN_DRIVERS."
        )
        return 0
